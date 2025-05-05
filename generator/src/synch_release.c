/** @file synch_release.c
 * @ingroup generator
 * @brief Functions for having multiple benchmark instances start at the same
 * time.
 * @author Mattia Nicolella
 *
 * @copyright (C) 2021 - 2024, Mattia Nicolella <mnico@bu.edu> and the rt-bench
 * contributors. SPDX-License-Identifier: MIT
 */

#include "synch_release.h"
#include "logging.h"
#include "signal_utils.h"
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#ifndef SYNCH_DELAY_REL_SEC
/// Initial delay for the synchronized benchmark start in seconds
#define SYNCH_DELAY_REL_SEC 0
#endif

#ifndef SYNCH_DELAY_REL_NSEC
/// Initial delay for the synchronized benchmark start in nanoseconds (default
/// 1msecs)
#define SYNCH_DELAY_REL_NSEC 1000 * 1000
#endif

/// Struct that contains the parameters for the benchmark synchronised start.
static struct synch_params_t {
  char *shm_name,  ///< The name of the shared memory.
      *sem_name;   ///< The name of the named semaphore.
  int fd;          ///< The file descriptor of the shared memory.
  sem_t *init_sem; ///< The semaphore used for shared shm init.
  struct synch_shm
      *shm; ///< The shared memory used to synchornise the benchmark group
} synch_params = {0};

enum synch_status get_synch_status() {
  if (synch_params.shm != NULL) {
    return synch_params.shm->status;
  } else {
    return SYNCH_DISABLED;
  }
}

struct timespec get_synch_delay() {
  struct timespec delay = {0};
  if (get_synch_status() == SYNCH_ENABLED) {
    delay = synch_params.shm->timer_initial_delay;
  }
  return delay;
}

/** @brief Signal handler for synchnised start
 * @param[in] signo Signal number (unused).
 * @param[in] info Why the signal was generated (unused).
 * @param[in] context interrupted thread context (unused).
 * @details
 * The process that receives `::SYNCH_RELEASE_SIGNAL` will try to elevete itself
 * to be the unblocker. It will check if the group size and the number of
 * benchmarks waiting for synchornisation matches and then proceed with the
 * unblocking.
 *
 * To unblock all benchmarks and keep them synchonised it will calculate an
 * absolute timestamp that will be used to initialise the deadline timer for all
 * synchronised benchmarks and unblock all benchmarks by posting on the
 * shared unnamed sempahore once for each benchmark recorded in
 * `waiting_bmarks`. It will then flip `unblocked` to `true` to signal that the
 * benchmarks are started, so that "late" benchmarks can gracefully exit when
 * they read this variable.
 *
 * To avoid race conditions only one process per group (the one that can
 * succesfully perform a compare and swap) will be able to unblock the others,
 * all other process will simply ignore the signal.
 */
void synch_on_start_handler(int signo, siginfo_t *info, void *context) {
  int i, res;
  pid_t pid_val = getpid(), *pid_ptr = &(synch_params.shm->unblocker_pid),
        pid_expected = 0;
  bool atomic_res, unblock_val = true;
  // make sure there is only one unblocker!
  // https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html
  atomic_res =
      __atomic_compare_exchange(pid_ptr, &pid_expected, &pid_val, false,
                                __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
  elogf(LOG_LEVEL_DEBUG, "we are %d and %d is the unblocker\n", pid_val,
        synch_params.shm->unblocker_pid);
  if (atomic_res) {
    if (get_synch_status() != SYNCH_ENABLED) {
      elogf(LOG_LEVEL_ERR,
            "Trying to unblock benchmarks with invalid synch status!\n");
      return;
    }
    if (synch_params.shm->waiting_bmarks < synch_params.shm->group_size) {
      elogf(LOG_LEVEL_ERR,
            "The group size is %d, but we only have %d benchmarks waiting for "
            "synchonisation!\n",
            synch_params.shm->group_size, synch_params.shm->waiting_bmarks);
      return;
    }
    __atomic_store(&(synch_params.shm->unblocked), &unblock_val,
                   __ATOMIC_SEQ_CST);
    if (synch_params.shm == NULL) {
      elogf(LOG_LEVEL_ERR,
            "Error while starting synchonised benchmarks, missing shm\n");
      res = -1;
    } else {
      // compute delay for benchmarks
      res = clock_gettime(CLOCK_REALTIME,
                          &(synch_params.shm->timer_initial_delay));
      if (res < 0) {
        perror(
            "Error while starting synchonised benchmarks, cannot get current "
            "time");
      } else {
        synch_params.shm->timer_initial_delay.tv_sec += SYNCH_DELAY_REL_SEC;
        synch_params.shm->timer_initial_delay.tv_nsec += SYNCH_DELAY_REL_NSEC;
        elogf(LOG_LEVEL_DEBUG, "set initial delay to %d sec and %d nsec\n",
              SYNCH_DELAY_REL_SEC, SYNCH_DELAY_REL_NSEC);
        for (i = 0; i < synch_params.shm->waiting_bmarks; i++) {
          res = sem_post(&(synch_params.shm->synch_sem));
          if (res < 0) {
            synch_params.shm->timer_initial_delay.tv_sec = 0;
            synch_params.shm->timer_initial_delay.tv_nsec = 0;
            perror(
                "Error while starting synchonised benchmarks, cannot post on "
                "semaphore");
          }
        }
      }
    }
    if (res < 0) {
      elogf(
          LOG_LEVEL_ERR,
          "\n\nWARNING:All benchmarks currently using the '-s %s' will have an "
          "invalid state"
          "forever, the user has to manually kill them\n\n",
          synch_params.shm_name);
    }
  } else {
    elogf(LOG_LEVEL_DEBUG, "We are %d and there is another unblocker, %d\n",
          pid_val, pid_expected);
  }
}

/**
 * @details This function will create a shared memory and a semaphore to
 * synchronise the start of multiple benchmarks. The process that creates the
 * shm will be considered the 'master' process and be responsible for setting
 * its size and initialising a unnamed semaphore used for synchronisation. All
 * benchmarks will then increase the `waiting_bmarks` variable in
 * `::synch_params_t` shared memory and register a `::SIGNAL_SYNCH_RELEASE`
 * signal handler. However the `::SIGNAL_SYNCH_RELEASE` will be blocked, to
 * allow `benchmark_init` to complete before the synchonised start.
 */
int init_synchronised_benchmark_group(const char *group_name) {
  int res, master = 0;
  sigset_t synch_sigset;

  elogf(LOG_LEVEL_TRACE, "Ignoring synch release signals since bmark is not "
                         "ready to be synched\n");
  sigemptyset(&synch_sigset);
  sigaddset(&synch_sigset, SIGNAL_SYNCH_RELEASE);
  res = sigprocmask(SIG_BLOCK, &synch_sigset, NULL);
  if (res < 0) {
    perror("Error during synch release signals blocking");
    return res;
  }

  synch_params.shm_name =
      malloc(sizeof(char) * (strlen("/rtbench.shm_") + strlen(group_name) + 1));
  if (synch_params.shm_name == NULL) {
    perror("Error during shm name allocation");
    return -1;
  }
  synch_params.sem_name =
      malloc(sizeof(char) * (strlen("/rtbench.sem_") + strlen(group_name) + 1));
  if (synch_params.sem_name == NULL) {
    perror("Error during semaphore name allocation");
    return -1;
  }

  res = sprintf(synch_params.shm_name, "/rtbench.shm_%s", group_name);
  if (res < 0) {
    perror("Errod during creation of synchronisation shm name");
    return res;
  }
  res = sprintf(synch_params.sem_name, "/rtbench.sem_%s", group_name);
  if (res < 0) {
    perror("Errod during creation of synchronisation semaphore name");
    return res;
  }
  synch_params.init_sem = sem_open(synch_params.sem_name, O_CREAT, 0644, 1);
  if (synch_params.init_sem == SEM_FAILED) {
    perror("Error during semaphore initialization for synchronised start");
    return -1;
  }
  elogf(LOG_LEVEL_DEBUG, "Created names semaphore %s\n", synch_params.sem_name);
  synch_params.fd =
      shm_open(synch_params.shm_name, O_CREAT | O_RDWR | O_EXCL, 0640);
  if (synch_params.fd < 0) {
    if (errno != EEXIST) {
      perror("Error during exlusive open of shared memory for synchronised "
             "start");
      deallocate_synch_resources();
      return synch_params.fd;
    }
    master = 0;
    // reopen the shm in non-exclusive mode ifthe shm is already there
    synch_params.fd = shm_open(synch_params.shm_name, O_CREAT | O_RDWR, 0640);
    if (synch_params.fd < 0) {
      perror("Error during shared memory open for synchronised start");
      deallocate_synch_resources();
      return synch_params.fd;
    }
    elogf(LOG_LEVEL_DEBUG, "Opened shared memory as non master\n");
  } else {
    master = 1;
    elogf(LOG_LEVEL_DEBUG, "Opened shared memory as master\n");
  }
  // if we are the master, we initialize the shared memory
  if (master) {
    do {
      res = ftruncate(synch_params.fd, sizeof(struct synch_params_t));
      if (res < 0 && errno != EINTR) {
        perror("Error during shared memory size setup for synchronised start");
        deallocate_synch_resources();
        return res;
      }
    } while (res < 0 && errno == EINTR);
  }
  synch_params.shm =
      mmap(NULL, sizeof(struct synch_shm), PROT_READ | PROT_WRITE, MAP_SHARED,
           synch_params.fd, 0);
  if (synch_params.shm == MAP_FAILED) {
    perror("Error during shared memory mapping for synchronised start");
    deallocate_synch_resources();
    return -1;
  }
  res = sem_wait(synch_params.init_sem);
  if (res < 0) {
    perror("Cannot wait on synch init semaphore");
    return res;
  }
  if (master) {
    elogf(LOG_LEVEL_DEBUG, "Init shared memory to 0\n");
    memset(synch_params.shm, 0, sizeof(struct synch_shm));
    res = sem_init(&(synch_params.shm->synch_sem), 1, 0);
    if (res < 0) {
      perror("Cannot init synch semaphore");
      return res;
    }
    // set ourselves as the master pid
    synch_params.shm->master_pid = getpid();
    // tell everyone shm setup is ok!
    synch_params.shm->status = SYNCH_ENABLED;
  }
  synch_params.shm->waiting_bmarks++;
  res = sem_post(synch_params.init_sem);
  if (res < 0) {
    perror("Cannot post on synch init semaphore");
    return res;
  }
  elogf(LOG_LEVEL_DEBUG, "mmapped shared memory\n");
  int blocked_signals[1] = {SIGNAL_SYNCH_RELEASE};
  res = setup_signal(SIGNAL_SYNCH_RELEASE, synch_on_start_handler,
                     blocked_signals, 1);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Error during handler setup for synchonised start\n");
    deallocate_synch_resources();
    return res;
  }
  return 0;
}

/** @details
 * Check if a benchmark is "late" (i.e. when the group is already unblocked and
 * the current benchmark did wait on the synchonisation semaphore yet). Then
 * unlblock `::SIGNAL_SYNCH_RELEASE` and wait on the synchornisation semaphore.
 * After begin unlocked, ignore additional `::SIGNAL_SYNCH_RELEASE`. If
 * synchornisation features are not enabled, do nothing and return.
 */
int wait_for_synch() {
  int res = 0;
  bool unblock_val;
  sigset_t synch_sigset;
  if (synch_params.shm->status != SYNCH_ENABLED) {
    return res;
  }
  __atomic_load(&(synch_params.shm->unblocked), &unblock_val, __ATOMIC_SEQ_CST);
  if (unblock_val == true) {
    elogf(LOG_LEVEL_ERR,
          "Waiting on an already started set of benchmarks, aborting.\n");
    return -EXIT_FAILURE;
  }
  elogf(LOG_LEVEL_TRACE, "Unblocking synch release signal\n");
  sigemptyset(&synch_sigset);
  sigaddset(&synch_sigset, SIGNAL_SYNCH_RELEASE);
  res = sigprocmask(SIG_UNBLOCK, &synch_sigset, NULL);
  if (res < 0) {
    perror("Error during synch release signal unblocking");
    return res;
  }
  do {
    elogf(LOG_LEVEL_TRACE, "Waiting for synchronised start\n");
    res = sem_wait(&(synch_params.shm->synch_sem));
    if (res < 0 && errno != EINTR) {
      perror("Error during semaphore wait for synchronised start");
      deallocate_synch_resources();
      return res;
    }
  } while (res < 0 && errno == EINTR);
  elogf(LOG_LEVEL_TRACE, "Ignoring additional synch release signals\n");
  sigemptyset(&synch_sigset);
  sigaddset(&synch_sigset, SIGNAL_SYNCH_RELEASE);
  res = sigprocmask(SIG_BLOCK, &synch_sigset, NULL);
  if (res < 0) {
    perror("Error during synch release signals blocking");
    return res;
  }
  elogf(LOG_LEVEL_TRACE, "benchmarks in sync.\n");
  return res;
}

void deallocate_synch_resources() {
  int res;
  pid_t own_pid = getpid(), master_pid = 0;
  if (synch_params.shm != NULL) {
    master_pid = synch_params.shm->master_pid;
  }
  if (synch_params.sem_name != NULL) {
    if (synch_params.init_sem != NULL) {
      res = sem_close(synch_params.init_sem);
      if (res < 0) {
        perror("Error during synchronisation semaphore deallocation");
      }
    }
    if (master_pid == own_pid) {
      res = sem_unlink(synch_params.sem_name);
      if (res < 0) {
        perror("Error during synchronisation semaphore unlinking");
      }
    }
    free(synch_params.sem_name);
  }
  if (synch_params.shm != NULL) {
    if (master_pid == own_pid) {
      res = sem_destroy(&(synch_params.shm->synch_sem));
      if (res < 0) {
        perror("Error during synch semaphore destruction");
      }
    }
    res = munmap(synch_params.shm, sizeof(struct synch_shm));
    if (res < 0) {
      perror("Error during synchronisation shared memory unmapping");
    }
  }
  if (synch_params.fd >= 0) {
    res = close(synch_params.fd);
    if (res < 0) {
      perror("Error during shared memory file descriptor deallocation");
    }
  }
  if (synch_params.shm_name != NULL) {
    if (master_pid == own_pid) {
      res = shm_unlink(synch_params.shm_name);
      if (res < 0) {
        perror("Error during shared memory unlinking");
      }
    }
    free(synch_params.shm_name);
  }
}
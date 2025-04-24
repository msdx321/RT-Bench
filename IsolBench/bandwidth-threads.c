/**
 * @file bandwidth-threads.c
 * @ingroup bandwidth
 * @brief multithread version of the bandwidth benchmark.
 * @author Heechul Yun <heechul@illinois.edu>, Zheng <zpwu@uwaterloo.ca>, Mattia
 * Nicolella <mnico@bu.edu>
 * @copyright (C) 2012 Original file is distributed under the University of
 * Illinois Open Source License. This file follows the general license of
 * RT-Bench. See LICENSE.TXT for details.
 * @details
 * Benchmark has been broken down in three components:
 * - init: benchmark_init();
 * - execution: benchmark_execution();
 * - teardown: benchmark_teardown();
 *
 * This allows the benchmark to be run periodically, by re-running only the
 * execution portion.
 *
 */

/**************************************************************************
 * Conditional Compilation Options
 **************************************************************************/

/**************************************************************************
 * Included Files
 **************************************************************************/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* See feature_test_macros(7) */
#endif
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

// Libraries used by rt-bench
#include "logging.h"
#include "optional_features.h"
#include "periodic_benchmark.h"

/**************************************************************************
 * Public Definitions
 **************************************************************************/
#define CACHE_LINE_SIZE 64 /** cache Line size is 64 byte */
#ifdef __arm__
#define DEFAULT_ALLOC_SIZE_KB 4096
#else
#define DEFAULT_ALLOC_SIZE_KB 16384
#endif
#define DEFAULT_ITERATIONS 5
#define DEFAULT_ACC_TYPE READ

/**************************************************************************
 * Public Types
 **************************************************************************/
/// Type of memory access to perform.
enum access_type { READ, WRITE };

/// Type of memory access to perform.
enum buffer_type { SHARED, PRIVATE };

/// Thread status enum, so threads do not run forever
enum thread_execution_status { THREAD_DISABLED = 0, THREAD_ENABLED };

/**************************************************************************
 * Global Variables
 **************************************************************************/
int g_mem_size = 0;            /** Global memory size */
int *g_mem_ptr = NULL;         /** Pointer to allocated memory region */
volatile unsigned int g_start; ///< Task start time.
volatile unsigned int g_end;   ///< Task end time.
volatile uint64_t g_nread = 0; //< Total number of bytes read
/// Buffer type
enum buffer_type buf_type = PRIVATE;

struct thread_status {
  int id,         ///< The thread id.
      cpu_id,     ///< CPU where the worker thread is pinned.
      *t_mem_ptr, ///< Pointer to thre portion of the buffer the thread has to
                  ///< access
      t_mem_size, ///< Thread-local memory size.
      iterations; ///< Number of iterations
  enum access_type acc_type; ///< Memory access type
  volatile enum thread_execution_status
      execution_status;          ///< If the current thread is enabled or not.
  volatile uint64_t nread;       ///< Number of bytes read per thread
  int64_t sum;                   ///< sum of the amount of read/written memory.
  volatile unsigned int t_start; ///< Task start time.
  volatile unsigned int t_end;   ///< Task end time.
} *thread_stat = NULL;

/// Array of pthread ids
pthread_t *pthread_id = NULL;

/// How many threads we are using
int thread_num = 0;

sem_t worker_mutex,     ///< A mutex to have all worker threads update a
                        ///< `num_threads_ready` without race conditions.
    worker_ready_queue, ///< The queue of worker threads that will need to be
                        ///< unlocked by the main thread.
    worker_complete_queue, ///< The queue of worker threads that will need to be
                           ///< unlocked by the main thread.
    main_start_mutex, ///< The mutex used by the main thread to wait for all the
                      ///< worker threads to be ready to start.
    main_end_mutex;   ///< The mutex used by the main thread to wait for all the
                      ///< worker threads to have finished the loop.
int num_threads_ready =
        0, ///< The number of threads ready to start a new computation loop.
    num_threads_completed =
        0; ///< The number of threads that have completed a computation loop.

/**************************************************************************
 * Public Functions
 **************************************************************************/
/** @brief Get timestamp in microseconds.
 * @returns Timestamp in microseconds.
 * */
unsigned int get_usecs() {
  struct timeval time;
  gettimeofday(&time, NULL);
  return (time.tv_sec * 1000000 + time.tv_usec);
}

/** @brief Print bandwidth stats.
 * @param[in] param Unused.
 * @details Function unused.
 * */
void print_bandwidth(int param) {
  float dur_in_sec;
  float bw;
  float dur = get_usecs() - g_start;
  dur_in_sec = (float)dur / 1000000.0f;
  flogf(LOG_LEVEL_FILE, log_filep, "g_nread(bytes read) = %lld\n",
        (long long)g_nread);
  flogf(LOG_LEVEL_FILE, log_filep, "elapsed = %.2f sec ( %.0f usec )\n",
        dur_in_sec, dur);
  bw = (float)g_nread / dur_in_sec / 1024.0f / 1024.0f;
  flogf(LOG_LEVEL_FILE, log_filep, "B/W = %.2f MB/s | ", bw);
  flogf(LOG_LEVEL_FILE, log_filep, "average = %.2f ns\n\n",
        (dur * 1000) / ((float)g_nread / CACHE_LINE_SIZE));
}

/** @brief Read memory access.
 * @param[in] mem_ptr location in memory to access
 * @param[in] mem_size size of memory to access
 * @param[out] nread total number of bytes read
 * @returns Amount of memory read.
 */
static inline int64_t bench_read(int *mem_ptr, int mem_size,
                                 volatile uint64_t *nread) {
  int i;
  int64_t sum = 0;
  for (i = 0; i < mem_size / 4; i += (CACHE_LINE_SIZE / 4)) {
    sum += mem_ptr[i];
  }
  *nread += mem_size;
  return sum;
}

/** @brief Write memory access.
 * @param[in] mem_ptr location in memory to access
 * @param[in] mem_size size of memory to access
 * @param[out] nread total number of bytes written
 * @returns Amount of memory read.
 */
static inline int bench_write(int *mem_ptr, int mem_size,
                              volatile uint64_t *nread) {
  register int i;
  for (i = 0; i < mem_size / 4; i += (CACHE_LINE_SIZE / 4)) {
    mem_ptr[i] = i;
  }
  *nread += mem_size;
  return 1;
}

/** @brief Print usage info.
 * @param[in] argc arguments number.
 * @param[in] argv Arguments array.
 * */
void usage(int argc, char *argv[]) {
  printf("Multithread memory bomb.\n");
  printf("Usage: $ %s [<option>]*\n\n", argv[0]);
  printf("-m: memory size in KB. deafult=16384\n");
  printf("-a: access type - read, write. default=read\n");
  printf("-i: iterations. default=5\n");
  printf("-t: thread number. default=1 per core in process affinity mask.\n");
  printf("-b: buffer type - shared: all threads access same buffer from the "
         "beginning; "
         "private: each thread accesses different buffers. default=private\n");
  printf("-h: help\n");
  printf("For -m, -a and -i it is possible to specify the parameter on a per "
         "thread basis, separating the values with a comma to have threads "
         "that do different access types.\n");
  printf(
      "\nExamples: \n$ bandwidth -m 8192 -a read -i 1 -b shared  <- 8MB read "
      ",1 one iteration, shared buffer.\n");
  printf("$ bandwidth -t 2 -m 8192,4096 -a read,write -i 1,5 -b shared <- "
         "thread 1 does 8MB read "
         ", one iteration, thread 2 does 4MB write , 5 iterations. Both threas "
         "share the same buffer.\n");
  exit(1);
}

/** @brief Wait for a semaphore, looping if the wait gets interrupted by a
 * signal.
 * @param[in] sem The saemaphore to use in `sem_wait`.
 * @returns the value returned by the `sem_wait` function.
 */
static inline int wait_for_sem(sem_t *sem) {
  int res = 0;
  do {
    res = sem_wait(sem);
  } while (errno == EINTR);
  return res;
}

/** @brief The logic all worker threads will execute.
 * @param[in] arg The `struct thread_stat` that holds the local thread status.
 * @details
 * Every thread will perform a renevous with the others then when every thread
 * is ready the last one will wake up the main thread, which will in turn unlock
 * all worker threads.
 *
 * Then all worker threads will perform a memory access as
 * directed by they local instance of the `struct thread_stat`.
 *
 * At the end of
 * the job, every worker thread will perfrom another rendevous and when every
 * worker is finished, the last worker will wake up main again, then the
 * execution loop will restart.
 *
 * If, after the thread is woken up, the thread was
 * disabled then it will break the execution loop and return.
 * @returns NULL
 */
void *thread_execution(void *arg) {
  if (arg == NULL) {
    elogf(LOG_LEVEL_ERR, "Worker thread has no argument\n");
    exit(-EXIT_FAILURE);
  }
  struct thread_status *local_status = (struct thread_status *)arg;
  int i = 0, res = 0;
  while (local_status->execution_status != THREAD_DISABLED) {
    elogf(LOG_LEVEL_DEBUG, "Worker thread %d starting compute rendevous\n",
          local_status->id);

    // thread_num + main thread rendevous to start computing
    res = wait_for_sem(&worker_mutex);
    if (res < 0) {
      elogf(LOG_LEVEL_ERR, "Thread %d cannot wait on worker mutex: %s\n",
            local_status->id, strerror(errno));
    }
    num_threads_ready++;
    elogf(LOG_LEVEL_DEBUG, "thread %d start sync ready %d complete %d\n",
          local_status->id, num_threads_ready, num_threads_completed);
    if (num_threads_ready == thread_num) {
      num_threads_ready = 0;
      res = sem_post(&main_start_mutex);
      if (res < 0) {
        elogf(LOG_LEVEL_ERR, "Thread %d cannot post on main mutex: %s\n",
              local_status->id, strerror(errno));
      }
    }
    res = sem_post(&worker_mutex);
    if (res < 0) {
      elogf(LOG_LEVEL_ERR, "Thread %d cannot post on worker mutex: %s\n",
            local_status->id, strerror(errno));
    }
    res = wait_for_sem(&worker_ready_queue);
    if (res < 0) {
      elogf(LOG_LEVEL_ERR, "Thread %d cannot wait on worker ready queue: %s\n",
            local_status->id, strerror(errno));
    }
    if (local_status->execution_status != THREAD_ENABLED) {
      break;
    }
    elogf(LOG_LEVEL_DEBUG, "Worker thread %d starting\n", local_status->id);
    /*
     * actual memory access
     */
    local_status->t_end = 0;
    local_status->nread = 0;
    local_status->t_start = get_usecs();
    for (i = 0;; i++) {
      switch (local_status->acc_type) {
      case READ:
        local_status->sum +=
            bench_read(local_status->t_mem_ptr, local_status->t_mem_size,
                       &(local_status->nread));
        break;
      case WRITE:
        local_status->sum +=
            bench_write(local_status->t_mem_ptr, local_status->t_mem_size,
                        &(local_status->nread));
        break;
      }

      if (local_status->iterations > 0 && i + 1 >= local_status->iterations)
        break;
    }
    local_status->t_end = get_usecs();
    elogf(LOG_LEVEL_DEBUG,
          "Worker thread %d waiting  for other threads to finish\n",
          local_status->id);
    // thread_num + main thread rendevous to start computing
    res = wait_for_sem(&worker_mutex);
    if (res < 0) {
      elogf(LOG_LEVEL_ERR, "Thread %d cannot wait on worker mutex: %s\n",
            local_status->id, strerror(errno));
    }
    num_threads_completed++;
    elogf(LOG_LEVEL_DEBUG, "thread %d end sync ready %d  completed %d\n",
          local_status->id, num_threads_ready, num_threads_completed);
    if (num_threads_completed == thread_num) {
      num_threads_completed = 0;
      res = sem_post(&main_end_mutex);
      if (res < 0) {
        elogf(LOG_LEVEL_ERR, "Thread %d cannot post on main mutex: %s\n",
              local_status->id, strerror(errno));
      }
    }
    res = sem_post(&worker_mutex);
    if (res < 0) {
      elogf(LOG_LEVEL_ERR, "Thread %d cannot post on worker mutex: %s\n",
            local_status->id, strerror(errno));
    }
    res = wait_for_sem(&worker_complete_queue);
    if (res < 0) {
      elogf(LOG_LEVEL_ERR,
            "Thread %d cannot wait on worker complete queue: %s\n",
            local_status->id, strerror(errno));
    }
  }
  elogf(LOG_LEVEL_DEBUG, "Worker thread %d terminating\n", local_status->id);
  return NULL;
}

/**
 * @brief Will interpret the benchmark parameters and initialize the testbed.
 * @param[in] parameters_num Number of parameters passed, should be 1.
 * @param[in] parameters The list of passed parameters.
 * @details
 * The required parameters array is documented in `usage()`, and can be brought
 * up by having "-h" in `parameters`.
 * @returns `0` on success, `-1` on error, setting errno.
 * @details This function will spawn and pin on thread per CPU, after setting up
 * some infrastructure to start and stop all threads each period and thieir
 * local arguments (iterations, access type and buffer)
 */
int benchmark_init(int parameters_num, void **parameters) {
  int opt, i, num_tokens = 0, res = 0;
  char *str_token = NULL, *mem_size_str = NULL, *iterations_str = NULL,
       *access_str = NULL;

  /*
   * get command line options
   */
  // adjust parameters list to have a dummy argument at position 0 (to fool
  // getopt)
  int opt_num = parameters_num + 1;
  char **opts = malloc(sizeof(char *) * opt_num);
  opts[0] = "bandwidth";
  memcpy(opts + 1, parameters, sizeof(char *) * parameters_num);
  while ((opt = getopt(opt_num, opts, "m:a:t:i:b:h")) != -1) {
    switch (opt) {
    case 'm': /* set memory size */
      mem_size_str = optarg;
      break;
    case 'a': /* set access type */
      access_str = optarg;
      break;
    case 'b': /* set buffer type */
      if (!strcmp(optarg, "shared"))
        buf_type = SHARED;
      else if (!strcmp(optarg, "private"))
        buf_type = PRIVATE;
      else {
        errno = EINVAL;
        free(opts);
        return -1;
      }
      break;
    case 'i': /* iterations */
      iterations_str = optarg;
      break;
    case 't':
      thread_num = strtol(optarg, NULL, 10);
      break;
    case 'h':
      usage(opt_num, opts);
      free(opts);
      return 0;
      break;
    }
  }
  elogf(LOG_LEVEL_DEBUG, "Parsed arguments\n");
  // get current affinity mask so we know how many threads we can use
  int cpus = get_nprocs(), max_threads = 0;
  cpu_set_t cpu_set_mask, thread_cpu_set_mask;
  res = sched_getaffinity(0, sizeof(cpu_set_t), &cpu_set_mask);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot get affinity mask: %s\n", strerror(errno));
    free(opts);
    return -EXIT_FAILURE;
  }
  max_threads = CPU_COUNT(&cpu_set_mask);
  elogf(LOG_LEVEL_DEBUG,
        "Got process affinity with %d cores ,user wants to use %d (0=all) "
        "cores\n",
        max_threads, thread_num);
  if (thread_num > max_threads) {
    elogf(LOG_LEVEL_ERR,
          "Using more than one thread per core is unsupported. User selected "
          "%d threads, but only %d core are in the process affinity mask\n",
          thread_num, max_threads);
    free(opts);
    return -EXIT_FAILURE;
  }
  if (thread_num == 0) {
    thread_num = max_threads;
  }
  elogf(LOG_LEVEL_DEBUG,
        "Allocated cpu masks and got maximum number of usable threads\n");
  elogf(LOG_LEVEL_TRACE, "Using %d worker threads\n", thread_num);

  res = sem_init(&worker_mutex, 0, 1);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot initialize worker mutex: %s\n",
          strerror(errno));
    free(opts);
    return -EXIT_FAILURE;
  }
  res = sem_init(&main_start_mutex, 0, 0);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot initialize main start mutex: %s\n",
          strerror(errno));
    free(opts);
    return -EXIT_FAILURE;
  }
  res = sem_init(&main_end_mutex, 0, 0);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot initialize main end mutex: %s\n",
          strerror(errno));
    free(opts);
    return -EXIT_FAILURE;
  }
  res = sem_init(&worker_ready_queue, 0, 0);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot initialize worker ready queue: %s\n",
          strerror(errno));
    free(opts);
    return -EXIT_FAILURE;
  }
  res = sem_init(&worker_complete_queue, 0, 0);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot initialize worker complete queue: %s\n",
          strerror(errno));
    free(opts);
    return -EXIT_FAILURE;
  }

  elogf(LOG_LEVEL_DEBUG, "Initialized pthreads semaphores\n");
  // allocate memory for all the extra metrics
  char *old_header;
  extra_measurement.num_elements = thread_num + 1;
  extra_measurement.data =
      malloc(sizeof(double) * extra_measurement.num_elements);
  if (extra_measurement.data == NULL) {
    elogf(LOG_LEVEL_ERR, "Cannot allocate memory for extra data\n");
    free(opts);
    return -EXIT_FAILURE;
  }
  elogf(LOG_LEVEL_DEBUG, "allocated data\n");
  memset(extra_measurement.data, 0,
         sizeof(double) * extra_measurement.num_elements);
  elogf(LOG_LEVEL_DEBUG, "initialized data\n");
  // create the extra measurements header
  res = asprintf(&extra_measurement.header, ",total bandwidth(MB/S)");
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot allocate memory for extra log header: %s\n",
          strerror(errno));
    free(opts);
    return -EXIT_FAILURE;
  }

  for (i = 0; i < thread_num; i++) {
    // add thread to log header
    old_header = extra_measurement.header;
    res = asprintf(&(extra_measurement.header), "%s,thread %d BW(MB/S)",
                   extra_measurement.header, i);
    if (res < 0) {
      elogf(LOG_LEVEL_ERR,
            "Cannot allocate memory for extra log header of thread %d: %s\n", i,
            strerror(errno));
      free(opts);
      free(old_header);
      return -EXIT_FAILURE;
    }
    free(old_header);
  }
  elogf(LOG_LEVEL_DEBUG, "Extra measurement header setup\n");

  // allocate memory for thread stats
  thread_stat =
      (struct thread_status *)malloc(sizeof(struct thread_status) * thread_num);
  if (thread_stat == NULL) {
    elogf(LOG_LEVEL_ERR, "Cannot allocate memory for thread stats\n");
    return -EXIT_FAILURE;
  }
  memset(thread_stat, 0, sizeof(struct thread_status) * thread_num);

  // fill up thread structure so that each thread can have independent
  // iteration, memory size and acce type values

  // fill up memory size
  num_tokens = 0;
  for (i = 0; i < thread_num; i++) {
    if (mem_size_str != NULL) {
      str_token = strtok((i == 0) ? mem_size_str : 0, ",");
      if (str_token != NULL) {
        num_tokens++;
        thread_stat[i].t_mem_size = strtol(str_token, NULL, 10) * 1024;
        if (thread_stat[i].t_mem_size <= 0) {
          elogf(LOG_LEVEL_ERR, "%d is an invalid memory amount for thread %d\n",
                thread_stat[i].t_mem_size, i);
          free(opts);
          return -EXIT_FAILURE;
        }
      } else {
        // we land here only int two cases
        if (num_tokens == 1) {
          // user specified only one size for all threads
          thread_stat[i].t_mem_size = thread_stat[0].t_mem_size;
        } else {
          // user specified memory sizes for a subset of the threads
          elogf(LOG_LEVEL_ERR,
                "Only %d memory sizes specified for %d threads!\n", thread_num,
                num_tokens);
          free(opts);
          return -EXIT_FAILURE;
        }
      }
    } else {
      // user specified nothing, use default value
      thread_stat[i].t_mem_size = DEFAULT_ALLOC_SIZE_KB * 1024;
    }
    // if we are using private buffers we need to increase the total memory size
    if (buf_type == PRIVATE) {
      g_mem_size += thread_stat[i].t_mem_size;
    } else {
      // with shared buffers we need to get the bigger buffer size the user has
      // specified and allcoate that.
      g_mem_size = (g_mem_size < thread_stat[i].t_mem_size)
                       ? thread_stat[i].t_mem_size
                       : g_mem_size;
    }
  }
  // fill up access type
  num_tokens = 0;
  for (i = 0; i < thread_num; i++) {
    if (access_str != NULL) {
      str_token = strtok((i == 0) ? access_str : 0, ",");
      if (str_token != NULL) {
        num_tokens++;
        if (!strcmp(str_token, "read"))
          thread_stat[i].acc_type = READ;
        else if (!strcmp(str_token, "write"))
          thread_stat[i].acc_type = WRITE;
        else {
          elogf(LOG_LEVEL_ERR, "%s is an invalid access type for thread %d\n",
                str_token, i);
          errno = EINVAL;
          free(opts);
          return -1;
        }
      } else {
        // we land here only int two cases
        if (num_tokens == 1) {
          // user specified only one access type for all threads
          thread_stat[i].acc_type = thread_stat[0].acc_type;
        } else {
          // user specified memory sizes for a subset of the threads
          elogf(LOG_LEVEL_ERR,
                "Only %d access types specified for %d threads!\n", thread_num,
                num_tokens);
          free(opts);
          return -EXIT_FAILURE;
        }
      }
    } else {
      // user specified nothing, use default value
      thread_stat[i].acc_type = DEFAULT_ACC_TYPE;
    }
  }
  // fill up iterations
  num_tokens = 0;
  for (i = 0; i < thread_num; i++) {
    if (iterations_str != NULL) {
      str_token = strtok((i == 0) ? iterations_str : 0, ",");
      if (str_token != NULL) {
        num_tokens++;
        thread_stat[i].iterations = strtol(str_token, NULL, 10);
        if (thread_stat[i].iterations <= 0) {
          elogf(LOG_LEVEL_ERR,
                "%d is an invalid iteration value for thread %d\n",
                thread_stat[i].t_mem_size, i);
          free(opts);
          return -EXIT_FAILURE;
        }
      } else {
        // we land here only int two cases
        if (num_tokens == 1) {
          // user specified only one size for all threads
          thread_stat[i].iterations = thread_stat[0].iterations;
        } else {
          // user specified memory sizes for a subset of the threads
          elogf(LOG_LEVEL_ERR,
                "Only %d iteration values specified for %d threads!\n",
                thread_num, num_tokens);
          free(opts);
          return -EXIT_FAILURE;
        }
      }
    } else {
      // user specified nothing, use default value
      thread_stat[i].iterations = DEFAULT_ITERATIONS;
    }
  }
  elogf(LOG_LEVEL_DEBUG, "Threads arguments setup done\n");

  /*
   * allocate contiguous region of memory, according to buffer type
   * If buffer type is private allocate a bigger buffer so that each thread can
   * access the user specified amount of memory
   */
  g_mem_ptr = (int *)malloc(g_mem_size);
  if (g_mem_ptr == NULL) {
    elogf(LOG_LEVEL_ERR, "Failed to allocate memory\n");
    free(opts);
    return -EXIT_FAILURE;
  }
  memset((char *)g_mem_ptr, 1, g_mem_size);
  elogf(LOG_LEVEL_DEBUG, "Allocated %d KB for thread work\n",
        g_mem_size / 1024);

  for (i = 0; i < g_mem_size / sizeof(int); i++)
    g_mem_ptr[i] = i;

  // now initialize the for each thread the pointer to the memory
  void *next_t_mem_ptr = g_mem_ptr;
  for (i = 0; i < thread_num; i++) {
    if (buf_type == PRIVATE) {
      // this way the buffer is shared, BUT each thread gets its own different
      // region of the specified size
      thread_stat[i].t_mem_ptr = next_t_mem_ptr;
      next_t_mem_ptr += thread_stat[i].t_mem_size;
    } else {
      thread_stat[i].t_mem_ptr = g_mem_ptr;
    }
  }
  elogf(LOG_LEVEL_DEBUG, "memory setup done,spawning threads\n");

  // allocate thread id array
  pthread_id = (pthread_t *)malloc(sizeof(pthread_t) * thread_num);
  if (pthread_id == NULL) {
    elogf(LOG_LEVEL_ERR, "Cannot allocate memory for thread stats\n");
    free(opts);
    return -EXIT_FAILURE;
  }
  memset(pthread_id, 0, sizeof(pthread_t) * thread_num);

  // spawn threads
  pthread_attr_t pthread_attr;
  int thread_index = 0;
  for (i = 0; i < cpus && thread_index < thread_num; i++) {
    // if we need to spawn a thread on cpu i
    if (CPU_ISSET(i, &cpu_set_mask)) {
      res = pthread_attr_init(&pthread_attr);
      if (res < 0) {
        elogf(LOG_LEVEL_ERR,
              "Cannot initilalize pthread attr for thread on cpu %d: %s\n", i,
              strerror(errno));
        free(opts);
        return -EXIT_FAILURE;
      }
      // we set the affinity for the current thread
      CPU_ZERO(&thread_cpu_set_mask);
      CPU_SET(i, &thread_cpu_set_mask);
      res = pthread_attr_setaffinity_np(&pthread_attr, sizeof(cpu_set_t),
                                        &thread_cpu_set_mask);
      if (res < 0) {
        elogf(LOG_LEVEL_ERR,
              "Cannot set pthread affinity for thread on cpu %d: %s\n", i,
              strerror(errno));
        free(opts);
        return -EXIT_FAILURE;
      }
      // set the thread ID
      thread_stat[thread_index].id = thread_index;
      // set the cpu ID
      thread_stat[thread_index].cpu_id = i;
      // enable the thread
      thread_stat[thread_index].execution_status = THREAD_ENABLED;
      // we spawn the thread
      res = pthread_create(pthread_id + thread_index, &pthread_attr,
                           thread_execution,
                           (void *)(thread_stat + thread_index));
      if (res < 0) {
        elogf(LOG_LEVEL_ERR, "Cannot start thread on cpu %d: %s\n", i,
              strerror(errno));
        free(opts);
        return -EXIT_FAILURE;
      }

      res = pthread_attr_destroy(&pthread_attr);
      if (res < 0) {
        elogf(LOG_LEVEL_ERR,
              "Cannot destroy pthread attr for thread on cpu %d: %s\n", i,
              strerror(errno));
        free(opts);
        return -EXIT_FAILURE;
      }
      thread_index++;
    }
  }
  /* print experiment info before starting */
  flogf(LOG_LEVEL_FILE, log_filep,
        "thread num=%d, global memsize=%d KB, buffer "
        "type=%s \n",
        thread_num, g_mem_size / 1024,
        ((buf_type == SHARED) ? "shared" : "private"));

  flogf(LOG_LEVEL_FILE, log_filep, "\nthread info:\n");
  for (i = 0; i < thread_num; i++) {
    flogf(LOG_LEVEL_FILE, log_filep, "\tthread %d:\n", i);
    flogf(LOG_LEVEL_FILE, log_filep, "\t\tusing cpu: %d\n",
          thread_stat[i].cpu_id);
    flogf(LOG_LEVEL_FILE, log_filep, "\t\taccess type: %s\n",
          (thread_stat[i].acc_type == READ) ? "read" : " write");
    flogf(LOG_LEVEL_FILE, log_filep, "\t\tstop at %d iterations\n",
          thread_stat[i].iterations);
    flogf(LOG_LEVEL_FILE, log_filep, "\t\tbuffer memsize: %d KB\n",
          thread_stat[i].t_mem_size / 1024);
    flogf(LOG_LEVEL_FILE, log_filep, "\t\tbuffer VA: %p\n",
          thread_stat[i].t_mem_ptr);
    fflush(log_filep);
  }
  free(opts);
  elogf(LOG_LEVEL_DEBUG, "benchmark setup done\n");
  return 0;
}

/**
 * @brief This function will unlock the worker threads, once per period.
 * @param[in] parameters_num Number of passed parameters, ignored.
 * @param[in] parameters The list of passed parameters, ignored.
 * @details This function will just wait on mutexes, the start one, to signal
 * all threads that they can start their job, and the end one, to wait until all
 * threads finish their job before reporting.
 */
void benchmark_execution(int parameters_num, void **parameters) {
  int res = 0, i;
  g_nread = 0;
  g_start = 0;
  g_end = 0;
  elogf(LOG_LEVEL_DEBUG, "Main waiting for all worker threads to be ready\n");
  res = wait_for_sem(&main_start_mutex);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Main thread cannot wait main mutex: %s\n",
          strerror(errno));
  }
  elogf(LOG_LEVEL_DEBUG, "Main thread unlocking all worker threads\n");
  for (i = 0; i < thread_num; i++) {
    sem_post(&worker_ready_queue);
    if (res < 0) {
      elogf(LOG_LEVEL_ERR,
            "Main thread cannot unlock thread %d in worker ready queue: %s\n",
            i, strerror(errno));
    }
  }
  elogf(LOG_LEVEL_DEBUG,
        "Main thread waiting for all worker threads to finish work\n");
  res = wait_for_sem(&main_end_mutex);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Main thread cannot wait main mutex: %s\n",
          strerror(errno));
  }
  for (i = 0; i < thread_num; i++) {
    sem_post(&worker_complete_queue);
    if (res < 0) {
      elogf(
          LOG_LEVEL_ERR,
          "Main thread cannot unlock thread %d in worker complete queue: %s\n",
          i, strerror(errno));
    }
  }
  for (i = 0; i < thread_num; i++) {
    elogf(LOG_LEVEL_DEBUG, "summing thread %d BW\n", i);
    if (g_start == 0 || thread_stat[i].t_start < g_start) {
      g_start = thread_stat[i].t_start;
    }
    g_nread += thread_stat[i].nread;
    if (g_end == 0 || thread_stat[i].t_end > g_end) {
      g_end = thread_stat[i].t_end;
    }
  }
  elogf(LOG_LEVEL_DEBUG, "All worker threads waiting for next period\n");
}

/** @brief Tiny hanler to just compute the banddwidth value.
 * @param[in] start Start timestamp un usecs.
 * @param[in] end End timestamp un usecs.
 * @param[in] nread Number of bytes read during the interval.
 * @returns The computed bandwidth.
 */
static inline double calculate_bandwidth(unsigned int start, unsigned int end,
                                         uint64_t nread) {
  elogf(LOG_LEVEL_DEBUG, "start : %u end %u\n", start, end);
  double dur = end - start;
  double dur_in_sec = (double)dur / 1000000.0f;
  elogf(LOG_LEVEL_DEBUG, "dur : %lf ur_insec %lf\n", dur, dur_in_sec);
  return (double)nread / dur_in_sec / 1024.0f / 1024.0f;
}

/**
 * @brief This handler computes the bandwidth as the extra measurement. @details
 * This function computes the experienced bandwidth (MBps) as a double after
 * each execution phase, reporting both the total and per thread extracted
 * bandwidth . This function is only called if the benchamrk has been built with
 * the `-DEXTENDED_REPORT`.
 */
void benchmark_log_data(void) {
  int i;
  extra_measurement.data[0] = calculate_bandwidth(g_start, g_end, g_nread);
  for (i = 0; i < thread_num; i++) {
    elogf(LOG_LEVEL_DEBUG, "thread %d start %u end %u, read %lu, size %d\n", i,
          thread_stat[i].t_start, thread_stat[i].t_end, thread_stat[i].nread,
          thread_stat[i].t_mem_size) extra_measurement.data[i + 1] =
        calculate_bandwidth(thread_stat[i].t_start, thread_stat[i].t_end,
                            thread_stat[i].nread);
  }
}

/**
 * @brief Will revert what `benchmark_init()` has done to initialize the
 * benchmark.
 * @param[in] parameters_num Ignored.
 * @param[in] parameters Ignored.
 * @details It will free `::g_mem_ptr`.
 */
void benchmark_teardown(int parameters_num, void **parameters) {
  int i = 0;
  if (thread_num > 0) {
    // disable all threads
    if (thread_stat != NULL) {
      for (i = 0; i < thread_num; i++) {
        elogf(LOG_LEVEL_DEBUG, "Disabling thread %d\n", i);
        thread_stat[i].execution_status = THREAD_DISABLED;
      }
    }
    if (pthread_id != NULL) {
      elogf(LOG_LEVEL_DEBUG,
            "Unlocking worker threads to allow them to terminate\n");
      for (i = 0; i < thread_num; i++) {
        sem_post(&worker_ready_queue);
        sem_post(&worker_complete_queue);
      }
      // join with all the threads
      for (i = 0; i < thread_num; i++) {
        elogf(LOG_LEVEL_DEBUG, "Joining on thread %d\n", i);
        pthread_join(pthread_id[i], NULL);
      }
    }
  }
  if (pthread_id != NULL) {
    // now we can deallocate all resources
    free(pthread_id);
  }
  sem_destroy(&main_start_mutex);
  sem_destroy(&main_end_mutex);
  sem_destroy(&worker_mutex);
  sem_destroy(&worker_ready_queue);
  sem_destroy(&worker_complete_queue);
  if (extra_measurement.header != NULL) {
    free(extra_measurement.header);
  }
  if (extra_measurement.data != NULL) {
    free(extra_measurement.data);
  }
  if (g_mem_ptr != NULL) {
    free(g_mem_ptr);
  }
  if (thread_stat != NULL) {
    free(thread_stat);
  }
}

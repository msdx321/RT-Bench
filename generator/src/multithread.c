/** @file multithread.h
 * @ingroup generator
 * @brief APIs for multithreaded periodic benchmarks.
 * @author Mattia Nicolella
 *
 * @copyright (C) 2025, Mattia Nicolella <mnico@bu.edu> and the rt-bench
 * contributors. SPDX-License-Identifier: MIT
 */

#include "multithread.h"
#include "bits/pthreadtypes.h"
#include "logging.h"
#include "pthread.h"
#include <errno.h>
#include <semaphore.h>
#include <string.h>

/// Global variable that carries information on benchmark threads.
struct bench_thread bench_thread = {0};
/// Last element in `::bench_thread`
static struct bench_thread_info *last_thread = NULL;

static pthread_mutex_t
    bench_mutex = PTHREAD_MUTEX_INITIALIZER, ///< A mutex to have all worker
                                             ///< threads update a
                                             ///< `::num_threads_ready`,
                                             ///< `::num_thread_cmpleted`.
    multithread_status_mutex =
        PTHREAD_MUTEX_INITIALIZER; ///<  A mutex to read/write
                                   ///< `::multithread_status` without race
                                   ///< conditions.
static sem_t bench_ready_queue, ///< The queue of worker threads that will need
                                ///< to be unlocked by the main thread.
    bench_complete_queue, ///< The queue of worker threads that will need to be
                          ///< unlocked by the main thread.
    main_start_sem, ///< The mutex used by the main thread to wait for all the
                    ///< benchmark worker threads to be ready to start.
    main_end_sem;   ///< The mutex used by the main thread to wait for all the
                    ///< worker threads to have finished the loop.
static int
    num_threads_ready =
        0, ///< The number of threads ready to start a new computation loop.
    num_threads_completed =
        0; ///< The number of threads that have completed a computation loop.

static pthread_cond_t all_threads_initialized =
    PTHREAD_COND_INITIALIZER; ///< If RT-Bench has finished initializing all
                              ///< threads, this variable shares a mutex with
                              ///< `::multithread_status`.
static enum thread_execution_status multithread_status =
    MULTITHREAD_DISABLED; ///< If multithreaded execution status.

///@details This function will also broadcast all threads threre is an error.
inline enum thread_execution_status get_multithread_status(void) {
  int res;
  enum thread_execution_status status;
  res = pthread_mutex_lock(&multithread_status_mutex);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot lock multithread status mutex: %s",
          strerror(errno));
    multithread_status = MULTITHREAD_ERR;
    pthread_cond_broadcast(&all_threads_initialized);
    pthread_mutex_unlock(&multithread_status_mutex);
    return MULTITHREAD_ERR;
  }
  status = multithread_status;
  res = pthread_mutex_unlock(&multithread_status_mutex);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot unlock multithread status mutex: %s",
          strerror(errno));
    multithread_status = MULTITHREAD_ERR;
    pthread_cond_broadcast(&all_threads_initialized);
    return MULTITHREAD_ERR;
  }
  return status;
}

///@details This function will also broadcast all threads of the change.
int set_multithread_status(enum thread_execution_status status) {
  int res = 0;
  res = pthread_mutex_lock(&multithread_status_mutex);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot lock multithread status mutex: %s",
          strerror(errno));
    multithread_status = MULTITHREAD_ERR;
    pthread_cond_broadcast(&all_threads_initialized);
    pthread_mutex_unlock(&multithread_status_mutex);
    return res;
  }
  elogf(LOG_LEVEL_DEBUG, "got multithread status mutex\n");
  if (multithread_status == MULTITHREAD_ERR) {
    elogf(LOG_LEVEL_DEBUG, "multithread status is in error state!\n");
    pthread_cond_broadcast(&all_threads_initialized);
    pthread_mutex_unlock(&multithread_status_mutex);
    return -1;
  }
  elogf(LOG_LEVEL_DEBUG, "multithread status is %d\n", multithread_status);
  multithread_status = status;
  res = pthread_cond_broadcast(&all_threads_initialized);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot broadcast multithread status change: %s",
          strerror(errno));
    multithread_status = MULTITHREAD_ERR;
    pthread_cond_broadcast(&all_threads_initialized);
  }
  elogf(LOG_LEVEL_DEBUG, "changed multithread status to %d\n",
        multithread_status);
  res = pthread_mutex_unlock(&multithread_status_mutex);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot unlock multithread status mutex: %s",
          strerror(errno));
    multithread_status = MULTITHREAD_ERR;
    pthread_cond_broadcast(&all_threads_initialized);
  }
  elogf(LOG_LEVEL_DEBUG, "released multithread status mutex\n");
  return res;
}

/** @brief Wait until all threads are initialized.
 * @returns `0` on success `-1` on failure.
 */
static inline int wait_for_thread_init(void) {
  int ret = 0;
  ret = pthread_mutex_lock(&multithread_status_mutex);
  if (ret < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot lock multithread status mutex: %s",
          strerror(errno));
    multithread_status = MULTITHREAD_ERR;
    pthread_mutex_unlock(&multithread_status_mutex);
    return ret;
  }
  while (multithread_status != MULTITHREAD_ENABLED) {
    if (multithread_status <= MULTITHREAD_DISABLED) {
      pthread_mutex_unlock(&multithread_status_mutex);
      return -1;
    }
    pthread_cond_wait(&all_threads_initialized, &multithread_status_mutex);
  }
  ret = pthread_mutex_unlock(&multithread_status_mutex);
  if (ret < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot lock multithread status mutex: %s",
          strerror(errno));
    multithread_status = MULTITHREAD_ERR;
  }
  return ret;
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

/// Make benchmark worker thread wait to be released by RT-Bench to start the
/// task.
void bench_thread_sync_start(struct bench_thread_info *info) {
  int res;
  // thread_num + main thread rendevous to start computing
  res = pthread_mutex_lock(&bench_mutex);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR,
          "Benchmark worker thread %lu cannot wait on bench mutex: %s\n",
          info->thread_id, strerror(errno));
  }
  num_threads_ready++;
  elogf(LOG_LEVEL_DEBUG, "thread %lu start sync ready %d complete %d\n",
        info->thread_id, num_threads_ready, num_threads_completed);
  if (num_threads_ready == bench_thread.num_threads) {
    num_threads_ready = 0;
    res = sem_post(&main_start_sem);
    if (res < 0) {
      elogf(LOG_LEVEL_ERR, "Thread %lu cannot post on main mutex: %s\n",
            info->thread_id, strerror(errno));
    }
  }
  res = pthread_mutex_unlock(&bench_mutex);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Thread %lu cannot post on bench mutex: %s\n",
          info->thread_id, strerror(errno));
  }
  res = wait_for_sem(&bench_ready_queue);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Thread %lu cannot wait on bench ready queue: %s\n",
          info->thread_id, strerror(errno));
  }
  /// If the benchmark worker thread gets unlocked and the multithread status is
  /// not enalbed it has to exit.
  if (get_multithread_status() != MULTITHREAD_ENABLED) {
    pthread_exit(NULL);
  }
  elogf(LOG_LEVEL_DEBUG, "Bench thread %lu starting\n", info->thread_id);
}

/// Make benchmark worker thread wait other worker threads to finish the task.
void bench_thread_sync_end(struct bench_thread_info *info) {
  // thread_num + main thread rendevous to start computing
  int res;
  res = pthread_mutex_lock(&bench_mutex);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Thread %lu cannot wait on worker mutex: %s\n",
          info->thread_id, strerror(errno));
  }
  num_threads_completed++;
  elogf(LOG_LEVEL_DEBUG, "thread %lu end sync ready %d  completed %d\n",
        info->thread_id, num_threads_ready, num_threads_completed);
  if (num_threads_completed == bench_thread.num_threads) {
    num_threads_completed = 0;
	elogf(LOG_LEVEL_DEBUG,"Thread %lu unlocking RT-Bench\n",info->thread_id);
    res = sem_post(&main_end_sem);
    if (res < 0) {
      elogf(LOG_LEVEL_ERR, "Thread %lu cannot post on main mutex: %s\n",
            info->thread_id, strerror(errno));
    }
  }
  res = pthread_mutex_unlock(&bench_mutex);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Thread %lu cannot post on worker mutex: %s\n",
          info->thread_id, strerror(errno));
  }
  res = wait_for_sem(&bench_complete_queue);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR,
          "Thread %lu cannot wait on worker complete queue: %s\n",
          info->thread_id, strerror(errno));
  }
  elogf(LOG_LEVEL_DEBUG, "Worker thread %lu waiting for next task\n",
        info->thread_id);
}

/** @brief The function all benchmark worker threads will execute.
 * @param[in] arg The `::bench_thread_info` item for the thread.
 * @details
 * This function is a wrapper for a benchamrk-specific thread function, to
 * ensure the benchmarks threads are synchornised correctly with the main
 * RT-Bench thread.
 * It waits until the main RT-Bench thread enables multithreading and then
 * executes the benchmark-specific function in a loop,
 * protected by two synchronization functions, until RT-Bench disables
 * multithreading.
 * @returns `NULL` if the thread was interrupted while synchonising, or the
 * value returned from the benchmark-specific thread functions.
 */
void *bench_worker_thread_func(void *arg) {
  int ret;
  if (arg == NULL) {
    elogf(LOG_LEVEL_ERR, "Worker thread has no argument\n");
    exit(-EXIT_FAILURE);
  }
  struct bench_thread_info *info = (struct bench_thread_info *)arg;
  void *res = NULL;
  ret = wait_for_thread_init();
  if (ret < 0) {
    return NULL;
  }
  // wait for all threads to be initialized
  while (get_multithread_status() == MULTITHREAD_ENABLED) {
    bench_thread_sync_start(info);
    res = info->thread_func(info->thread_arg);
    bench_thread_sync_end(info);
  }
  return res;
}

/** @details Updates `::bench_thread` and call `pthread_create`.
 * Additionally it also wrap `func` so the benchmark does not
 * have to handle the synchronization between worker threads and RT-Bench main thread.
 */
int create_bench_thread(pthread_attr_t *attr, void *(*func)(void *),
                        void *arg) {
  int res = 0;
  res = set_multithread_status(MULTITHREAD_INITIALIZING);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot set multithread status to initializing\n");
    set_multithread_status(MULTITHREAD_ERR);
    return res;
  }
  // create new thread_info item
  struct bench_thread_info *new = malloc(sizeof(struct bench_thread_info));
  if (new == NULL) {
    elogf(
        LOG_LEVEL_ERR,
        "Cannot allocate memory for benchmark worker thread list item num %d\n",
        bench_thread.num_threads + 1);
    set_multithread_status(MULTITHREAD_ERR);
    return -1;
  }
  new->thread_arg = arg;
  new->thread_func = func;
  // create the thread
  res = pthread_create(&(new->thread_id), attr, bench_worker_thread_func, new);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot create benchmark worker thread num %d: %s\n",
          bench_thread.num_threads + 1, strerror(errno));
    set_multithread_status(MULTITHREAD_ERR);
    return res;
  }
  // update bench thread
  if (bench_thread.num_threads == 0) {
    bench_thread.threads = new;
  }
  bench_thread.num_threads++;
  if (last_thread != NULL) {
    last_thread->next = new;
  }
  last_thread = new;
  return res;
}

int main_multithread_init(void) {
  int res;
  res = sem_init(&main_start_sem, 0, 0);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot initialize main start mutex: %s\n",
          strerror(errno));
    return -EXIT_FAILURE;
  }
  res = sem_init(&main_end_sem, 0, 0);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot initialize main end mutex: %s\n",
          strerror(errno));
    return -EXIT_FAILURE;
  }
  res = sem_init(&bench_ready_queue, 0, 0);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot initialize worker ready queue: %s\n",
          strerror(errno));
    return -EXIT_FAILURE;
  }
  res = sem_init(&bench_complete_queue, 0, 0);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot initialize worker complete queue: %s\n",
          strerror(errno));
    return -EXIT_FAILURE;
  }
  return set_multithread_status(MULTITHREAD_ENABLED);
}

int main_thread_sync_start(void) {
  int res = 0, i;
  elogf(LOG_LEVEL_DEBUG,
        "RT-Bench waiting for all worker threads to be ready\n");
  res = wait_for_sem(&main_start_sem);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "RT-Bench cannot wait main mutex: %s\n",
          strerror(errno));
    set_multithread_status(MULTITHREAD_ERR);
    return res;
  }
  elogf(LOG_LEVEL_DEBUG, "RT-Bench unlocking all worker threads\n");
  for (i = 0; i < bench_thread.num_threads; i++) {
    sem_post(&bench_ready_queue);
    if (res < 0) {
      elogf(LOG_LEVEL_ERR,
            "Main thread cannot unlock thread %d in worker ready queue: %s\n",
            i, strerror(errno));
      set_multithread_status(MULTITHREAD_ERR);
      return res;
    }
  }
  return res;
}

int main_thread_sync_end(void) {
  int i, res = 0;
  elogf(LOG_LEVEL_DEBUG,
        "RT-Bench waiting for all worker threads to finish work\n");
  res = wait_for_sem(&main_end_sem);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Main thread cannot wait main mutex: %s\n",
          strerror(errno));
    set_multithread_status(MULTITHREAD_ERR);
    return res;
  }
  for (i = 0; i < bench_thread.num_threads; i++) {
    sem_post(&bench_complete_queue);
    if (res < 0) {
      elogf(LOG_LEVEL_ERR,
            "RT-Bench cannot unlock thread %d in worker complete queue: %s\n",
            i, strerror(errno));
      set_multithread_status(MULTITHREAD_ERR);
      return res;
    }
  }
  return res;
}

void destroy_bench_threads(void) {
  int i = 0;
  struct bench_thread_info *current = NULL, *next = NULL;
  set_multithread_status(MULTITHREAD_DISABLED);
  if (bench_thread.num_threads > 0) {
    // unlock all threads
    if (bench_thread.threads != NULL) {
      elogf(LOG_LEVEL_DEBUG,
            "Unlocking worker threads to allow them to terminate\n");
      for (i = 0; i < bench_thread.num_threads; i++) {
        sem_post(&bench_ready_queue);
        sem_post(&bench_complete_queue);
      }
      // join with all the threads
      current = bench_thread.threads;
      while (current == NULL) {
        elogf(LOG_LEVEL_DEBUG, "Joining on thread %d\n", i);
        next = current->next;
        pthread_join(current->thread_id, NULL);
        free(current);
        current = next;
      }
    }
  }
  sem_destroy(&main_start_sem);
  sem_destroy(&main_end_sem);
  pthread_mutex_destroy(&bench_mutex);
  pthread_mutex_destroy(&multithread_status_mutex);
  pthread_cond_destroy(&all_threads_initialized);
  sem_destroy(&bench_ready_queue);
  sem_destroy(&bench_complete_queue);
}
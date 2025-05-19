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
#include <stdint.h>
#include <string.h>

/// List of benchmark worker threads and their information.
struct bench_thread_info {
  pthread_t thread_id;            ///< The thread id.
  void *(*thread_func)(void *);   ///< The thread function.
  void *thread_arg;               ///< The argument for the thread function.
  struct bench_thread_info *next; ///< The next element.
};

/// Global status for the multithread benchmarks.
struct bench_thread {
  struct bench_thread_info *threads; ///< The 1st item of the list of threads
  unsigned int num_threads;          /// < The total number of threads.
};

/// Global variable that carries information on benchmark threads.
struct bench_thread bench_thread = {0};
/// Last element in `::bench_thread`
static struct bench_thread_info *last_thread = NULL;

static pthread_mutex_t
    thread_ready_mutex = PTHREAD_MUTEX_INITIALIZER, ///< A mutex to have all
                                                    ///< worker threads update a
                                                    ///< `::num_threads_ready`,
    thread_completed_mutex =
        PTHREAD_MUTEX_INITIALIZER, ///< A mutex to have all worker
                                   ///< threads update a
                                   ///< `::num_thread_completed`.
    multithread_status_mutex =
        PTHREAD_MUTEX_INITIALIZER; ///<  A mutex to read/write
                                   ///< `::multithread_status` without race
                                   ///< conditions.
static int num_threads_ready =
               0, ///< The number of threads ready to start a new computation
                  ///< loop, protected by `::threads_ready_mutex`.
    num_threads_completed =
        0; ///< The number of threads that have completed a computation loop,
           ///< protected by `::threads_completed_mutex`.

static pthread_cond_t multithread_status_change =
    PTHREAD_COND_INITIALIZER; ///<  If
                              ///< `::multithread_status` has changed,
                              ///< protected by
                              ///< `::multithread_status_mutex`.
sem_t threads_ready,          /// < RT-Bench nedd to wait until all threads
                              /// are ready to start the task.
    threads_completed;        /// < RT-Bench nedd to wait until all threads
                              /// are done with the current task.
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
    pthread_cond_broadcast(&multithread_status_change);
    pthread_mutex_unlock(&multithread_status_mutex);
    return MULTITHREAD_ERR;
  }
  status = multithread_status;
  res = pthread_mutex_unlock(&multithread_status_mutex);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot unlock multithread status mutex: %s",
          strerror(errno));
    multithread_status = MULTITHREAD_ERR;
    pthread_cond_broadcast(&multithread_status_change);
    return MULTITHREAD_ERR;
  }
  return status;
}

///@details initializes semaphores for multithread execution
int multithread_init(void) {
  int res = 0;
  res = sem_init(&threads_ready, 0, 0);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot initialize threads_ready semaphore: %s",
          strerror(errno));
    return res;
  }
  res = sem_init(&threads_completed, 0, 0);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot initialize threads_completed semaphore: %s",
          strerror(errno));
    return res;
  }
  return res;
}

///@details This function will also broadcast all threads of the change.
inline int set_multithread_status(enum thread_execution_status status) {
  int res = 0;
  res = pthread_mutex_lock(&multithread_status_mutex);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot lock multithread status mutex: %s",
          strerror(errno));
    multithread_status = MULTITHREAD_ERR;
    pthread_cond_broadcast(&multithread_status_change);
    return res;
  }
  elogf(LOG_LEVEL_DEBUG, "got multithread status mutex\n");
  if (multithread_status == MULTITHREAD_ERR) {
    elogf(LOG_LEVEL_DEBUG, "multithread status is in error state!\n");
    pthread_cond_broadcast(&multithread_status_change);
    pthread_mutex_unlock(&multithread_status_mutex);
    return -1;
  }
  elogf(LOG_LEVEL_DEBUG, "multithread status is %d\n", multithread_status);
  multithread_status = status;
  res = pthread_cond_broadcast(&multithread_status_change);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot broadcast multithread status change: %s",
          strerror(errno));
    multithread_status = MULTITHREAD_ERR;
    pthread_cond_broadcast(&multithread_status_change);
  }
  elogf(LOG_LEVEL_DEBUG, "changed multithread status to %d\n",
        multithread_status);
  res = pthread_mutex_unlock(&multithread_status_mutex);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot unlock multithread status mutex: %s",
          strerror(errno));
    multithread_status = MULTITHREAD_ERR;
    pthread_cond_broadcast(&multithread_status_change);
  }
  elogf(LOG_LEVEL_DEBUG, "released multithread status mutex\n");
  return res;
}

/** @brief Wait until the multithread status reaches the desired state
 * @param desired_status The status of `::multhread_staus` we need to wait for.
 * @details Function will exit with an error if `::multithread_status` is either
 * `MULTIHREAD_DISABLED` or less.
 * @returns `0` on success `-1` on failure.
 */
static inline int wait_for_main(enum thread_execution_status desired_status) {
  int ret = 0, res = 0;
  ret = pthread_mutex_lock(&multithread_status_mutex);
  if (ret < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot lock multithread status mutex: %s",
          strerror(errno));
    multithread_status = MULTITHREAD_ERR;
    pthread_mutex_unlock(&multithread_status_mutex);
    return ret;
  }
  while (multithread_status != desired_status &&
         multithread_status > MULTITHREAD_DISABLED) {
    pthread_cond_wait(&multithread_status_change, &multithread_status_mutex);
  }
  res = multithread_status == MULTITHREAD_ERR;
  ret = pthread_mutex_unlock(&multithread_status_mutex);
  if (ret < 0) {
    res = ret;
    elogf(LOG_LEVEL_ERR, "Cannot lock multithread status mutex: %s",
          strerror(errno));
    multithread_status = MULTITHREAD_ERR;
  }
  return res;
}

/** @brief Increment `::num_threads_ready`.
 * @details Signal main thread only when all threads are ready.
 * @returns `0` on success, `-1` on error
 */
static inline int add_thread_ready(void) {
  int res = 0;
  res = pthread_mutex_lock(&thread_ready_mutex);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot lock thread_ready_mutex: %s\n",
          strerror(errno));
  }
  num_threads_ready++;
    elogf(LOG_LEVEL_DEBUG, "%d/%d threads ready\n",num_threads_ready,bench_thread.num_threads);
  if (num_threads_ready >= bench_thread.num_threads) {
    elogf(LOG_LEVEL_DEBUG, "last ready thread signals main\n");
    res = sem_post(&threads_ready);
    if (res < 0) {
      elogf(LOG_LEVEL_ERR, "Cannot signal that all threads are ready: %s\n",
            strerror(errno));
    }
  }
  res = pthread_mutex_unlock(&thread_ready_mutex);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot unlock thread_ready_mutex: %s\n",
          strerror(errno));
  }
  return res;
}

/** @brief Increment `::num_threads_completed`.
 * @details Signal main thread only when all threads are ready.
 * @returns `0` on success, `-1` on error
 */
static inline int add_thread_completed(void) {
  int res = 0;
  res = pthread_mutex_lock(&thread_completed_mutex);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot lock thread_completed_mutex: %s\n",
          strerror(errno));
  }
  num_threads_completed++;
    elogf(LOG_LEVEL_DEBUG, "%d/%d threads completed\n",num_threads_completed,bench_thread.num_threads);
  if (num_threads_completed >= bench_thread.num_threads) {
    elogf(LOG_LEVEL_DEBUG, "last completed thread signals main\n");
    res = sem_post(&threads_completed);
    if (res < 0) {
      elogf(LOG_LEVEL_ERR, "Cannot signal that all threads are completed: %s\n",
            strerror(errno));
    }
  }
  res = pthread_mutex_unlock(&thread_completed_mutex);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Cannot unlock thread_completed_mutex: %s\n",
          strerror(errno));
  }
  return res;
}

/// Make benchmark worker thread wait to be released by RT-Bench to start the
/// task.
static inline void bench_thread_sync_start(struct bench_thread_info *info) {
  int res;
  res = add_thread_ready();
  if (res < 0) {
    return;
  }
  res = wait_for_main(MULTITHREAD_ENABLED);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Thread %lu cannot wait for main on start sync: %s\n",
          info->thread_id, strerror(errno));
  }
  /// If the benchmark worker thread gets unlocked and the multithread status is
  /// not enabled it has to exit.
  if (get_multithread_status() <= MULTITHREAD_DISABLED) {
    elogf(LOG_LEVEL_DEBUG, "Worker thread %lu skipping task and terminating\n",
          info->thread_id);
    // before exiting signal this thread had completed, so the main thread does
    // not wait for us.
    add_thread_completed();
    pthread_exit(NULL);
  }
}

/// Make benchmark worker thread wait other worker threads to finish the task.
static inline void bench_thread_sync_end(struct bench_thread_info *info) {
  // thread_num + main thread rendevous to start computing
  int res;
  res = add_thread_completed();
  if (res < 0) {
    return;
  }
  res = wait_for_main(MULTITHREAD_WAITING_END);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "Thread %lu cannot wait for main to be ready: %s\n",
          info->thread_id, strerror(errno));
  }
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
  if (arg == NULL) {
    elogf(LOG_LEVEL_ERR, "Worker thread has no argument\n");
    exit(-EXIT_FAILURE);
  }
  struct bench_thread_info *info = (struct bench_thread_info *)arg;
  void *res = NULL;
	int ret=0;
  uint64_t task_id = 0;
  // wait for all threads to be initialized
    elogf(LOG_LEVEL_DEBUG, "Worker thread %lu waiting for end of main thread init\n",
          info->thread_id);
  ret = wait_for_main(MULTITHREAD_WAITING_START);
  if (ret < 0) {
    elogf(LOG_LEVEL_ERR, "Thread %lu cannot wait for main have spawned all threads: %s\n",
          info->thread_id, strerror(errno));
		return NULL;
  }
    elogf(LOG_LEVEL_DEBUG, "Worker thread %lu detected end of main thread init\n",
          info->thread_id);
  while (get_multithread_status() >= MULTITHREAD_WAITING_START) {
    bench_thread_sync_start(info);
    elogf(LOG_LEVEL_DEBUG, "Worker thread %lu starting task #%ld\n",
          info->thread_id, task_id);
    res = info->thread_func(info->thread_arg);
    bench_thread_sync_end(info);
    elogf(LOG_LEVEL_DEBUG, "Worker thread %lu waiting for next task\n",
          info->thread_id);
    task_id++;
  }
  elogf(LOG_LEVEL_DEBUG, "thread %lu terminating\n", info->thread_id);
  return res;
}

/** @details Updates `::bench_thread` and call `pthread_create`.
 * Additionally it also wrap `func` so the benchmark does not
 * have to handle the synchronization between worker threads and RT-Bench main
 * thread.
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
  memset(new, 0, (sizeof(struct bench_thread_info)));
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

int main_thread_sync_start(void) {
  int res = 0;
  elogf(LOG_LEVEL_DEBUG,
        "main thread waiting for %d worker threads to be ready\n",bench_thread.num_threads);
  do {
    res = sem_wait(&threads_ready);
  } while (res < 0 && errno == EINTR);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR,
          "main thread wait for worker threads to be ready: %s\n",
          strerror(errno));
    set_multithread_status(MULTITHREAD_ERR);
    return res;
  }
  res = pthread_mutex_lock(&thread_ready_mutex);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "main thread cannot lock  thread_ready_mutex: %s\n",
          strerror(errno));
    set_multithread_status(MULTITHREAD_ERR);
    return res;
  }
  num_threads_ready = 0;
  pthread_mutex_unlock(&thread_ready_mutex);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "main thread cannot unlock  thread_ready_mutex: %s\n",
          strerror(errno));
    set_multithread_status(MULTITHREAD_ERR);
    return res;
  }
  elogf(LOG_LEVEL_DEBUG, "main thread unlocking all worker threads\n");
  res = set_multithread_status(MULTITHREAD_ENABLED);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "main thread cannot unlock all threads: %s\n",
          strerror(errno));
    set_multithread_status(MULTITHREAD_ERR);
    return res;
  }
  return res;
}

int main_thread_sync_end(void) {
  int res = 0;
  elogf(LOG_LEVEL_DEBUG,
        "main thread waiting for %d worker threads to be completed\n",bench_thread.num_threads);
  do {
    res = sem_wait(&threads_completed);
  } while (res < 0 && errno == EINTR);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "main thread wait for worker threads completion: %s\n",
          strerror(errno));
    set_multithread_status(MULTITHREAD_ERR);
    return res;
  }
  res = pthread_mutex_lock(&thread_completed_mutex);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR,
          "main thread cannot lock  thread_completed_mutex: %s\n",
          strerror(errno));
    set_multithread_status(MULTITHREAD_ERR);
    return res;
  }
  num_threads_completed = 0;
  pthread_mutex_unlock(&thread_completed_mutex);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR,
          "main thread cannot unlock  thread_completed_mutex: %s\n",
          strerror(errno));
    set_multithread_status(MULTITHREAD_ERR);
    return res;
  }
  elogf(LOG_LEVEL_DEBUG, "main thread unlocking all worker threads \n");
  res = set_multithread_status(MULTITHREAD_WAITING_END);
  if (res < 0) {
    elogf(LOG_LEVEL_ERR, "main thread cannot unlock all threads: %s\n",
          strerror(errno));
    set_multithread_status(MULTITHREAD_ERR);
    return res;
  }
  return res;
}

void destroy_bench_threads(void) {
  int i = 0;
  struct bench_thread_info *current = NULL, *next = NULL;
  if (bench_thread.num_threads > 0) {
    elogf(LOG_LEVEL_TRACE, "Stopping benchmark threads\n");
    set_multithread_status(MULTITHREAD_DISABLED);
    elogf(LOG_LEVEL_DEBUG, " Stopping %d threads, list head %p\n",
          bench_thread.num_threads, bench_thread.threads);
    if (bench_thread.threads != NULL) {
      // join with all the threads
      current = bench_thread.threads;
      while (current != NULL) {
        elogf(LOG_LEVEL_DEBUG, "Joining on thread %d\n", i);
        next = current->next;
        pthread_join(current->thread_id, NULL);
        free(current);
        current = next;
      }
    }
  }
  pthread_mutex_destroy(&thread_ready_mutex);
  pthread_mutex_destroy(&thread_completed_mutex);
  pthread_mutex_destroy(&multithread_status_mutex);
  pthread_cond_destroy(&multithread_status_change);
  sem_post(&threads_ready);
  sem_post(&threads_completed);
  sem_destroy(&threads_ready);
  sem_destroy(&threads_completed);
}
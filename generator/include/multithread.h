#ifndef MULTITHREAD_H
#define MULTITHREAD_H

/** @file multithread.h
 * @ingroup generator
 * @brief APIs for multithreaded periodic benchmarks.
 * @author Mattia Nicolella
 *
 * @copyright (C) 2025, Mattia Nicolella <mnico@bu.edu> and the rt-bench
 * contributors. SPDX-License-Identifier: MIT
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <pthread.h>

/// Thread status enum, so we know when thread functionality is enabled or not.
enum thread_execution_status {
  MULTITHREAD_ERR = -1,      ///< Error during multithread execution.
  MULTITHREAD_DISABLED = 0,  ///< Multithread execution disabled.
  MULTITHREAD_INITIALIZING, ///< Threads are being spawned.
  MULTITHREAD_WAITING_START, ///< Threads are waiting to start the task.
  MULTITHREAD_ENABLED,       ///< Threads are enabled.
  MULTITHREAD_WAITING_END    ///< Threads are done with the task.
};

/** @brief initialize global resources for multhreaded execution.
 * @returns `0` on success, `-1` otherwise.
 */
int multithread_init();

/** @brief RT-Bench waits until all benchmark worker threads can start executing
 * the task.
 * @returns `0` on success, `-1` on error.
 */
int main_thread_sync_start(void);

/** @brief RT-Bench waits until all benchmark worker threads can are done
 * executing the task.
 * @returns `0` on success, `-1` on error.
 */
int main_thread_sync_end(void);

/** @brief Create an track a worker thread
 * @param[in] attr The thread attribute, passed directly to `phtread_create`.
 * @param[in] func The function that the worker thread needs to execute.
 * @param[in] arg The argument for the thread function, passed directly to
 * `phtread_create`.
 * @returns `0` on success, `-1` on error.
 */
int create_bench_thread(pthread_attr_t *_attr, void *(*func)(void *),
                        void *arg);

/// Destroy the worker threads and all the related synchronisation resources.
void destroy_bench_threads(void);

/** @brief Check the status on multhreaded execution.
 * @returns on of the possible values of the `multithread_status` enum.
 */
enum thread_execution_status get_multithread_status(void);

/** @brief Change the status of multhreaded execution.
 * @param[in] status The new value to set.
 * @returns `0` on success `-1` on error.
 */
int set_multithread_status(enum thread_execution_status status);

#endif
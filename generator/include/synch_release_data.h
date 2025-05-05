/** @file synch_release.h
 * @ingroup generator
 * @brief Data structures for having multiple benchmark instances start at the same time.
 * @author Mattia Nicolella
 *
 * @copyright (C) 2021 - 2024, Mattia Nicolella <mnico@bu.edu> and the rt-bench contributors.
 * SPDX-License-Identifier: MIT
 */

#ifndef SYNCH_RELEASE_DATA_H
#define SYNCH_RELEASE_DATA_H

#include <stdbool.h>
#include <semaphore.h>
#include <sys/time.h>
#include <sys/types.h>

/// Default name for the synchonised benchmark group.
#define SYNCH_GRP_DEFAULT_NAME "default_group"

/// Enum to query the status of the synchronised release features.
enum synch_status {
  SYNCH_DISABLED = 0, ///< Synch release features disabled.
	SYNCH_ENABLED, ///< Synch release features enabled and shared structures ready.
};

struct synch_shm {
  enum synch_status
      status;      ///< The status of the synchornised release features.
    bool unblocked;  ///< Sanity check variable to make sure that no benchmarks
                     // join after we unblock.
    sem_t synch_sem; ///< The semaphore used for benchmark synchronisation.
    unsigned int group_size;     ///< The number of benchmarks in the group.
    unsigned int waiting_bmarks; ///< The number of benchmarks waiting for
                                 ///< synchronisation.
    struct timespec timer_initial_delay; ///< The initial delay of the
                                         ///< timer.
    pid_t unblocker_pid; ///< The pid of the process responsible for
                         ///< unblocking the group
    pid_t master_pid; ///< The pid of the process responsible for
                         ///< allocating and releasing group shared resources.
  };
#endif
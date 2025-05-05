/** @file synch_release.h
 * @ingroup generator
 * @brief Functions for having multiple benchmark instances start at the same time.
 * @author Mattia Nicolella
 *
 * @copyright (C) 2021 - 2024, Mattia Nicolella <mnico@bu.edu> and the rt-bench contributors.
 * SPDX-License-Identifier: MIT
 */

#ifndef SYNCH_RELEASE_H
#define SYNCH_RELEASE_H

#include <stdbool.h>
#include <semaphore.h>
#include <sys/time.h>
#include <sys/types.h>
#include "synch_release_data.h"

/// @brief Get the status of the synchronisation features
enum synch_status get_synch_status();

/** @brief Get the shared aboslute timestamp at which all benchmarks have to start.
 * @return The itimerspect struct with the absolute timestamp. A struct initialized to `0` indicates an error.
 */
struct timespec get_synch_delay();

/**@brief Add the current benchmark to a group of benchmark that will start simultaneously.
 * @param[in] group_name The name of the benchmark group.
 * @return `0` on success, `<0` on error.
 */
int init_synchronised_benchmark_group(const char *group_name);

// @brief Wait for synchronisation with rest of the group.
int wait_for_synch();

/// Deallocate shared resources for synchronised benchmark start.
void deallocate_synch_resources();

#endif
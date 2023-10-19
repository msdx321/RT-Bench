/** @file memory_watcher.h
 * @ingroup generator
 * @brief A simple memory watcher that allows preallocation and checks that the
 * heap will not be expanded.
 * @author Mattia Nicolella
 *
 * @copyright (C) 2021 - 2022, Mattia Nicolella <mnico@bu.edu> and the rt-bench
 * contributors. SPDX-License-Identifier: MIT
 */
#ifndef MEMORY_WATCHER_H
#define MEMORY_WATCHER_H

#include "optional_features.h"
#include <stdlib.h>

/** @brief Initializes the memory watcher and preallocates the necessary memory.
 * @param[in] heap_size The amount of memory that must be preallocated.
 * @param[in] heap_start The address of the heap start. `NULL` when glibc is
 * free to decide.
 */
void start_memory_watcher(size_t heap_size, void *heap_start);

///@brief Stops the memory watcher.
void stop_memory_watcher();
#endif

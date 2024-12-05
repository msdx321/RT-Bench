/** @file performance_sampler.h
 * @ingroup generator
 * @brief Functions exported by the performance sampler.
 * @author Denis Hoornaert
 *
 * @copyright (C) 2021 - 2022, Denis Hoornaert <denis.hoornaert@tum.de> and the
 * rt-bench contributors. SPDX-License-Identifier: MIT
 */

#ifndef PERFORMANCE_SAMPLER_H
#define PERFORMANCE_SAMPLER_H

#include "optional_features.h"
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "performance_counters.h"
#include <sched.h>
#include <stdio.h>

/// 1 KB in bytes
#define KB 1024
/// 1 MB in bytes
#define MB KB *KB

/// Struct used to hold data sampled from the counters.
struct sampling_data {
  long unsigned samples;    ///< The amount of measurements recorded.
  struct perf_counters sum; ///< Sum of measurement recorded.
};

int setup_perf_sampler(unsigned iterations, cpu_set_t core_affinity,
                       long unsigned time_bucket);

/** @brief Deallocate resources for the sampler. Assumes stop has been performed
 before.
 @returns 0 on errors
 */
int teardown_perf_sampler(void);

/// @brief Start sampling.
void start_sampling(void);

/// @brief Stop sampling.
void stop_sampling(void);
/// @brief Log sampling data to file.
void log_samples(FILE *filep);

#endif /* PERFORMANCE_SAMPLER */
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

/** @brief Setup the perf sampler.
 * @param[in] core_affinity The affinity mask for the sampler thread.
 * @param[in] time_bucket The period in nanoseconds, after which a new sample must be
 * taken.
 * @details
 * The function will setup the sampler thread and start it, but it will not
 * activate the sampling.
 */
int setup_perf_sampler(cpu_set_t core_affinity,
		       long unsigned time_bucket);

/** @brief Teardown the parf sampler
 * @details Assumes stop has been performed before
 * @returns 0 on success
 */
int teardown_perf_sampler(void);

/// Start the perf sampler
void start_sampling(void);

/// Pause the perf sampler
void stop_sampling(void);

/** @brief export all samples to the log file
 * @param[in] filep The file where to write all samples
 */
void log_samples(FILE *filep);

#endif /* PERFORMANCE_SAMPLER */
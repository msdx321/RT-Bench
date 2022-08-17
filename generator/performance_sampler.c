/** @file performance_sampler.c
 * @ingroup generator
 * @brief Performance sampler implementation.
 * @author Denis Hoornaert
 *
 * @copyright (C) 2021 - 2022, Denis Hoornaert <denis.hoornaert@tum.de> and the rt-bench contributors.
 * SPDX-License-Identifier: MIT
 */
#define _GNU_SOURCE

#include "performance_sampler.h"
#include "performance_counters.h"
#include <semaphore.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>


#define NANOSECONDS  (1UL)
#define MICROSECONDS (1000*NANOSECONDS)
#define MILLISECONDS (1000*MICROSECONDS)
#define SECONDS      (1000*MILLISECONDS)
#define MINUTES      (60*SECONDS)


static pthread_t sampler_thread;
static pthread_attr_t attr;
static struct sched_param params;

static sem_t sampler_sync;


static unsigned sampling_alive = 0;

static unsigned sampling_active = 0;

static struct timespec time_bucket;
static struct timespec rem;


static unsigned long first_l2_refills_sample;
static unsigned sampling_counter;
static unsigned expected_samples;
static struct sampling_data* sampling_data;


static void* sampling(void* dummy)
{
	while (sampling_alive) {
		if (sampling_active) {
			if (sampling_counter == 0) {
				first_l2_refills_sample = pmcs_get_value().l2_refills;
				sampling_data[sampling_counter].min = 0;
				sampling_data[sampling_counter].sum = 0;
				sampling_data[sampling_counter].max = 0;
			}
			else {
				long unsigned diff = pmcs_get_value().l2_refills-first_l2_refills_sample;
				sampling_data[sampling_counter].min = (sampling_data[sampling_counter].min >= diff)? diff : sampling_data[sampling_counter].min;
				sampling_data[sampling_counter].sum += diff;
				sampling_data[sampling_counter].max = (sampling_data[sampling_counter].max <= diff)? diff : sampling_data[sampling_counter].max;
			}
			sampling_data[sampling_counter].samples++;
		}
		sampling_counter++;
		nanosleep(&time_bucket, &rem);
	}
}


int setup_perf_sampler(unsigned iterations, cpu_set_t core_affinity, long unsigned input_time_bucket)
{
	expected_samples = iterations;
	sampling_alive = 1;
	time_bucket.tv_sec = 0;
	time_bucket.tv_nsec = input_time_bucket;
	// setup sampling struct
	sampling_data = (struct sampling_data*)malloc(5*SECONDS/input_time_bucket*sizeof(struct sampling_data));
	// @todo: memset the array
	for (size_t i = 0; i < 5*SECONDS/input_time_bucket; i++)
		sampling_data[i].min = ULONG_MAX;
	// pthread_attr
	int res = pthread_attr_init(&attr);
	if (res != 0) {
		return res;
	}
	res = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	if (res != 0) {
		return res;
	}
	res = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
	if (res != 0) {
		return res;
	}
	res = pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
	if (res != 0) {
		return res;
	}
	params.sched_priority = 51;
	res = pthread_attr_setschedparam(&attr, &params);
	if (res != 0) {
		return res;
	}
	// If no core specified (i.e., core affinity == 0), then inherit from parent thread (i.e., skip setaffinity)
	if (CPU_COUNT(&core_affinity) > 0) {
		res = pthread_attr_setaffinity_np(&attr, sizeof(core_affinity), &core_affinity);
		if (res != 0) {
			return res;
		}
	}
	// Start thread
	res = pthread_create(&sampler_thread, &attr, sampling, NULL);
	return res;

}

/// Assumes stop has been performed before
/// Returns 0 on success
int teardown_perf_sampler(void)
{
	sampling_alive = 0;
	int res = pthread_join(sampler_thread, NULL);
	free(sampling_data);
	return res;
}

void start_sampling(void)
{
	sampling_active = 1;
	sampling_counter = 0;
}

void stop_sampling(void)
{
	sampling_active = 0;
	sampling_counter = 0;
}

void log_samples(FILE* filep)
{
	// Print header
	fprintf(filep, "samples, sum, min, max\n");
	// @todo and max allocated slots
	for (int j = 0; sampling_data[j].samples > 0; j++) {
		fprintf(filep, "%lu, %lu, %lu, %lu\n", sampling_data[j].samples, sampling_data[j].sum, sampling_data[j].min, sampling_data[j].max);
	}
}

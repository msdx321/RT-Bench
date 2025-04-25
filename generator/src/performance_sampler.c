/** @file performance_sampler.c
 * @ingroup generator
 * @brief Performance sampler implementation.
 * @author Denis Hoornaert
 *
 * @copyright (C) 2021 - 2022, Denis Hoornaert <denis.hoornaert@tum.de> and the
 * rt-bench contributors. SPDX-License-Identifier: MIT
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#include <string.h>
#endif
#include "performance_counters.h"
#include "performance_sampler.h"
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <time.h>

#define NANOSECONDS (1UL)
#define MICROSECONDS (1000 * NANOSECONDS)
#define MILLISECONDS (1000 * MICROSECONDS)
#define SECONDS (1000 * MILLISECONDS)
#define MINUTES (60 * SECONDS)

/// The generic item in a list of sampled data buffers
struct sampler_list_item {
  struct sampling_data *data;     ///< The sampled data
  struct sampler_list_item *next; ///< The next element in the list
};

/// The sampler thread ID
static pthread_t sampler_thread;
/// The sampler thread ID attribute
static pthread_attr_t attr;
/// Scheduling paramters for the sampler thread
static struct sched_param params;

/// `1` if the sampling thread is alive, `0` otherwise
static unsigned sampling_alive = 0;

/// `1` if the sampling is active, `0` otherwise
static unsigned sampling_active = 0;

/// The sampling time bucket
static struct timespec time_bucket;
static struct timespec rem;

/// The first sample, used to normalize the successive ones
static struct perf_counters first_sample;
static unsigned sampling_counter;
static unsigned expected_samples;

static struct sampling_list_item
    *sampling_list_head = NULL,    ///< The head of the list of sampled items
    *sampling_data_current = NULL; ///< The current item in the list begin used.

/** @brief Create a new sampler item and add it to the tail of the list
 * @param[in] tail The tail of the list
 * @returns The new list item or `NULL` on error.
 * @details If tail if `NULL` the new item will only be returned.
 */
static struct sampler_list_item *
sampling_list_add(struct sampler_list_item *tail) {
  struct sampler_list_item *new = malloc(sizeof(struct sampler_list_item));
  if (new != NULL) {
    new->next = NULL;
    new->data = malloc(5 * SECONDS / time_bucket.tv_nsec *
                       sizeof(struct sampling_data));
    if (new->data == NULL) {
      free(new);
      new = NULL;
    }
    memset(new->data, 0,
           5 * SECONDS / time_bucket.tv_nsec * sizeof(struct sampling_data));
    if (tail != NULL) {
      tail->next = new;
    }
  }
  return new;
}

static void *sampling(void *dummy) {
  while (sampling_alive) {
    if (sampling_active) {
      if (sampling_counter == 0) {
        first_sample = pmcs_get_value();
        sampling_data[sampling_counter].sum.l1_references = 0;
        sampling_data[sampling_counter].sum.l1_refills = 0;
        sampling_data[sampling_counter].sum.l2_references = 0;
        sampling_data[sampling_counter].sum.l2_refills = 0;
        sampling_data[sampling_counter].sum.inst_retired = 0;
      } else {
        struct perf_counters diff = pmcs_get_value();
        sampling_data[sampling_counter].sum.l1_references +=
            diff.l1_references - first_sample.l1_references;
        sampling_data[sampling_counter].sum.l1_refills +=
            diff.l1_refills - first_sample.l1_refills;
        sampling_data[sampling_counter].sum.l2_references +=
            diff.l2_references - first_sample.l2_references;
        sampling_data[sampling_counter].sum.l2_refills +=
            diff.l2_refills - first_sample.l2_refills;
        sampling_data[sampling_counter].sum.inst_retired +=
            diff.inst_retired - first_sample.inst_retired;
      }
      sampling_data[sampling_counter].samples++;
    }
    sampling_counter++;
    nanosleep(&time_bucket, &rem);
  }
  pthread_exit(NULL);
}

int setup_perf_sampler(unsigned iterations, cpu_set_t core_affinity,
                       long unsigned input_time_bucket) {
  expected_samples = iterations;
  sampling_alive = 1;
  time_bucket.tv_sec = 0;
  time_bucket.tv_nsec = input_time_bucket;
  // setup sampling struct
  sampling_list_head = sampling_list_add(NULL);
  sampling_data_current = sampling_list_head;
  // @todo: memset the array
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
  // If no core specified (i.e., core affinity == 0), then inherit from parent
  // thread (i.e., skip setaffinity)
  if (CPU_COUNT(&core_affinity) > 0) {
    res = pthread_attr_setaffinity_np(&attr, sizeof(core_affinity),
                                      &core_affinity);
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
int teardown_perf_sampler(void) {
  sampling_alive = 0;
  int res = pthread_join(sampler_thread, NULL);
  free(sampling_data);
  return res;
}

void start_sampling(void) {
  sampling_active = 1;
  sampling_counter = 0;
}

void stop_sampling(void) {
  sampling_active = 0;
  sampling_counter = 0;
}

void log_samples(FILE *filep) {
  // Print header
  fprintf(filep, "samples,l1_references,l1_refills,l2_references,l2_refills,"
                 "inst_retired\n");
  // @todo and max allocated slots
  for (int j = 0; sampling_data[j].samples > 0; j++) {
    fprintf(filep, "%lu, %lu, %lu, %lu, %lu, %lu\n", sampling_data[j].samples,
            sampling_data[j].sum.l1_references, sampling_data[j].sum.l1_refills,
            sampling_data[j].sum.l2_references, sampling_data[j].sum.l2_refills,
            sampling_data[j].sum.inst_retired);
  }
}

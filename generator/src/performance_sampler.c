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
#endif

#include "logging.h"
#include "performance_counters.h"
#include "performance_sampler.h"
#include "signal_utils.h"
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define NANOSECONDS (1UL)
#define MICROSECONDS (1000 * NANOSECONDS)
#define MILLISECONDS (1000 * MICROSECONDS)
#define SECONDS (1000 * MILLISECONDS)
#define MINUTES (60 * SECONDS)

/// The generic item in a list of sampled data buffers
struct sampler_list_item {
  unsigned int data_len;          /// The length of the data buffer
  struct sampling_data *data;     ///< The sampled data
  struct sampler_list_item *next; ///< The next element in the list
};

/// The real malloc used by `sampler_list_add()`
extern void *__real_malloc(size_t size);

/// The sampler thread ID
static pthread_t sampler_thread;

/// `1` if the sampling thread is alive, `0` otherwise
volatile static unsigned sampling_alive = 0;

/// `1` if the sampling is active, `0` otherwise
volatile static unsigned sampling_active = 0;

/// The first sample, used to normalize the successive ones
volatile static struct perf_counters first_sample;
/// `1` if the first sample was already taken, `0` otherwise
volatile static unsigned first_sample_taken = 0;

static struct sampler_list_item
    *sampling_list_head = NULL,    ///< The head of the list of sampled items
    *sampling_data_current = NULL; ///< The current item in the list begin used.

/// Timer used to periodically take perf samples.
timer_t perf_timer;
/// SET os fisngal where the samplingthread will wait
sigset_t thread_sigwait_set;

/** @brief Create a new sampler item and add it to the tail of the list
 * @param[in] tail The tail of the list
 * @param[in] sample_count How manu samples the sampler item should fit.
 * will double the
 * @returns The new list item or `NULL` on error.
 * @details If tail is `NULL` the new item will only be returned.
 * The amount of memory allocated depends on the tail and sample_count
 * parameters:
 *  - `sample_count == 0` and `tail != NULL` allocate double size of the
 * previous sampler buffer.
 *  - `sample_count == 0` and `tail == NULL` allocate one sampler item.
 *  - `sample_count != 0` allocate a  buffer of `sample_count` length.
 *
 * This function will be called during execution, so it will reclaim memory
 * using `__real_malloc` to avoid stealing memory from the heap dedicated to the
 * benchmark. Additionally, to limit the number of allocation during execution,
 * the size of the buffer with be doubled for each new list element.
 */
static struct sampler_list_item *
sampling_list_add(struct sampler_list_item *tail, unsigned int sample_count) {
  int buffer_len;
  if (sample_count == 0) {
    if (tail != NULL) {
      buffer_len = tail->data_len * 2;
    } else {
      buffer_len = 1;
    }
  } else {
    buffer_len = sample_count;
  }
  // allocate a contiguous region for both list item struct and buffer
  struct sampler_list_item *new =
      __real_malloc(sizeof(struct sampler_list_item) +
                    (buffer_len * sizeof(struct sampling_data)));
  elogf(LOG_LEVEL_DEBUG, "new VA: %p size of sampler_list_item 0x%lx\n", new,sizeof(struct sampler_list_item));
  if (new != NULL) {
    new->next = NULL;
    new->data_len = buffer_len;
    // move right past the sampler_list_item struct and cast to sampling_data
    new->data = (struct sampling_data *)(new + 1);
    elogf(LOG_LEVEL_DEBUG, "new->data VA: %p\n", new->data);
    memset(new->data, 0, new->data_len * sizeof(struct sampling_data));
    if (tail != NULL) {
      tail->next = new;
    }
  }
  return new;
}

/** @brief The thread function that will periodically take perf samples.
 * @param[in] dummy Ignored.
 * @details
 * The function will also add other elements to the sample list if the current
 * buffer gets full.
 * @returns NULL
 */
static void *sampling(void *dummy) {
  int sampling_counter = 0;
	int sig;
  while (sampling_alive) {
    if (sampling_active) {
      if (first_sample_taken == 0) {
        first_sample_taken = 1;
        first_sample = pmcs_get_value();
        sampling_data_current->data[sampling_counter].sum.l1_references = 0;
        sampling_data_current->data[sampling_counter].sum.l1_refills = 0;
        sampling_data_current->data[sampling_counter].sum.l2_references = 0;
        sampling_data_current->data[sampling_counter].sum.l2_refills = 0;
        sampling_data_current->data[sampling_counter].sum.inst_retired = 0;
      } else {
        struct perf_counters diff = pmcs_get_value();
        sampling_data_current->data[sampling_counter].sum.l1_references +=
            diff.l1_references - first_sample.l1_references;
        sampling_data_current->data[sampling_counter].sum.l1_refills +=
            diff.l1_refills - first_sample.l1_refills;
        sampling_data_current->data[sampling_counter].sum.l2_references +=
            diff.l2_references - first_sample.l2_references;
        sampling_data_current->data[sampling_counter].sum.l2_refills +=
            diff.l2_refills - first_sample.l2_refills;
        sampling_data_current->data[sampling_counter].sum.inst_retired +=
            diff.inst_retired - first_sample.inst_retired;
      }
      sampling_data_current->data[sampling_counter].samples++;
      sampling_counter++;
    }
    if (sampling_counter == sampling_data_current->data_len) {
      sampling_data_current = sampling_list_add(sampling_data_current, 0);
      sampling_counter = 0;
    }
		sigwait(&thread_sigwait_set,&sig);
  }
  pthread_exit(NULL);
}

int setup_perf_sampler(cpu_set_t core_affinity,
                       long unsigned time_bucket) {
  pthread_attr_t attr;
  struct sched_param params;
  sigset_t blocked_set;
  // setup sampling struct
  sampling_list_head =
      sampling_list_add(NULL, 5 * SECONDS / time_bucket);
  sampling_data_current = sampling_list_head;
  if (sampling_list_head == NULL) {
    return -EXIT_FAILURE;
  }
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
  elogf(LOG_LEVEL_DEBUG, "Setting up signal mask for sampler thread\n");
  // add SIGNAL_PERF_SAMPLE, to the set the thread will wait on
  res=sigemptyset(&thread_sigwait_set);
  if (res < 0) {
    return res;
  }
  res = sigaddset(&thread_sigwait_set, SIGNAL_PERF_SAMPLE);
  if (res < 0) {
    return res;
  }
  // block SIGNAL_PERF_SAMPLE, since the dedicated thread will handle it
  elogf(LOG_LEVEL_DEBUG, "Blocking thread signal for main process\n");
  res=sigemptyset(&blocked_set);
  if (res < 0) {
    return res;
  }
  res = sigaddset(&blocked_set, SIGNAL_PERF_SAMPLE);
  if (res < 0) {
    return res;
  }
  res = sigprocmask(SIG_BLOCK, &blocked_set, NULL);
  if (res < 0) {
    return res;
  }
  elogf(LOG_LEVEL_DEBUG, "Creating timer for sampling thread\n");
	// create the timer that will periodically generate SIGNAL_PERF_SAMPLE
	res=setup_timer(&perf_timer, SIGNAL_PERF_SAMPLE, 0, time_bucket);
  if (res < 0) {
    return res;
  }
  sampling_alive = 1;
  elogf(LOG_LEVEL_DEBUG, "Spawning sampler thread\n");
  // Start thread
  res = pthread_create(&sampler_thread, &attr, sampling, NULL);
  if(res!=0){
    errno=res;
  }
  return res;
}

int teardown_perf_sampler(void) {
	//user ret and res to make sure that if one of timer_delete / pthread_join fails we report the failure
  int res,ret=0;
  struct sampler_list_item *current = sampling_list_head, *next = NULL;
  if (sampling_alive == 1) {
  res=timer_delete(perf_timer);
  ret = res;
    // send a last signal, so thread can terminate
    pthread_kill(sampler_thread, SIGNAL_PERF_SAMPLE);
  }
  sampling_alive = 0;
  res = pthread_join(sampler_thread, NULL);
	// invalid thread id will not generate an error
  if (res < 0 && errno != ESRCH) {
		ret=res;
  }
  elogf(LOG_LEVEL_DEBUG,"Joined with sampler thread\n");
  while (current != NULL) {
    next = current->next;
    free(current);
    current = next;
  }
  elogf(LOG_LEVEL_DEBUG,"Freed samples linked list\n");
  return ret;
}

void start_sampling(void) {
  sampling_active = 1;
  first_sample_taken = 0;
}

void stop_sampling(void) {
  sampling_active = 0;
  first_sample_taken = 0;
}

void log_samples(FILE *filep) {
  // Print header
  fprintf(filep, "samples,l1_references,l1_refills,l2_references,l2_refills,"
                 "inst_retired\n");
  struct sampler_list_item *current = sampling_list_head;
  while (current != NULL) {
    // we want to navigate all the buffer but avoiding samples ith 0 values
    // (which imply the bufer has still free space)
    for (int j = 0; j < current->data_len && current->data[j].samples > 0 ;
         j++) {
      fprintf(
          filep, "%lu, %lu, %lu, %lu, %lu, %lu\n", current->data[j].samples,
          current->data[j].sum.l1_references, current->data[j].sum.l1_refills,
          current->data[j].sum.l2_references, current->data[j].sum.l2_refills,
          current->data[j].sum.inst_retired);
    }
    current = current->next;
  }
}

/**
 * @file bandwidth.c
 * @ingroup bandwidth
 * @brief RT-Bench-compatible bandwidth benchmark.
 * @author Heechul Yun <heechul@illinois.edu>, Zheng <zpwu@uwaterloo.ca>
 * @copyright (C) 2012 This file is distributed under the University of Illinois
 * Open Source License. See LICENSE.TXT for details.
 * @details
 * The original script has been broken down in three components:
 * - init: benchmark_init();
 * - execution: benchmark_execution();
 * - teardown: benchmark_teardown();
 *
 * This allows the benchmark to be run periodically, by re-running only the
 * execution portion.
 *
 */

/* clang -S -mllvm --x86-asm-syntax=intel ./bandwidth.c */

/**************************************************************************
 * Conditional Compilation Options
 **************************************************************************/

/**************************************************************************
 * Included Files
 **************************************************************************/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* See feature_test_macros(7) */
#endif
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

// Libraries used by rt-bench
#include "logging.h"
#include "optional_features.h"
#include "periodic_benchmark.h"

/**************************************************************************
 * Public Definitions
 **************************************************************************/
#define CACHE_LINE_SIZE 64 /** cache Line size is 64 byte */
#ifdef __arm__
#define DEFAULT_ALLOC_SIZE_KB 4096
#else
#define DEFAULT_ALLOC_SIZE_KB 16384
#endif

/**************************************************************************
 * Public Types
 **************************************************************************/
/// Type of memory access to perform.
enum access_type { READ, WRITE };

/**************************************************************************
 * Global Variables
 **************************************************************************/
int g_mem_size = DEFAULT_ALLOC_SIZE_KB * 1024; /** Memory size */
int *g_mem_ptr = 0; /** Pointer to allocated memory region */

volatile uint64_t g_nread = 0; /** Number of bytes read */
volatile unsigned int g_start; /** Starting time */
volatile unsigned int g_end;
/// Memory access type
int acc_type = READ;
/// Number of iterations
int iterations = 5;
/// sum of the amount of read/written memory.
int64_t sum = 0;
/// The computed memory bandwidth reported as an extra measurement
double bandwidth=0;
/**************************************************************************
 * Public Functions
 **************************************************************************/
/** @brief Get timestamp in microseconds.
 * @returns Timestamp in microseconds.
 * */
unsigned int get_usecs() {
  struct timeval time;
  gettimeofday(&time, NULL);
  return (time.tv_sec * 1000000 + time.tv_usec);
}

/** @brief Print bandwidth stats.
 * @param[in] param Unused.
 * @details Function unused.
 * */
void print_bandwidth(int param) {
  float dur_in_sec;
  float bw;
  float dur = get_usecs() - g_start;
  dur_in_sec = (float)dur / 1000000.0f;
  flogf(LOG_LEVEL_FILE, log_filep, "g_nread(bytes read) = %lld\n",
        (long long)g_nread);
  flogf(LOG_LEVEL_FILE, log_filep, "elapsed = %.2f sec ( %.0f usec )\n",
        dur_in_sec, dur);
  bw = (float)g_nread / dur_in_sec / 1024.0f / 1024.0f;
  flogf(LOG_LEVEL_FILE, log_filep, "B/W = %.2f MB/s | ", bw);
  flogf(LOG_LEVEL_FILE, log_filep, "average = %.2f ns\n\n",
        (dur * 1000) / ((float)g_nread / CACHE_LINE_SIZE));
}

/** @brief Read memory access.
 * @returns Amount of memory read.
 */
int64_t bench_read() {
  int i;
  int64_t sum = 0;
  for (i = 0; i < g_mem_size / 4; i += (CACHE_LINE_SIZE / 4)) {
    sum += g_mem_ptr[i];
  }
  g_nread += g_mem_size;
  return sum;
}

/** @brief Write memory access.
 * @returns Amount of memory read.
 */
int bench_write() {
  register int i;
  for (i = 0; i < g_mem_size / 4; i += (CACHE_LINE_SIZE / 4)) {
    g_mem_ptr[i] = i;
  }
  g_nread += g_mem_size;
  return 1;
}

/** @brief Print usage info.
 * @param[in] argc arguments number.
 * @param[in] argv Arguments array.
 * */
void usage(int argc, char *argv[]) {
  ologf(LOG_LEVEL_INFO,"Usage: $ %s [<option>]*\n\n", argv[0]);
  ologf(LOG_LEVEL_INFO,"-m: memory size in KB. deafult=8192\n");
  ologf(LOG_LEVEL_INFO,"-a: access type - read, write. default=read\n");
  ologf(LOG_LEVEL_INFO,"-i: iterations. default=5\n");
  ologf(LOG_LEVEL_INFO,"-h: help\n");
  ologf(LOG_LEVEL_INFO,"\nExamples: \n$ bandwidth -m 8192 -a read -i 1  <- 8MB read "
         ",1 one iteration.\n");
  exit(1);
}

/**
 * @brief Will interpret the benchmark parameters and initialize the testbed.
 * @param[in] parameters_num Number of parameters passed, should be 1.
 * @param[in] parameters The list of passed parameters.
 * @details
 * The required parameters array is documented in `usage()`, and can be brought
 * up by having "-h" in `parameters`.
 * @returns `0` on success, `-1` on error, setting errno.
 */
int benchmark_init(int parameters_num, void **parameters) {
  int opt;
  int i;

  /*
   * get command line options
   */
  // adjust parameters list to have a dummy argument at position 0 (to fool
  // getopt)
  int opt_num = parameters_num + 1;
  ologf(LOG_LEVEL_TRACE,"allocatin parameters\n");
  char **opts = malloc(sizeof(char *) * opt_num);
  ologf(LOG_LEVEL_TRACE,"malloc done\n");
  if (opts == NULL) {
    elogf(LOG_LEVEL_ERR, "Failed to allocate memory\n");
    return -1;
  }
  ologf(LOG_LEVEL_TRACE,"copying parameters\n");
  opts[0] = "bandwidth";
  ologf(LOG_LEVEL_TRACE,"parameters_num %d\n", parameters_num);
  ologf(LOG_LEVEL_TRACE,"opt_num %d\n", opt_num);
  for (i = 0; i < parameters_num; i++) {
    ologf(LOG_LEVEL_TRACE,"parameters[%d]: %p\n", i, parameters[i]);
		opts[i + 1] = parameters[i];
    ologf(LOG_LEVEL_TRACE,"%p=opt[%d]: %p\n", opts+i+1, i+1, opts[i+1]);
  }
  for (i = 0; i < opt_num; i++) {
    ologf(LOG_LEVEL_TRACE,"%p=opt[%d]: %p\n", opts+i,i, opts[i]);
  }
  ologf(LOG_LEVEL_TRACE,"starting to parse opts\n");
  while ((opt = getopt(opt_num, opts, "m:a:t:i:h")) != -1) {
    ologf(LOG_LEVEL_TRACE,"case %c, optarg %s\n", opt, optarg);
    switch (opt) {
    case 'm': /* set memory size */
      g_mem_size = 1024 * strtol(optarg, NULL, 0);
      break;
    case 'a': /* set access type */
      if (!strcmp(optarg, "read"))
        acc_type = READ;
      else if (!strcmp(optarg, "write"))
        acc_type = WRITE;
      else {
        errno = EINVAL;
        return -1;
      }
      break;
    case 'i': /* iterations */
      iterations = strtol(optarg, NULL, 0);
      break;
    case 'h':
      usage(opt_num, opts);
      return 0;
      break;
    default:
			free(opts);
      return -1;
			break;
    }
  }
  free(opts);
  /*
   * allocate contiguous region of memory
   */
  g_mem_ptr = (int *)malloc(g_mem_size);
  if (g_mem_ptr == NULL) {
    elogf(LOG_LEVEL_ERR, "Failed to allocate memory\n");
    return -1;
  }
  memset((char *)g_mem_ptr, 1, g_mem_size);

  for (i = 0; i < g_mem_size / sizeof(int); i++)
    g_mem_ptr[i] = i;

  // setup extra measurement struct
  extra_measurement.num_elements = 1;
  extra_measurement.header = "bandwidth(MB/S)";
  extra_measurement.data = &bandwidth;

  /* print experiment info before starting */
  flogf(LOG_LEVEL_FILE, log_filep, "memsize=%d KB, type=%s\n",
        g_mem_size / 1024, ((acc_type == READ) ? "read" : "write"));
  flogf(LOG_LEVEL_FILE, log_filep, "stop at %d iterations\n", iterations);
  flogf(LOG_LEVEL_FILE, log_filep, "buffer VA: %p\n", g_mem_ptr);
  return 0;
}

/**
 * @brief This handler is where the memory bandwidth will be computed.
 * @param[in] parameters_num Number of passed parameters, ignored.
 * @param[in] parameters The list of passed parameters, ignored.
 * @details
 * This function will make several memory accesses (defined by `::iterations`,
 * then it will call `quit()` to compute and report the bandwidth.
 */
void benchmark_execution(int parameters_num, void **parameters) {
  int i = 0;
  /*
   * actual memory access
   */
  g_start = get_usecs();
  g_nread = 0;
  for (i = 0;; i++) {
    switch (acc_type) {
    case READ:
      sum += bench_read();
      break;
    case WRITE:
      sum += bench_write();
      break;
    }

    if (iterations > 0 && i + 1 >= iterations)
      break;
  }
  g_end = get_usecs();
}

/**
 * @brief This handler returns the bandwidth as the extra measurement.
 * @details
 * This function returns the experienced bandwidth (MBps) as a float after
 * each execution phase.
 */
void benchmark_log_data(void) {
  double dur = g_end - g_start;
  double dur_in_sec = (double)dur / 1000000.0f;
  *extra_measurement.data = (double)g_nread / dur_in_sec / 1024.0f / 1024.0f;
	extra_measurement.status=EXTRA_MEASUREMENT_VALID;
}

/**
 * @brief Will revert what `benchmark_init()` has done to initialize the
 * benchmark.
 * @param[in] parameters_num Ignored.
 * @param[in] parameters Ignored.
 * @details It will free `::g_mem_ptr`.
 */
void benchmark_teardown(int parameters_num, void **parameters) {
  if (g_mem_ptr != NULL) {
    free(g_mem_ptr);
  }
}
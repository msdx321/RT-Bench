/** @file logging.c
 * @ingroup generator
 * @brief Implementation of the logging facilities in logging.h.
 * @author Mattia Nicolella
 */

#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/// The file pointer to the log output file.
FILE *log_filep = NULL;

/// Default output path and filename for log information.
#define DEFAULT_LOG_OUTPUT_PATH "./benchmark.log"

/** @details
 * The benchmark verbosity should be initialized only in during the benchmark
 * startup. It is made available as a global variable since every time the
 * logging macro is invoked, this variable must be checked to determine if the
 * message has to be printed. The default log level is ::LOG_LEVEL_INFO.
 *
 * @copyright (C) 2021 - 2022, Mattia Nicolella <mnico@bu.edu> and the rt-bench
 * contributors. SPDX-License-Identifier: MIT
 */
enum log_level benchmark_verbosity = LOG_LEVEL_INFO;

/// Extra benchmark dependant measured data.
struct benchmark_extra_data extra_measurement = {0};

/** @brief Reports the benchmark timing depending on the chosen logging level.
 * @param[in] file The file where the timing will be printed if the logging
 * level is set to `::LOG_LEVEL_FILE`.
 * @param[in] period_start The timestamp, in seconds, when the period started.
 * @param[in] period_end The timestamp, in seconds, when the period completed.
 * @param[in] job_end The timestamp, in seconds, when the job ended.
 * @param[in] deadline The timestamp, in seconds, of the first deadline since
 * the job started, or the timestamp of the skipped deadline.
 * @param[in] period_start_clocks The timestamp, in clock cycles, when the
 * period started.
 * @param[in] period_end_clocks The timestamp, in clock cycles, when the period
 * completed.
 * @param[in] job_end_clocks The timestamp, in clock cycles,  when the job
 * ended.
 * @param[in] deadline_clocks The timestamp, in clock cycles, of the first
 * deadline since the job started, or the timestamp of the skipped deadline.
 * @details
 * Depending on the chosen log level (`::benchmark_verbosity` value), the
 * benchmark timing can be either be printed in a human-friendly or in a
 * csv-like format. The job is assumed to start when the period starts.\n The
 * verbose output is associated to `::LOG_LEVEL_TRACE`, while the csv-like
 * format is associated to both `::LOG_LEVEL_INFO` and `::LOG_LEVEL_FILE`. When
 * `::LOG_LEVEL_ERR` is set no message will be printed.
 *
 * Both `get_rdtsc()` and `get_timestamp()` are used, to be safe in case only
 * one of these methods is working.
 *
 * The csv-like format is composed by the following elements, which will be
 * printed in the order they are described, separated by commas:
 * 1. period start timestamp (in clock cycles);
 * 2. period end timestamp (in clock cycles);
 * 3. job end timestamp (in clock cycles);
 * 4. timestamp (in clock cycles) of the first deadline since job start, called
 * "job deadline";
 * 5. job elapsed time (in clock cycles);
 * 6. period start (in seconds)
 * 7. period end (in seconds)
 * 8. job end (in seconds)
 * 9. job deadline (in seconds)
 * 10. job elapsed time (in seconds)
 * 11. job deadline status:
 *   - `::DEADLINE_MET` if the job deadline was met.
 *   - `::DEADLINE_MISSED` if the job deadline was missed.
 * 12. job utilization (elapsed / (period end  - period start ));
 * 13. job density (elapsed / (job deadline - period start ));
 *
 * Example for a completed job with missed deadline:
 * `84208125098780,84208125153008,84208125296876,84208125153008,198096,32451.516650,32451.516671,32451.516726,32451.516671,0.000076,0,3.66,3.66`
 *
 * Since jobs that exceed the deadline are not killed, there can be jobs that
 * take multiple deadlines and periods to terminate.\n The skipped deadlines and
 * periods can be reported by setting all the elements described to `0`,
 * excluding only the deadline timestamp or the period start timestamp.\n This
 * report is made by printing a line where all the elements but the deadline or
 * the period start are set to 0.\n Example for a deadline skip:
 * `0,0,0,2154695482719,0,0.000000000,0.000000000,0.000000000,821.040738944,0.000000000,0,0,0`
 */
void print_timing(FILE *file, unsigned long long period_start_clocks,
                  unsigned long long period_end_clocks,
                  unsigned long long job_end_clocks,
                  unsigned long long deadline_clocks, long double period_start,
                  long double period_end, long double job_end,
                  long double deadline) {
  int deadline_status = DEADLINE_MISSED;
  if ((job_end > 0 && job_end <= deadline) &&
      (job_end_clocks > 0 && job_end_clocks <= deadline_clocks)) {
    deadline_status = DEADLINE_MET;
  }
  long double elapsed = 0;
  if (job_end > period_start) {
    elapsed = job_end - period_start;
  }
  unsigned long long elapsed_clocks = 0;
  if (job_end_clocks > period_start_clocks) {
    elapsed_clocks = job_end_clocks - period_start_clocks;
  }
  double utilization = 0;
  if (period_end > period_start) {
    utilization = elapsed / (period_end - period_start);
  }
  double utilization_clocks = 0;
  if (period_end_clocks > period_start_clocks) {
    utilization_clocks =
        (elapsed_clocks + 0.0) / (period_end_clocks - period_start_clocks);
  }
  double density = 0;
  if (deadline > period_start) {
    density = elapsed / (deadline - period_start);
  }
  double density_clocks = 0;
  if (deadline_clocks > period_start_clocks) {
    density_clocks =
        (elapsed_clocks + 0.0) / (deadline_clocks - period_start_clocks);
  }
  double d = density;
  if (density <= 0) {
    d = density_clocks;
  }
  double u = utilization;
  if (utilization <= 0) {
    utilization = utilization_clocks;
  }
  switch (benchmark_verbosity) {
  case LOG_LEVEL_MAX:
  case LOG_LEVEL_DEBUG:
  case LOG_LEVEL_TRACE:
    if (job_end != 0) {
ologf(benchmark_verbosity,"\nJob completed\n");
ologf(benchmark_verbosity,"period start: %llu clock cycles\t %.9Lf seconds \t-\t period "
             "end: %llu clock cycles\t %.9Lf seconds\n",
             period_start_clocks, period_start, period_end_clocks, period_end);
ologf(benchmark_verbosity,"job end: %llu clock cycles\t %.9Lf seconds\n", job_end_clocks,
             job_end);
ologf(benchmark_verbosity,"job deadline: %llu clock cycles\t %.9Lf seconds \t-\tdeadline "
             "status:%d (%d=met)\n",
             deadline_clocks, deadline, deadline_status, DEADLINE_MET);
ologf(benchmark_verbosity,"job duration: %llu clock cycles\t %.9Lf "
             "seconds\t-\tutilization:%.3g\t-\tdensity:%.3g\n\n",
             elapsed_clocks, elapsed, u, d);
    } else {
      if (deadline != 0 || deadline_clocks != 0) {
ologf(benchmark_verbosity,"\n\t Deadline %llu (%.9Lf) skipped\n\n", deadline_clocks,
               deadline);
      }
      if (period_start != 0 || period_start_clocks != 0) {
ologf(benchmark_verbosity,"\n\t Period %llu (%.9Lf) skipped\n\n", period_start_clocks,
               period_start);
      }
    }
    break;
  case LOG_LEVEL_FILE:
    fprintf(
        file,
        "%llu,%llu,%llu,%llu,%llu,%.9Lf,%.9Lf,%.9Lf,%.9Lf,%.9Lf,%d,%.3g,%.3g",
        period_start_clocks, period_end_clocks, job_end_clocks, deadline_clocks,
        elapsed_clocks, period_start, period_end, job_end, deadline, elapsed,
        deadline_status, u, d);
    break;
  case LOG_LEVEL_INFO:
ologf(LOG_LEVEL_INFO,
        "%llu,%llu,%llu,%llu,%llu,%.9Lf,%.9Lf,%.9Lf,%.9Lf,%.9Lf,%d,%.3g,%.3g",
        period_start_clocks, period_end_clocks, job_end_clocks, deadline_clocks,
        elapsed_clocks, period_start, period_end, job_end, deadline, elapsed,
        deadline_status, u, d);
    break;
  case LOG_LEVEL_ERR:
  case LOG_LEVEL_MIN:
    break;
  }
}

void print_performance_counters(
    FILE *file, long unsigned l1_ref_start, long unsigned l1_miss_start,
    long unsigned l2_ref_start, long unsigned l2_miss_start,
    long unsigned inst_retired_start, long unsigned clock_count_start,
    long unsigned l1_ref_end, long unsigned l1_miss_end,
    long unsigned l2_ref_end, long unsigned l2_miss_end,
    long unsigned inst_retired_end, long unsigned clock_count_end) {
  long unsigned job_l1_ref = l1_ref_end - l1_ref_start;
  long unsigned job_l1_miss = l1_miss_end - l1_miss_start;
  long unsigned job_l2_ref = l2_ref_end - l2_ref_start;
  long unsigned job_l2_miss = l2_miss_end - l2_miss_start;
  long unsigned job_inst_retired = inst_retired_end - inst_retired_start;
  long unsigned job_clock_count = clock_count_end - clock_count_start;
  double job_l1_miss_ratio = ((double)job_l1_miss) / ((double)job_l1_ref);
  double job_l2_miss_ratio = ((double)job_l2_miss) / ((double)job_l2_ref);
  switch (benchmark_verbosity) {
  case LOG_LEVEL_MAX:
  case LOG_LEVEL_DEBUG:
  case LOG_LEVEL_TRACE:
ologf(benchmark_verbosity,"\nLevel 1 Data cache\n");
ologf(benchmark_verbosity,"L1-D references/accesses: %lu\n", job_l1_ref);
ologf(benchmark_verbosity,"L1-D refills/misses: %lu\n", job_l1_miss);
ologf(benchmark_verbosity,"L1-D miss ratio (accesses/misses): %f%%\n", job_l1_miss_ratio);
ologf(benchmark_verbosity,"\nLevel 2 Data cache\n");
ologf(benchmark_verbosity,"L2 references/accesses: %lu\n", job_l2_ref);
ologf(benchmark_verbosity,"L2 refills/misses: %lu\n", job_l2_miss);
ologf(benchmark_verbosity,"L2 miss ratio (accesses/misses): %f%%\n", job_l2_miss_ratio);
ologf(benchmark_verbosity,"\nCPU\n");
ologf(benchmark_verbosity,"Instruction retired (i.e., executed in hardware): %lu\n",
           job_inst_retired);
ologf(benchmark_verbosity,"CPU clock-cycles: %lu\n", job_clock_count);
    break;
  case LOG_LEVEL_FILE:
    fprintf(file, ",%lu,%lu,%f,%lu,%lu,%f,%lu,%lu", job_l1_ref, job_l1_miss,
            job_l1_miss_ratio, job_l2_ref, job_l2_miss, job_l2_miss_ratio,
            job_inst_retired, job_clock_count);
    break;
  case LOG_LEVEL_INFO:
ologf(LOG_LEVEL_INFO,",%lu,%lu,%f,%lu,%lu,%f,%lu,%lu", job_l1_ref, job_l1_miss,
           job_l1_miss_ratio, job_l2_ref, job_l2_miss, job_l2_miss_ratio,
           job_inst_retired, job_clock_count);
    break;
  case LOG_LEVEL_ERR:
  case LOG_LEVEL_MIN:
    break;
  }
}

/** @brief Print extra data for benchmarks that have the EXTENDED_REPORT macro
 * defined.
 * @param[in] file The output file where the extra data will be printed.
 */
void print_extra_data(FILE *file) {
  int i;
  switch (benchmark_verbosity) {
  case LOG_LEVEL_MAX:
  case LOG_LEVEL_DEBUG:
  case LOG_LEVEL_TRACE:
    // print the the header extra metrics but skip the first comma
    ologf(benchmark_verbosity,"Extra metrics:\n%s\n", extra_measurement.header + 1);
    for (i = 0; i < extra_measurement.num_elements; i++) {
      ologf(benchmark_verbosity,"%lf ", extra_measurement.data[i]);
    }
    ologf(benchmark_verbosity,"\n");
    break;
  case LOG_LEVEL_FILE:
    for (i = 0; i < extra_measurement.num_elements; i++) {
      fprintf(file, ",%lf", extra_measurement.data[i]);
    }
    break;
  case LOG_LEVEL_INFO:
    for (i = 0; i < extra_measurement.num_elements; i++) {
      ologf(LOG_LEVEL_INFO,",%lf", extra_measurement.data[i]);
    }
    break;
  case LOG_LEVEL_ERR:
  case LOG_LEVEL_MIN:
    break;
  }
}

void print_statistics(FILE *file, unsigned long long period_start_clocks,
                      unsigned long long period_end_clocks,
                      unsigned long long job_end_clocks,
                      unsigned long long deadline_clocks,
                      long double period_start, long double period_end,
                      long double job_end, long double deadline,
                      long unsigned l1_ref_start, long unsigned l1_miss_start,
                      long unsigned l2_ref_start, long unsigned l2_miss_start,
                      long unsigned inst_retired_start,
                      long unsigned clock_count_start, long unsigned l1_ref_end,
                      long unsigned l1_miss_end, long unsigned l2_ref_end,
                      long unsigned l2_miss_end, long unsigned inst_retired_end,
                      long unsigned clock_count_end) {
  print_timing(file, period_start_clocks, period_end_clocks, job_end_clocks,
               deadline_clocks, period_start, period_end, job_end, deadline);
#if defined FEAT_PERF_SUPPORT && FEAT_PERF_SUPPORT == OPT_FEAT_ENABLED
  print_performance_counters(
      file, l1_ref_start, l1_miss_start, l2_ref_start, l2_miss_start,
      inst_retired_start, clock_count_start, l1_ref_end, l1_miss_end,
      l2_ref_end, l2_miss_end, inst_retired_end, clock_count_end);
#endif

#ifdef EXTENDED_REPORT
  if (extra_measurement.status == EXTRA_MEASUREMENT_VALID) {
    print_extra_data(file);
  }
#endif

  switch (benchmark_verbosity) {
  case LOG_LEVEL_FILE:
    fprintf(file, "\n");
    break;
  case LOG_LEVEL_INFO:
ologf(LOG_LEVEL_INFO,"\n");
    break;
  case LOG_LEVEL_MAX:
  case LOG_LEVEL_DEBUG:
  case LOG_LEVEL_TRACE:
  case LOG_LEVEL_ERR:
  case LOG_LEVEL_MIN:
    break;
  }
}

/**
 * @details
 * The filename_prefix is used to create a filename with the following format:
 * `filename_prefixfilename_suffix`. If the filename_prefix is not defined, the
 * default filename will be used.
 * The file is opened in append mode by default.
 */
FILE *open_output_file(char *default_filename, char *filename_prefix,
                       char *filename_suffix, char *mode) {
  size_t suffix_len, fname_len;
  char *fname = NULL;
  FILE *filep = NULL;
  if (filename_prefix != NULL) {
    suffix_len = strlen(filename_suffix) + 1;
    fname_len = strlen(filename_prefix) + suffix_len + 1;
    fname = malloc(sizeof(char) * fname_len);
    memset(fname, 0, sizeof(char) * fname_len);
    snprintf(fname, fname_len, "%s%s", filename_prefix, filename_suffix);
  } else {
    fname_len = strlen(default_filename) + 1;
    fname = malloc(sizeof(char) * fname_len);
    memset(fname, 0, sizeof(char) * fname_len);
    snprintf(fname, fname_len, "%s", default_filename);
  }
  if (mode == NULL) {
    mode = "a";
    elogf(LOG_LEVEL_INFO, "File mode for %s not defined, using append mode\n",
          fname);
  }
  filep = fopen(fname, mode);
  free(fname);
  if (filep == NULL) {
    perror("Error opening file");
  }
  return filep;
}

/** @details
 * Creates a new log file or append to an already existing one, according to the
 * given filename. After the file has been opened/created the timestamp (in ISO
 * 8601) of the current tun will be written.
 * */
FILE *open_log_file(char *filename) {
  FILE *bmark_output =
      open_output_file(DEFAULT_LOG_OUTPUT_PATH, filename, ".log", "a");
  if (bmark_output != NULL) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    fprintf(bmark_output,
          "\n\tNEW RUN AT: %d-%02d-%02dT-%02d:%02d:%02d\n", tm.tm_year + 1900,
          tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  }
  return bmark_output;
}

/** @details
 *  Flushes and closes the log file.
 * */
int close_output_file(FILE *file) {
  int res = 0;
  res = fflush(file);
  if (res != EOF) {
    res = fclose(file);
  }
  return res;
}
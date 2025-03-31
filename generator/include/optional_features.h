/** @file optional_features.h
 * @brief Preprocessor directives to manage RT-Bench optional features.
 * @details __NOTE__: This file has to be included in all header files.
 **/
#ifndef RT_BENCH_OPTIONAL_FEATURES_H
#define RT_BENCH_OPTIONAL_FEATURES_H
/// Value for optional features enabled state.
#define OPT_FEAT_ENABLED 1
/// Value for optional features disabled state.
#define OPT_FEAT_DISABLED 0

// sanity checks for optional features
#if (defined FEAT_JSON_SUPPORT && FEAT_JSON_SUPPORT != OPT_FEAT_ENABLED &&     \
     FEAT_JSON_SUPPORT != OPT_FEAT_DISABLED)
#error "FEAT_JSON_SUPPORT must be OPT_FEAT_ENABLED or OPT_FEAT_DISABLED"
#endif
#if (defined FEAT_SCHED_DEADLINE_SUPPORT &&                                    \
     FEAT_SCHED_DEADLINE_SUPPORT != OPT_FEAT_ENABLED &&                        \
     FEAT_SCHED_DEADLINE_SUPPORT != OPT_FEAT_DISABLED)
#error                                                                         \
    "FEAT_SCHED_DEADLINE_SUPPORT must be OPT_FEAT_ENABLED or OPT_FEAT_DISABLED"
#endif
#if (defined FEAT_PERF_SUPPORT && FEAT_PERF_SUPPORT != OPT_FEAT_ENABLED &&     \
     FEAT_PERF_SUPPORT != OPT_FEAT_DISABLED)
#error "FEAT_PERF_SUPPORT must be OPT_FEAT_ENABLED or OPT_FEAT_DISABLED"
#endif

// check if json-c is available
#if (FEAT_JSON_SUPPORT == OPT_FEAT_ENABLED && defined __has_include &&         \
     __has_include(<json-c/json.h>) )
#include <json-c/json.h>
#if JSON_C_VERSION_NUM >= 0x000f00
/// Indicates whether the json parser is supported.
#define JSON_SUPPORT
#endif
#endif

// tell the user if they are using the extended report version
#if (defined FEAT_EXTENDED_REPORT_SUPPORT &&                                   \
     FEAT_EXTENDED_REPORT_SUPPORT == OPT_FEAT_ENABLED)
#define EXTENDED_REPORT
#endif

// If  SCHED_DEADLINE is not defined we cannot use the deadline scheduler.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
#if (!defined FEAT_SCHED_DEADLINE_SUPPORT && defined SCHED_DEADLINE) ||        \
    FEAT_SCHED_DEADLINE_SUPPORT == OPT_FEAT_ENABLED
/// Indicates whether the deadline scheduler is supported.
#define SCHED_DEADLINE_SUPPORT
#endif

#if defined FEAT_PERF_SUPPORT && FEAT_PERF_SUPPORT == OPT_FEAT_ENABLED &&      \
    !defined CORTEX_A53 && !defined CORE_I7
#error "Unsupported platform for perf counters."
#endif
#if !defined FEAT_PRINT_SKIPPED_DEADLINE_SUPPORT ||                            \
    FEAT_PRINT_SKIPPED_DEADLINE_SUPPORT == OPT_FEAT_ENABLED
/// Indicates whether to print skipped deadelines with 0s.
#define PRINT_SKIPPED_DEADLINE
#endif

#if FEAT_MEM_WATCHER_ALIGN == OPT_FEAT_ENABLED && MEM_WATCHER_ALIGN != 1
#include <stdint.h>
#if MEM_WATCHER_ALIGN == 8
typedef uint8_t mem_watcher_address_t;
#elif MEM_WATCHER_ALIGN == 16
typedef uint16_t mem_watcher_address_t;
#elif MEM_WATCHER_ALIGN == 32
typedef uint32_t mem_watcher_address_t;
#elif MEM_WATCHER_ALIGN == 64
typedef uint64_t mem_watcher_address_t;
#else
#error "Alignment of "MEM_WATCHER_ALIGN" for the memory watcher is unsupported. Supported alignments for memory watcher are: 1, 8, 16, 32 and 64 bytes"
#endif
#else
typedef void mem_watcher_address_t;
#endif

#endif

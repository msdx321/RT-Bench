/**
 * @file signal_utils.h
 * @ingroup generator
 * @brief Utility functions to setup a signal handler and a real-time timer.
 * @details
 * This headera also include the definition of the signals used in the generator.
 * @author Mattia Nicolella
 *
 * @copyright (C) 2021 - 2022, Mattia Nicolella <mnico@bu.edu> and the rt-bench
 * contributors. SPDX-License-Identifier: MIT
 */

#ifndef SIGNAL_UTILS_H
#define SIGNAL_UTILS_H
#include <signal.h>
#include <time.h>

/// The real-time signal that identifies the deadline occurrence.
#define SIGNAL_DEADLINE SIGRTMIN

/// The real-time signal that identifies the end of the period.
#define SIGNAL_END_PERIOD SIGRTMIN + 1

/// The real-time signal that identifies a new perf sample.
#define SIGNAL_PERF_SAMPLE SIGRTMIN + 2

/** @brief A function that creates and arms a real-time timer.
 * @param[out] timer The pointer that will contain the created timer.
 * @param[in] signal_generated The signal that the timer must generated when it
 * expires.
 * @param[in] interval_sec The seconds after which the timer will expire.
 * @param[in] interval_nsec The nanoseconds after which the timer will expire.
 */
int setup_timer(timer_t *timer, int signal_generated, long interval_sec,
                long interval_nsec);

/**
 * @brief A simple function that is used to install a signal handler.
 * @param[in] handled_signal The signal that is to be associated to the handler.
 * @param[in] handler The handler that is to be associated to the signal.
 * @param[in] masked_signals An array, containing the signals that must be
 * masked during the handler execution.
 * @param[in] masked_signals_num The number of element of `masked_signals`.
 */
int setup_signal(int handled_signal, void (*handler)(int, siginfo_t *, void *),
                 int *masked_signals, int masked_signals_num);
#endif

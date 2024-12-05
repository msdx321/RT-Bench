/** @file utils.h
 * @ingroup generator
 * @brief Boilerplate functions.
 * @author Mattia Nicolella
 *
 * @copyright (C) 2021 - 2024, Mattia Nicolella <mnico@bu.edu> and the rt-bench
 * contributors. SPDX-License-Identifier: MIT
 */
#ifndef UTILS_H
#define UTILS_H

#include <sys/types.h>
#include <sys/wait.h>

/** @brief A function that creates and arms a real-time timer.
 * @param[out] timer The pointer that will contain the created timer.
 * @param[in] signal_generated The signal that the timer must generated when
 * it expires.
 * @param[in] interval_sec The seconds after which the timer will expire.
 * @param[in] interval_nsec The nanoseconds after which the timer will expire.
 * @param[in] timer_type type of the timer, 0 for relative, TIMER_ABSTIME for
 * absolute.
 * @returns 0 on success, <0 on failure.
 */
int setup_timer(timer_t *timer, int signal_generated, long interval_sec,
                long interval_nsec, int timer_type);

/**
 * @brief A simple function that is used to install a signal handler.
 * @param[in] handled_signal The signal that is to be associated to the
 * handler.
 * @param[in] handler The handler that is to be associated to the signal.
 * @param[in] masked_signals An array, containing the signals that must be
 * masked during the handler execution.
 * @param[in] masked_signals_num The number of element of `masked_signals`.
 * @returns 0 on success, <0 on failure.
 */
int setup_signal(int handled_signal, void (*handler)(int, siginfo_t *, void *),
                 int *masked_signals, int masked_signals_num);
#endif

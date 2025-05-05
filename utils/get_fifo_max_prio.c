/** @file get_fifo_max_prio.c
 * @ingroup utils
 * @brief a simple C file that gives as output the max supported priority for the FIFO scheduler.
 * @author Mattia Nicolella
 *
 * @copyright (C) 2021 - 2022, Mattia Nicolella <mnico@bu.edu> and the rt-bench contributors.
 * SPDX-License-Identifier: MIT
*/
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
int main(void)
{
	int max_prio = sched_get_priority_max(SCHED_FIFO);
	printf("%d\n", max_prio);
	return ((max_prio == -1) ? EXIT_FAILURE : EXIT_SUCCESS);
}
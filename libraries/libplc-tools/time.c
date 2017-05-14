/**
 * @file
 * 
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

// NOTE: Link with '-lrt' to use 'clock_gettime'
#include <sys/time.h>
#include <time.h>
#include "+common/api/+base.h"
#include "api/time.h"

ATTR_EXTERN uint32_t plc_time_get_tick_ms(void)
{
	// Note: in man 'clock_gettime' it is indicated that for glibc < 2.17 it's necessary to link
	//	with -lrt
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	unsigned int tick = ts.tv_nsec / 1000000;
	tick += ts.tv_sec * 1000;
	return tick;
}

ATTR_EXTERN inline struct timespec plc_time_get_hires_stamp(void)
{
	struct timespec stamp_current;
	int ret = clock_gettime(CLOCK_MONOTONIC, &stamp_current);
	assert(ret != -1);
	return stamp_current;
}

ATTR_EXTERN inline int32_t plc_time_hires_interval_to_usec(struct timespec t1, struct timespec t2)
{
	int32_t lapse_us = (t2.tv_sec - t1.tv_sec) * 1000000L
			+ (int) ((t2.tv_nsec - t1.tv_nsec) / 1000);
	return lapse_us;
}

ATTR_EXTERN inline void plc_time_add_usec_to_hires_interval(struct timespec *t, int32_t interval_us)
{
	t->tv_sec += interval_us / 1000000;
	t->tv_nsec += (interval_us % 1000000) * 1000;
	// Check carry
	if (t->tv_nsec > 1000000000L)
	{
		t->tv_nsec -= 1000000000L;
		t->tv_sec++;
	}
}

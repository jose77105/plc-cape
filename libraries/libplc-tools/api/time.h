/**
 * @file
 * @brief	Time measurement facilities
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef LIBPLC_TOOLS_TIME_H
#define LIBPLC_TOOLS_TIME_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief	Gets the current value of an always running milliseconds counter
 * @return	The milliseconds counter
 * @details	The counter overflows after 2^32 ms = 49.7 days
 */
uint32_t plc_time_get_tick_ms(void);
/**
 * @brief	Gets the current stamp using a high-precision counter (platform-dependent)
 * @return	The stamp in platform-dependent units. Use @ref plc_time_hires_interval_to_usec
 *			to convert it to microseconds
 */
struct timespec plc_time_get_hires_stamp(void);
/**
 * @brief	Converts the interval between two _timespec_ stamps to microseconds
 * @param	t1	Beginning of the interval in _timespec_ units
 * @param	t2	End of the interval in _timespec_ units
 * @return	Interval t2-t1 in microseconds
 */
int32_t plc_time_hires_interval_to_usec(struct timespec t1, struct timespec t2);
/**
 * @brief	Adds an interval of time to a _hires_ stamp
 * @param	t			The _hires_ stamp in _timespec_ units
 * @param	interval_us	The interval to add in microseconds
 */
void plc_time_add_usec_to_hires_interval(struct timespec *t, int32_t interval_us);

#ifdef __cplusplus
}
#endif

#endif /* LIBPLC_TOOLS_TIME_H */

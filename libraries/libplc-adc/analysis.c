/**
 * @file
 * 
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include "+common/api/+base.h"
#include "api/analysis.h"
#include "libraries/libplc-tools/api/time.h"

struct plc_rx_analysis
{
	struct rx_statistics rx_statistics;
	enum plc_rx_statistics_mode statistics_mode;
	struct timespec rx_stamp_cycle;
	float rx_samples_per_bit;
	sample_rx_t data_offset;
	sample_rx_t data_hi_threshold_detection;
};

ATTR_EXTERN struct plc_rx_analysis *plc_rx_analysis_create(void)
{
	struct plc_rx_analysis *plc_rx_analysis = calloc(1, sizeof(struct plc_rx_analysis));
	return plc_rx_analysis;
}

ATTR_EXTERN void plc_rx_analysis_release(struct plc_rx_analysis *plc_rx_analysis)
{
	free(plc_rx_analysis);
}

ATTR_EXTERN void plc_rx_analysis_configure(struct plc_rx_analysis *plc_rx_analysis,
		float freq_adc_sps, uint32_t data_bit_us, sample_rx_t data_offset,
		sample_rx_t data_hi_threshold_detection)
{
	plc_rx_analysis->rx_samples_per_bit = (float) data_bit_us * freq_adc_sps / 1000000.0;
	plc_rx_analysis->data_offset = data_offset;
	plc_rx_analysis->data_hi_threshold_detection = data_hi_threshold_detection;
}

ATTR_EXTERN void plc_rx_analysis_reset(struct plc_rx_analysis *plc_rx_analysis)
{
	memset(&plc_rx_analysis->rx_statistics, 0, sizeof(struct rx_statistics));
	plc_rx_analysis->rx_stamp_cycle = plc_time_get_hires_stamp();
}

ATTR_INTERN void plc_rx_analysis_set_statistics_mode(struct plc_rx_analysis *plc_rx_analysis, 
	enum plc_rx_statistics_mode mode)
{
	plc_rx_analysis->statistics_mode = mode;
}

ATTR_INTERN void plc_rx_analysis_report_rx_statistics_time(struct plc_rx_analysis *plc_rx_analysis,
		uint32_t buffer_preparation_us)
{
	struct rx_statistics *rx_stat = &plc_rx_analysis->rx_statistics;
	rx_stat->buffer_preparation_us = buffer_preparation_us;
	if (rx_stat->buffers_handled <= 2)
	{
		rx_stat->buffer_preparation_min_us = buffer_preparation_us;
		rx_stat->buffer_preparation_max_us = buffer_preparation_us;
	}
	else
	{
		if (buffer_preparation_us < rx_stat->buffer_preparation_min_us)
			rx_stat->buffer_preparation_min_us = buffer_preparation_us;
		if (buffer_preparation_us > rx_stat->buffer_preparation_max_us)
			rx_stat->buffer_preparation_max_us = buffer_preparation_us;
	}
}

// POSTCONDITION: 'rx_statistics' items must be atomic but it's not required for the whole struct
//	(for performance and simplicity)
ATTR_EXTERN const struct rx_statistics *plc_rx_analysis_get_statistics(
		struct plc_rx_analysis *plc_rx_analysis)
{
	return &plc_rx_analysis->rx_statistics;
}

// TODO: Improvements: For performance reasons allow to configure on-the-fly the type of statistics
//	to analyze
ATTR_EXTERN int plc_rx_analysis_analyze_buffer(struct plc_rx_analysis *plc_rx_analysis, 
	sample_rx_t *buffer, uint32_t buffer_samples)
{
	int i;
	struct rx_statistics *rx_stat = &plc_rx_analysis->rx_statistics;
	rx_stat->buffers_handled++;
	// Analyze the samples for valid data detection
	sample_rx_t *buffer_cur = buffer;
	sample_rx_t min = *buffer_cur;
	sample_rx_t max = *buffer_cur;
	uint32_t accum = 0;
	// To use 'uint32_t' safely for 'accum' check that 'buffer_samples' within the limits
	assert(buffer_samples <= 0xFFFF);
	// TODO: Make 'HI_THRESHOLD_DETECTION' this automatic or configurable? Improve the filter
	//	anti-glitches
	uint32_t samples_hi_threshold_detection = (uint32_t) (plc_rx_analysis->rx_samples_per_bit / 4);
	int hi_samples = 0;
	int data_detected = 0;
	for (i = buffer_samples; i > 0; i--, buffer_cur++)
	{
		if (*buffer_cur < min)
			min = *buffer_cur;
		if (*buffer_cur > max)
			max = *buffer_cur;
		accum += *buffer_cur;
		if (*buffer_cur >
				plc_rx_analysis->data_offset + plc_rx_analysis->data_hi_threshold_detection)
		{
			if (++hi_samples == samples_hi_threshold_detection)
			{
				// TODO: Improve 'data_detected' strategy
				data_detected = 1;
			}
		}
	}
	// Calculate time between calls. If not buffers missing it will give the time-per-buffer
	switch(plc_rx_analysis->statistics_mode)
	{
	case plc_rx_statistics_none:
		break;
	case plc_rx_statistics_time:
	{
		struct timespec stamp_cycle_new = plc_time_get_hires_stamp();
		uint32_t buffer_cycle_us = plc_time_hires_interval_to_usec(
			plc_rx_analysis->rx_stamp_cycle, stamp_cycle_new);
		plc_rx_analysis->rx_stamp_cycle = stamp_cycle_new;
		rx_stat->buffer_cycle_us = buffer_cycle_us;
		if (rx_stat->buffers_handled <= 2)
		{
			rx_stat->buffer_cycle_min_us = buffer_cycle_us;
			rx_stat->buffer_cycle_max_us = buffer_cycle_us;
		}
		else
		{
			if (buffer_cycle_us < rx_stat->buffer_cycle_min_us)
				rx_stat->buffer_cycle_min_us = buffer_cycle_us;
			if (buffer_cycle_us > rx_stat->buffer_cycle_max_us)
				rx_stat->buffer_cycle_max_us = buffer_cycle_us;
		}
		break;
	}
	case plc_rx_statistics_values:
	{
		plc_rx_analysis->rx_statistics.buffer_min = min;
		// plc_rx_analysis->rx_statistics.buffer_max = max;
		plc_rx_analysis->rx_statistics.buffer_max = max++;
		float dc_mean = (float) accum / buffer_samples;
		float ac_mean = 0.0;
		buffer_cur = buffer;
		for (i = buffer_samples; i > 0; i--)
			ac_mean += abs((float) (*buffer_cur++) - dc_mean);
		ac_mean /= buffer_samples;
		plc_rx_analysis->rx_statistics.buffer_dc_mean = dc_mean;
		plc_rx_analysis->rx_statistics.buffer_ac_mean = ac_mean;
		break;
	}
	default:
		assert(0);
	}
	return data_detected;
}

/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#define _GNU_SOURCE		// asprintf
#include <math.h>		// ceil
#include <pthread.h>
#include <unistd.h>		// unlink
#include "+common/api/+base.h"
#include "common.h"
#include "decoder.h"
#include "libraries/libplc-adc/api/adc.h"
#include "libraries/libplc-adc/api/analysis.h"
#include "libraries/libplc-cape/api/leds.h"
#include "libraries/libplc-tools/api/application.h"
#include "libraries/libplc-tools/api/file.h"
#include "libraries/libplc-tools/api/time.h"
#include "monitor.h"
#include "rx.h"
#include "settings.h"

#define ADC_FILE_CAPTURE "adc.csv"
#define ADC_FILE_CAPTURE_DATA "adc_data.csv"

#define ADC_BUFFER_COUNT 2048
#define FILE_DATA_SAMPLES 100

struct rx
{
	struct rx_settings settings;
	struct plc_rx_analysis *plc_rx_analysis;
	struct monitor *monitor;
	struct plc_leds *leds;
	struct plc_adc *plc_adc;
	struct decoder *decoder;
	int adc_buffer_samples;
	uint16_t *adc_sample_by_sample_buffer;
	int adc_sample_by_sample_buffer_pos;
	pthread_t thread;
	volatile int end_thread;
	float rx_samples_per_bit;
	uint8_t *buffer_data;
	uint32_t buffer_data_count;
	char *file_rx_path;
	sample_rx_t *buffer_to_file_rx;
	sample_rx_t *buffer_to_file_rx_cur;
	uint32_t buffer_to_file_rx_remaining;
	char *file_data_path;
	uint8_t *buffer_to_file_data;
	uint8_t *buffer_to_file_data_cur;
	uint32_t buffer_to_file_data_remaining;
};

// Enums-to-text mappings
const char *rx_mode_enum_text[rx_mode_COUNT] =
{ "none", "sample_by_sample", "buffer_by_buffer", "kernel_buffering", };

const char *demodulation_mode_enum_text[demod_mode_COUNT] =
{ "none", "real_time", "deferred", };

struct rx *rx_create(const struct rx_settings *settings, struct monitor *monitor, 
		struct plc_leds *leds, struct plc_adc *plc_adc, struct decoder *decoder)
{
	struct rx *rx = calloc(1, sizeof(struct rx));
	rx->settings = *settings;
	rx->plc_rx_analysis = plc_rx_analysis_create();
	rx->monitor = monitor;
	monitor_set_rx_analysis(rx->monitor, rx->plc_rx_analysis);
	rx->leds = leds;
	rx->plc_adc = plc_adc;
	rx->decoder = decoder;
	char *output_dir = plc_application_get_output_abs_dir();
	asprintf(&rx->file_rx_path, "%s/%s", output_dir, ADC_FILE_CAPTURE);
	asprintf(&rx->file_data_path, "%s/%s", output_dir, ADC_FILE_CAPTURE_DATA);
	free(output_dir);
	switch (settings->rx_mode)
	{
	case rx_mode_sample_by_sample:
		rx->adc_buffer_samples = ADC_BUFFER_COUNT;
		rx->adc_sample_by_sample_buffer = (uint16_t *) malloc(
				rx->adc_buffer_samples * sizeof(uint16_t));
		break;
	case rx_mode_buffer_by_buffer:
	case rx_mode_kernel_buffering:
		rx->adc_buffer_samples = ADC_BUFFER_COUNT;
		break;
	default:
		assert(0);
		break;
	}
	return rx;
}

void rx_release(struct rx *rx)
{
	if (rx->adc_sample_by_sample_buffer)
		free(rx->adc_sample_by_sample_buffer);
	free(rx->file_data_path);
	free(rx->file_rx_path);
	plc_rx_analysis_release(rx->plc_rx_analysis);
	free(rx);
}

static void *thread_adc_rx_buffer(void *arg)
{
	struct rx *rx = arg;
	while (!rx->end_thread)
	{
		rx->adc_sample_by_sample_buffer[rx->adc_sample_by_sample_buffer_pos++] =
				plc_adc_read_sample(rx->plc_adc);
		if (rx->adc_sample_by_sample_buffer_pos >= rx->adc_buffer_samples)
			rx->adc_sample_by_sample_buffer_pos = 0;
	}
	return NULL;
}

static int rx_on_buffer_completed(
		struct rx *rx, sample_rx_t *samples_buffer, uint32_t samples_buffer_count)
{
	struct timespec stamp_ini = plc_time_get_hires_stamp();
	int data_detected = plc_rx_analysis_analyze_buffer(rx->plc_rx_analysis, samples_buffer,
			samples_buffer_count);
	monitor_on_buffer_received(rx->monitor, samples_buffer, samples_buffer_count);
	if (data_detected)
	{
		if (rx->buffer_to_file_rx_remaining > 0)
		{
			// Storage in file (if enabled)
			int samples_to_copy =
					(rx->buffer_to_file_rx_remaining <= rx->adc_buffer_samples) ?
							rx->buffer_to_file_rx_remaining : rx->adc_buffer_samples;
			memcpy(rx->buffer_to_file_rx_cur, samples_buffer,
				samples_to_copy * sizeof(sample_rx_t));
			rx->buffer_to_file_rx_cur += samples_to_copy;
			rx->buffer_to_file_rx_remaining -= samples_to_copy;
			if (rx->buffer_to_file_rx_remaining == 0)
				log_line("RX file captured");
		}

		// Demodulation in real-time (if enabled)
		if (rx->settings.demod_mode == demod_mode_real_time)
		{
			uint32_t data_demodulated = decoder_parse_next_samples(rx->decoder, samples_buffer,
					rx->buffer_data, rx->buffer_data_count);
			log_format("%.*s", data_demodulated, rx->buffer_data);
			if (rx->buffer_to_file_data_remaining)
			{
				if (data_demodulated > rx->buffer_to_file_data_remaining)
					data_demodulated = rx->buffer_to_file_data_remaining;
				memcpy(rx->buffer_to_file_data_cur, rx->buffer_data, data_demodulated);
				rx->buffer_to_file_data_cur += data_demodulated;
				rx->buffer_to_file_data_remaining -= data_demodulated;
			}
		}
	}
	uint32_t buffer_processing_us = plc_time_hires_interval_to_usec(stamp_ini,
			plc_time_get_hires_stamp());
	plc_rx_analysis_report_rx_statistics_time(rx->plc_rx_analysis, buffer_processing_us);
	return data_detected;
}

static void rx_on_buffer_completed_wrapper(void *rx, sample_rx_t *samples_buffer,
		uint32_t samples_buffer_count)
{
	int data_detected = rx_on_buffer_completed(rx, samples_buffer, samples_buffer_count);
	plc_leds_set_rx_activity(((struct rx*) rx)->leds, data_detected);
}

void rx_start_capture(struct rx *rx)
{
	usleep(100000);
	TRACE(3, "Start capture");
	rx->rx_samples_per_bit = (float) rx->settings.data_bit_us * plc_adc_get_sampling_frequency()
			/ 1000000.0;
	log_format("rx_samples_per_bit=%g\n", rx->rx_samples_per_bit);

	TRACE(3, "Rest analysis");
	// Initialize analysis
	plc_rx_analysis_reset(rx->plc_rx_analysis);
	TRACE(3, "Configure analysis");
	plc_rx_analysis_configure(
			rx->plc_rx_analysis, plc_adc_get_sampling_frequency(), rx->settings.data_bit_us,
			rx->settings.data_offset, rx->settings.data_hi_threshold_detection);

	TRACE(3, "Checking file");
	// Delete previous file if exists
	if (access(rx->file_data_path, F_OK) != -1)
		unlink(rx->file_data_path);

	rx->buffer_to_file_rx_remaining = rx->settings.samples_to_file;
	if (rx->settings.samples_to_file)
	{
		rx->buffer_to_file_rx = malloc(rx->settings.samples_to_file * sizeof(sample_rx_t));
		rx->buffer_to_file_data_remaining = FILE_DATA_SAMPLES;
		rx->buffer_to_file_data = malloc(FILE_DATA_SAMPLES);
	}
	rx->buffer_to_file_rx_cur = rx->buffer_to_file_rx;
	rx->buffer_to_file_data_cur = rx->buffer_to_file_data;

	TRACE(3, "Setting callback");
	plc_adc_set_rx_buffer_completed_callback(rx->plc_adc, rx_on_buffer_completed_wrapper, rx);

	TRACE(3, "Initiate demodulator");
	decoder_initialize_demodulator(rx->decoder, rx->adc_buffer_samples,
			rx->settings.demod_data_hi_threshold, rx->settings.data_offset, rx->rx_samples_per_bit,
			rx->settings.samples_to_file);
	rx->buffer_data_count = ceil((float) rx->adc_buffer_samples / rx->rx_samples_per_bit / 8.0);
	rx->buffer_data = malloc(rx->buffer_data_count);
	TRACE(3, "plc_adc_start_capture");
	switch (rx->settings.rx_mode)
	{
	case rx_mode_buffer_by_buffer:
	case rx_mode_kernel_buffering:
	{
		plc_adc_start_capture(rx->plc_adc, rx->adc_buffer_samples,
				(rx->settings.rx_mode == rx_mode_kernel_buffering));
		break;
	}
	case rx_mode_sample_by_sample:
		rx->end_thread = 0;
		rx->adc_sample_by_sample_buffer_pos = 0;
		int ret = pthread_create(&rx->thread, NULL, thread_adc_rx_buffer, rx);
		assert(ret == 0);
		break;
	default:
		assert(0);
		break;
	}
	TRACE(3, "Capture started");
}

void rx_stop_capture(struct rx *rx)
{
	int ret;
	if (rx->plc_adc)
	{
		switch (rx->settings.rx_mode)
		{
		case rx_mode_buffer_by_buffer:
		case rx_mode_kernel_buffering:
			plc_adc_stop_capture(rx->plc_adc);
			usleep(100000);
			break;
		case rx_mode_sample_by_sample:
		{
			rx->end_thread = 1;
			int ret = pthread_join(rx->thread, NULL);
			assert(ret == 0);
		}
			break;
		default:
			assert(0);
			break;
		}
		// If real-time data logged add a new line for freshh logging
		if (rx->settings.demod_mode == demod_mode_real_time)
			log_line("");
		if (rx->settings.samples_to_file)
		{
			if (rx->settings.demod_mode == demod_mode_deferred)
			{
				sample_rx_t *rx_cur = rx->buffer_to_file_rx;
				sample_rx_t *rx_cur_end = rx->buffer_to_file_rx_cur - rx->adc_buffer_samples + 1;
				for (; rx_cur < rx_cur_end; rx_cur += rx->adc_buffer_samples)
				{
					uint32_t data_demodulated = decoder_parse_next_samples(rx->decoder, rx_cur,
							rx->buffer_data, rx->buffer_data_count);
					if (rx->buffer_to_file_data_remaining)
					{
						if (data_demodulated > rx->buffer_to_file_data_remaining)
							data_demodulated = rx->buffer_to_file_data_remaining;
						memcpy(rx->buffer_to_file_data_cur, rx->buffer_data, data_demodulated);
						rx->buffer_to_file_data_cur += data_demodulated;
						rx->buffer_to_file_data_remaining -= data_demodulated;
					}
				}
			}
		}
		if (rx->buffer_to_file_data)
		{
			ret = plc_file_write_csv(rx->file_data_path, csv_u8, rx->buffer_to_file_data,
					rx->buffer_to_file_data_cur - rx->buffer_to_file_data, 0);
			assert(ret >= 0);
			free(rx->buffer_to_file_data);
		}
	}
	if (rx->buffer_data)
		free(rx->buffer_data);
	decoder_terminate_demodulator(rx->decoder);
	if (rx->buffer_to_file_rx)
	{
		ret = plc_file_write_csv(rx->file_rx_path, sample_rx_csv_enum, rx->buffer_to_file_rx,
				rx->buffer_to_file_rx_cur - rx->buffer_to_file_rx, 0);
		assert(ret >= 0);
		free(rx->buffer_to_file_rx);
	}
	plc_leds_set_rx_activity(rx->leds, 0);
}

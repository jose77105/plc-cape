/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include <gtk/gtk.h>		// g_warning
#include <unistd.h>			// usleep
#include <sys/utsname.h>	// uname
#include "+common/api/+base.h"
#include "+common/api/bbb.h"
#include "recorder_plc.h"
#include "libraries/libplc-adc/api/adc.h"
#include "tools.h"

//#define BUFFER_SAMPLES 2048
#define BUFFER_SAMPLES 1024
// #define SAMPLES_OFFSET ((1 << 12) / 2)
#define SAMPLES_OFFSET 0

Recorder_plc::Recorder_plc()
{
	plc_adc = NULL;
	rx_device = plc_rx_device_adc_bbb;
	samples_adc_len = BUFFER_SAMPLES;
	samples_adc = (sample_rx_t*) malloc(sizeof(sample_rx_t) * samples_adc_len);
	samples_adc_cur = samples_adc;
	samples_adc_end = samples_adc + samples_adc_len;
	samples_adc_pop = samples_adc;
	paused = 0;
	samples_adc_stored = 0;
	overflows_detected = 0;
	capturing_rate_sps = 0;
	samples_adc_capturing_time_per_quarter_buffer_us = 0;
	plc_rx_analysis = NULL;
	rx_statistics_mode = plc_rx_statistics_none;
}

Recorder_plc::~Recorder_plc()
{
	free(samples_adc);
}

void Recorder_plc::initialize(void)
{
	puts("Initiating ADC for continuous recording...");
	struct utsname utsname;
	int ret = uname(&utsname);
	assert(ret == 0);
	rx_device = (strcmp(utsname.machine, "i686") == 0) ? plc_rx_device_alsa : plc_rx_device_adc_bbb;
	plc_adc = plc_adc_create(rx_device);
	if (plc_adc == NULL)
	{
		perror("Error: ADC object cannot be created");
		exit(EXIT_FAILURE);
	}
	plc_adc_set_rx_buffer_completed_callback(plc_adc, rx_buffer_completed_callback, this);
	plc_rx_analysis = plc_rx_analysis_create();
	set_configuration_defaults();
}

void Recorder_plc::terminate(void)
{
	assert(plc_rx_analysis);
	plc_rx_analysis_release(plc_rx_analysis);
	plc_rx_analysis = NULL;
	assert(plc_adc);
	plc_adc_release(plc_adc);
	plc_adc = NULL;
	puts("ADC stopped");
}

void Recorder_plc::release_configuration(void)
{
}

void Recorder_plc::set_configuration_defaults(void)
{
	memset((recorder_plc_configuration*) this, 0, sizeof(struct recorder_plc_configuration));
	capturing_rate_sps = (rx_device == plc_rx_device_alsa) ? 48000.0 : ADC_MAX_CAPTURE_RATE_SPS;
	plc_rx_analysis_set_statistics_mode(plc_rx_analysis, rx_statistics_mode);
}

int Recorder_plc::begin_configuration(void)
{
	release_configuration();
	set_configuration_defaults();
	return 0;
}

int Recorder_plc::set_configuration_setting(const char *identifier, const char *data)
{
	if (strcmp(identifier, "capturing_rate_sps") == 0)
	{
		capturing_rate_sps = atof(data);
	}
	else
	{
		return -1;
	}
	return 0;
}

int Recorder_plc::end_configuration(void)
{
	return 0;
}

void Recorder_plc::set_preferred_capturing_rate(float capturing_rate_sps)
{
	// TODO: Apply recorder device constraints
	this->capturing_rate_sps = capturing_rate_sps;
}

float Recorder_plc::get_real_capturing_rate(void)
{
	return capturing_rate_sps;
}

void Recorder_plc::start_recording(void)
{
	assert(plc_adc);
	samples_adc_pop = samples_adc;
	samples_adc_stored = 0;
	overflows_detected = 0;
	samples_adc_capturing_time_per_quarter_buffer_us = (uint32_t) ((250000.0 / capturing_rate_sps)
			* samples_adc_len + .5);
	plc_adc_start_capture(plc_adc, BUFFER_SAMPLES, 1, capturing_rate_sps);
	plc_rx_analysis_reset(plc_rx_analysis);
}

void Recorder_plc::stop_recording(void)
{
	assert(plc_adc);
	plc_adc_stop_capture(plc_adc);
	// TODO: Improve tracing
	if (overflows_detected)
	{
		char text[100];
		sprintf(text, "RECORDER: Overflows detected: %u", overflows_detected);
		g_warning(text);
	}
}

void Recorder_plc::pause(void)
{
	paused = 1;
}

void Recorder_plc::resume(void)
{
	paused = 0;
	samples_adc_stored = 0;
	samples_adc_pop = samples_adc_cur;
}

void Recorder_plc::pop_recorded_buffer(sample_rx_t *samples, uint32_t samples_count)
{
	// TODO: Improve contention mechanism
	while (samples_count > samples_adc_stored)
	{
		assert(samples_count <= BUFFER_SAMPLES);
		usleep(samples_adc_capturing_time_per_quarter_buffer_us);
	}
	uint32_t samples_to_end_buffer = samples_adc_end - samples_adc_pop;
	if (samples_to_end_buffer > samples_count)
	{
		for (; samples_count > 0; samples_count--)
			*samples++ = *samples_adc_pop++ + SAMPLES_ZERO_REF - SAMPLES_OFFSET;
	}
	else
	{
		uint32_t samples_count_first = samples_to_end_buffer;
		uint32_t samples_count_second = samples_count - samples_to_end_buffer;
		for (; samples_count_first > 0; samples_count_first--)
			*samples++ = *samples_adc_pop++ + SAMPLES_ZERO_REF - SAMPLES_OFFSET;
		samples_adc_pop = samples_adc;
		for (; samples_count_second > 0; samples_count_second--)
			*samples++ = *samples_adc_pop++ + SAMPLES_ZERO_REF - SAMPLES_OFFSET;
	}
	samples_adc_stored -= samples_count;
}

void Recorder_plc::get_statistics(char *text, size_t text_size)
{
	const struct rx_statistics *statistics = plc_rx_analysis_get_statistics(plc_rx_analysis);
	switch (rx_statistics_mode)
	{
	case plc_rx_statistics_values:
		snprintf(text, text_size, "DC mean: %g, AC mean: %g\nMin: %u, Max: %u",
				statistics->buffer_dc_mean, statistics->buffer_ac_mean, statistics->buffer_min,
				statistics->buffer_max);
		break;
	default:
		*text = '\0';
		break;
	}
}

void Recorder_plc::rx_buffer_completed_callback(void *data, sample_rx_t *samples_buffer,
		uint32_t samples_buffer_count)
{
	Recorder_plc *recorder = (Recorder_plc*) data;
	if (recorder->paused)
		return;
	assert(samples_buffer_count <= (uint32_t )(recorder->samples_adc_end - recorder->samples_adc));
	uint32_t samples_to_end_buffer = recorder->samples_adc_end - recorder->samples_adc_cur;
	if (samples_to_end_buffer > samples_buffer_count)
	{
		memcpy(recorder->samples_adc_cur, samples_buffer,
				samples_buffer_count * sizeof(sample_rx_t));
		recorder->samples_adc_cur += samples_buffer_count;
	}
	else
	{
		memcpy(recorder->samples_adc_cur, samples_buffer,
				samples_to_end_buffer * sizeof(sample_rx_t));
		uint32_t samples_remaining = samples_buffer_count - samples_to_end_buffer;
		memcpy(recorder->samples_adc, samples_buffer + samples_to_end_buffer,
				samples_remaining * sizeof(sample_rx_t));
		recorder->samples_adc_cur = recorder->samples_adc + samples_remaining;
	}
	recorder->samples_adc_stored += samples_buffer_count;
	// TODO: Control 'pop' overflow
	if (recorder->samples_adc_stored > recorder->samples_adc_len)
	{
		recorder->samples_adc_stored = recorder->samples_adc_len;
		recorder->samples_adc_pop = recorder->samples_adc_cur;
		recorder->overflows_detected++;
	}
	plc_rx_analysis_analyze_buffer(recorder->plc_rx_analysis, samples_buffer, samples_buffer_count);
}

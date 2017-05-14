/**
 * @file
 * @brief	**Main** file
 *
 * @see		@ref plugin-decoder-pwm
 *
 * @cond COPYRIGHT_NOTES
 *
 * ##LICENSE
 *
 *		This file is part of plc-cape project.
 *
 *		plc-cape project is free software: you can redistribute it and/or modify
 *		it under the terms of the GNU General Public License as published by
 *		the Free Software Foundation, either version 3 of the License, or
 *		(at your option) any later version.
 *
 *		plc-cape project is distributed in the hope that it will be useful,
 *		but WITHOUT ANY WARRANTY; without even the implied warranty of
 *		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *		GNU General Public License for more details.
 *
 *		You should have received a copy of the GNU General Public License
 *		along with plc-cape project.  If not, see <http://www.gnu.org/licenses/>. 
 *
 * @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega
 * 
 * @endcond
 */

#include <err.h>			// warnx
#include <math.h>
#include "+common/api/+base.h"
#include "+common/api/error.h"
#include "+common/api/logger.h"
#include "+common/api/setting.h"
#include "libraries/libplc-tools/api/signal.h"
// Declare the custom type used as handle. Doing it like this avoids the 'void*' hard-casting
#define PLUGINS_API_HANDLE_EXPLICIT_DEF
typedef struct decoder *decoder_api_h;
#include "plugins/decoder/api/decoder.h"

struct decoder
{
	float sampling_rate_sps;
	// Static data
	uint32_t chunk_samples;
	float carrier_freq;
	sample_rx_t offset;
	sample_rx_t carrier_threshold;
	uint32_t stop_samples;
	uint32_t bit_width_us;
	uint32_t samples_to_file;
	float samples_per_bit;
	// Dynamic data
	struct plc_signal_iir *signal_iir;
	uint32_t samples_with_carrier;
	uint32_t samples_without_carrier;
};

// Connection with the 'singletons_provider'
static singletons_provider_get_t singletons_provider_get = NULL;
static singletons_provider_h singletons_provider_handle = NULL;

// Error reporting function
static struct plc_error_api *plc_error_api;
static void *error_ctrl_handle;

// Logger served by the 'singletons_provider'
static struct plc_logger_api *logger_api;
static void *logger_handle;

//[iir_b,iir_a] = butter(2, 0.05)
static const float iir_a_default[] = { 1.00000, -1.77863, 0.80080 };
static const float iir_b_default[] = { 0.0055427, 0.0110854, 0.0055427 };

// Error function shortcut
int set_error_msg(const char *msg)
{
	if (plc_error_api)
		plc_error_api->set_error_msg(error_ctrl_handle, msg);
	else
		warnx("%s", msg);
	return -1;
}

static void decoder_set_defaults(struct decoder *decoder)
{
	memset(decoder, 0, sizeof(*decoder));
	decoder->sampling_rate_sps = 100000.0f;
	decoder->carrier_threshold = 50;
	decoder->offset = 500;
	decoder->bit_width_us = 1000;
}

struct decoder *decoder_create(void)
{
	struct decoder *decoder = malloc(sizeof(struct decoder));
	decoder_set_defaults(decoder);
	return decoder;
}

static void decoder_release_resources(struct decoder *decoder)
{
}

void decoder_release(struct decoder *decoder)
{
	decoder_release_resources(decoder);
	free(decoder);
}

const struct plc_setting_definition accepted_settings[] = {
	{
		"sampling_rate_sps", plc_setting_float, "Freq Capture [sps]", {
			.f = 100000.0f }, 0 }, {
		"freq", plc_setting_float, "Frequency", {
			.f = 2000.0f }, 0 }, {
		"data_hi_threshold", plc_setting_u16, "Data HI Threshold", {
			.u16 = 50 }, 0 }, {
		"data_offset", plc_setting_u16, "Data offset", {
			.u16 = 500 }, 0 }, {
		"bit_width_us", plc_setting_u32, "Bit Width [us]", {
			.u32 = 1000 }, 0 }, {
		"samples_to_file", plc_setting_u32, "Samples to file", {
			.u32 = 0 }, 0 } };

const struct plc_setting_definition *decoder_get_accepted_settings(struct decoder *decoder,
		uint32_t *accepted_settings_count)
{
	*accepted_settings_count = ARRAY_SIZE(accepted_settings);
	return accepted_settings;
}

int decoder_begin_settings(struct decoder *decoder)
{
	decoder_release_resources(decoder);
	decoder_set_defaults(decoder);
	return 0;
}

int decoder_set_setting(struct decoder *decoder, const char *identifier,
		union plc_setting_data data)
{
	if (strcmp(identifier, "sampling_rate_sps") == 0)
	{
		decoder->sampling_rate_sps = data.f;
	}
	else if (strcmp(identifier, "freq") == 0)
	{
		decoder->carrier_freq = data.f;
	}
	else if (strcmp(identifier, "data_hi_threshold") == 0)
	{
		decoder->carrier_threshold = data.u16;
	}
	else if (strcmp(identifier, "data_offset") == 0)
	{
		decoder->offset = data.u16;
	}
	else if (strcmp(identifier, "bit_width_us") == 0)
	{
		decoder->bit_width_us = data.u32;
	}
	else if (strcmp(identifier, "samples_to_file") == 0)
	{
		decoder->samples_to_file = data.u32;
	}
	else
	{
		return set_error_msg("Unknown setting");
	}
	return 0;
}

int decoder_end_settings(struct decoder *decoder)
{
	decoder->samples_per_bit = round(
			decoder->sampling_rate_sps * decoder->bit_width_us / 1000000.0f);
	if (decoder->samples_per_bit == 0)
		return set_error_msg("Bit width must be greater than 1 us");
	// Stop_samples = 1-bit. Just a reasonable default value
	decoder->stop_samples = ceil(decoder->samples_per_bit);
	return 0;
}

void decoder_initialize(struct decoder *decoder, uint32_t chunk_samples)
{
#ifdef DEBUG
	if (logger_api)
	{
		char szMsg[128];
		sprintf(szMsg, "offset=%u, carrier_threshold=%u, samples_per_bit=%.1f\n",
				decoder->offset, decoder->carrier_threshold, decoder->samples_per_bit);
		logger_api->log_line(logger_handle, szMsg);
	}
#endif
	assert(decoder->signal_iir == NULL);
	decoder->chunk_samples = chunk_samples;
	decoder->signal_iir = plc_signal_iir_create(chunk_samples, iir_a_default,
			ARRAY_SIZE(iir_a_default), iir_b_default, ARRAY_SIZE(iir_b_default));
	plc_signal_iir_set_samples_to_file(decoder->signal_iir, decoder->samples_to_file);
	if (decoder->carrier_freq)
	{
		plc_signal_iir_set_demodulator(decoder->signal_iir, plc_signal_iir_demodulator_cos);
		plc_signal_iir_set_demodulation_frequency(decoder->signal_iir,
				decoder->carrier_freq / decoder->sampling_rate_sps);
	}
	else
	{
		plc_signal_iir_set_demodulator(decoder->signal_iir, plc_signal_iir_demodulator_abs);
	}
	decoder->samples_with_carrier = 0;
	decoder->samples_without_carrier = 0;
}

void decoder_terminate(struct decoder *decoder)
{
	plc_signal_iir_release(decoder->signal_iir);
	decoder->signal_iir = NULL;
}

uint32_t decoder_parse_next_samples(struct decoder *decoder, const sample_rx_t *buffer_in,
		uint8_t *buffer_data_out, uint32_t buffer_data_out_count)
{
	plc_signal_iir_process_chunk(decoder->signal_iir, buffer_in, decoder->offset);
	uint32_t n;
	float *out_f = plc_signal_get_buffer_out(decoder->signal_iir);
	uint8_t *buffer_data_out_ini = buffer_data_out;
	for (n = 0; n < decoder->chunk_samples; n++)
	{
		int hi_level_detected = (out_f[n] >= decoder->carrier_threshold);
		if (decoder->samples_with_carrier == 0)
		{
			// If 'hi_level_detected' start counting the carrier, even when below the level
			// Improvement: instead of accumulate just mark the starting point
			if (hi_level_detected)
				decoder->samples_with_carrier++;
		}
		else
		{
			decoder->samples_with_carrier++;
			// Data-end detection
			if (hi_level_detected)
			{
				// Whenever carrier detected reset the 'samples_without_carrier' counter
				decoder->samples_without_carrier = 0;
			}
			else
			{
				if (++decoder->samples_without_carrier >= decoder->stop_samples)
				{
					// Reject the spurius
					if (decoder->samples_with_carrier < round(decoder->samples_per_bit / 2.0))
					{
						decoder->samples_with_carrier = 0;
						decoder->samples_without_carrier = 0;
					}
					else
					{
						// Adjust removing the stop-samples extra-counting
						decoder->samples_with_carrier -= decoder->stop_samples;
						float f = round(
								decoder->samples_with_carrier / decoder->samples_per_bit);
						if ((f >= 32.0) && (f <= 255.0))
							*buffer_data_out++ = (uint8_t) f;
						decoder->samples_with_carrier = 0;
						decoder->samples_without_carrier = 0;
					}
				}
			}
		}
	}
	return buffer_data_out - buffer_data_out_ini;
}

ATTR_EXTERN void PLUGIN_API_SET_SINGLETON_PROVIDER(singletons_provider_get_t callback,
		singletons_provider_h handle)
{
	singletons_provider_get = callback;
	singletons_provider_handle = handle;
	// Ask for the required callbacks
	uint32_t version;
	singletons_provider_get(singletons_provider_handle, singleton_id_logger, (void**) &logger_api,
			&logger_handle, &version);
	assert(!logger_api || (version >= 1));
}

ATTR_EXTERN void *PLUGIN_API_LOAD(uint32_t *plugin_api_version, uint32_t *plugin_api_size)
{
	CHECK_INTERFACE_MEMBERS_COUNT(decoder_api, 9);
	*plugin_api_version = 1;
	*plugin_api_size = sizeof(struct decoder_api);
	struct decoder_api *decoder_api = calloc(1, *plugin_api_size);
	decoder_api->create = decoder_create;
	decoder_api->release = decoder_release;
	decoder_api->get_accepted_settings = decoder_get_accepted_settings;
	decoder_api->begin_settings = decoder_begin_settings;
	decoder_api->set_setting = decoder_set_setting;
	decoder_api->end_settings = decoder_end_settings;
	decoder_api->initialize = decoder_initialize;
	decoder_api->terminate = decoder_terminate;
	decoder_api->parse_next_samples = decoder_parse_next_samples;
	return decoder_api;
}

ATTR_EXTERN void PLUGIN_API_UNLOAD(void *decoder_api)
{
	free(decoder_api);
}

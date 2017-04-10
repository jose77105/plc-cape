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
 *	Copyright (C) 2016 Jose Maria Ortega
 * 
 * @endcond
 */

#include <math.h>
#include "+common/api/+base.h"
#include "+common/api/logger.h"

// Declare the custom type used as handle. Doing it like this avoids the 'void*' hard-casting
#define PLUGINS_API_HANDLE_EXPLICIT_DEF
typedef struct decoder *decoder_api_h;
#include "plugins/decoder/api/decoder.h"

struct decoder
{
	// TODO: Remove buffer
	sample_rx_t *just_a_dummy_buffer;
	// Static data
	uint32_t chunk_samples;
	sample_rx_t offset;
	sample_rx_t carrier_threshold;
	uint32_t stop_samples;
	float samples_per_bit;
	// Dynamic data
	uint32_t samples_with_carrier;
	uint32_t samples_without_carrier;
};

// Connection with the 'singletons_provider'
static singletons_provider_get_t singletons_provider_get = NULL;
static singletons_provider_h singletons_provider_handle = NULL;

// Logger served by the 'singletons_provider'
static struct plc_logger_api *logger_api;
static void *logger_handle;

struct decoder *decoder_create(void)
{
	struct decoder *decoder = calloc(1, sizeof(struct decoder));
	return decoder;
}

void decoder_release(struct decoder *decoder)
{
	free(decoder);
}

void decoder_initialize_demodulator(struct decoder *decoder, uint32_t chunk_samples,
		sample_rx_t demod_data_hi_threshold, sample_rx_t data_offset,
		float samples_per_bit, uint32_t samples_to_file)
{
#ifdef DEBUG
	if (logger_api)
	{
		char szMsg[128];
		sprintf(szMsg, "offset=%u, carrier_threshold=%u, samples_per_bit=%.1f\n",
				decoder->offset, decoder->carrier_threshold, samples_per_bit);
		logger_api->log_line(logger_handle, szMsg);
	}
#endif
	decoder->chunk_samples = chunk_samples;
	decoder->carrier_threshold = demod_data_hi_threshold;
	decoder->offset = data_offset;
	decoder->samples_per_bit = samples_per_bit;
	decoder->samples_with_carrier = 0;
	decoder->samples_without_carrier = 0;
	// Stop_samples = 1-bit. Just a reasonable default value
	decoder->stop_samples = ceil(samples_per_bit);
	decoder->just_a_dummy_buffer = malloc(chunk_samples * sizeof(sample_rx_t));
}

void decoder_terminate_demodulator(struct decoder *decoder)
{
	free(decoder->just_a_dummy_buffer);
}

uint32_t decoder_parse_next_samples(struct decoder *decoder, const sample_rx_t *buffer_in,
		uint8_t *buffer_data_out, uint32_t buffer_data_out_count)
{
	uint8_t *buffer_data_out_ini = buffer_data_out;
	const sample_rx_t *buffer_in_end = buffer_in + decoder->chunk_samples;
	for (; buffer_in < buffer_in_end; buffer_in++)
	{
		int hi_level_detected = (abs((int16_t) *buffer_in - (int16_t) decoder->offset)
				>= decoder->carrier_threshold);
		if (decoder->samples_with_carrier == 0)
		{
			// TODO: instead of 'int16_t' use a global macro
			// If 'hi_level_detected' start counting the carrier, even when below the level
			// Improvement: instead of accumulate just mark the starting point
			if (hi_level_detected)
				decoder->samples_with_carrier++;
		}
		else
		{
			decoder->samples_with_carrier++;
			// Data-end detection
			// TODO: Now detection is done by a high-enough sample level. This makes the PWM
			//	width measured shorter than real.
			//	Ideal: filtering as in OOK but more computational effort
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
						// TODO: Add as a tracing configurable mode of the 'Encoder' plugin
						/*
						 if (logger_api)
						 {
						 char szMsg[64];
						 sprintf(szMsg, "samples_with_carrier=%u, samples_without_carrier=%u",
						 decoder->samples_with_carrier,
						 decoder->samples_without_carrier);
						 logger_api->log_line(logger_handle, szMsg);
						 }
						 */
						// Adjust removing the stop-samples extra-counting
						decoder->samples_with_carrier -= decoder->stop_samples;
						// TODO: test for maximum value exceeded
						*buffer_data_out++ = (uint8_t) round(
								decoder->samples_with_carrier / decoder->samples_per_bit);
						decoder->samples_with_carrier = 0;
						decoder->samples_without_carrier = 0;
					}
				}
			}
		}
	}
	return buffer_data_out - buffer_data_out_ini;
}

sample_rx_t *decoder_chunk_get_output_buffer(struct decoder *decoder)
{
	return decoder->just_a_dummy_buffer;
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
	CHECK_INTERFACE_MEMBERS_COUNT(decoder_api, 5);
	*plugin_api_version = 1;
	*plugin_api_size = sizeof(struct decoder_api);
	struct decoder_api *decoder_api = calloc(1, *plugin_api_size);
	decoder_api->create = decoder_create;
	decoder_api->release = decoder_release;
	decoder_api->initialize_demodulator = decoder_initialize_demodulator;
	decoder_api->terminate_demodulator = decoder_terminate_demodulator;
	decoder_api->parse_next_samples = decoder_parse_next_samples;
	return decoder_api;
}

ATTR_EXTERN void PLUGIN_API_UNLOAD(void *decoder_api)
{
	free(decoder_api);
}

/**
 * @file
 * @brief	**Main** file
 *	
 * @see		@ref plugin-decoder-ook
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

// FIR_SAMPLES must be odd
#define FIR_SAMPLES 21
#define FIR_CENTER ((FIR_SAMPLES-1)/2)
#define FIR_CUTOFF_HZ (DEMOD_FREQ_HZ/2)

// Analog-to-Digital converter
#define ADC_BITS 12
#define ADC_RANGE (1 << ADC_BITS)
#define ADC_MAX (ADC_RANGE-1)

// Quantifier
#define Q_BITS 10
// #define Q_BITS AFE_DAC_BITS
#define Q_RANGE (1 << Q_BITS)
#define Q_MAX (Q_RANGE-1)

#define SAMPLES_PER_BIT_FRACTION_MAGNIFIER 1000

// TODO: Make global variables an option. In any case, it seems that the resynchrno is not properly
//	implemented. Review
static const int auto_resynchronization = 0;

//[iir_b,iir_a] = butter(2, 0.01)
static const float iir_a_default[] = { 1.00000, -1.95558, 0.95654 };
static const float iir_b_default[] = { 2.4136e-04, 4.8272e-04, 2.4136e-04 };

struct decoder
{
	float sampling_rate_sps;
	// chunk mode
	uint32_t chunk_samples;
	struct plc_signal_iir *signal_iir;
	sample_rx_t *buffer_out_filter;
	// data mode
	float carrier_freq;
	uint32_t data_hi_threshold;
	uint32_t data_offset;
	uint32_t bit_width_us;
	uint32_t samples_to_file;
	uint32_t samples_hi_threshold;
	uint32_t samples_not_hi_threshold;
	uint32_t samples_per_bit;
	int samples_per_bit_fraction;
	uint32_t samples_hi;
	uint32_t samples_lo;
	sample_rx_t *buffer_out_filter_cur;
	sample_rx_t *buffer_out_filter_end;
	sample_rx_t *buffer_out_filter_next;
	int buffer_data_next_fraction;
	uint8_t data_in_process;
	int start_bit_found;
	uint16_t data_per_frame;
	uint16_t data_per_frame_cur;
	uint8_t bits_per_data;
	uint8_t bits_per_data_cur;
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
	decoder->data_hi_threshold = 50;
	decoder->data_offset = 500;
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
	assert((decoder->signal_iir == NULL) && (decoder->buffer_out_filter == NULL));
	decoder_release_resources(decoder);
	free(decoder);
}

const struct plc_setting_definition accepted_settings[] = {
	{
		"sampling_rate_sps", plc_setting_float, "Freq Capture [sps]", {
			.f = 100000.0f }, 0 }, {
		"freq", plc_setting_float, "Frequency", {
			.f = 2000.0f }, 0 }, {
		"data_hi_threshold", plc_setting_u16, "Demod data HI Threshold", {
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
		decoder->data_hi_threshold = data.u16;
	}
	else if (strcmp(identifier, "data_offset") == 0)
	{
		decoder->data_offset = data.u16;
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
	return 0;
}

// PRECONDITION: 'buffer_out_filter' with allocated space enough
// PRECONDITION: ceil(samples_per_bit) > decoder->chunk_samples
void decoder_initialize(struct decoder *decoder, uint32_t chunk_samples)
{
	assert((decoder->signal_iir == NULL) && (decoder->buffer_out_filter == NULL));
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
	decoder->buffer_out_filter = malloc(chunk_samples * sizeof(sample_rx_t));
	// For simplicity in the circular buffer assume 'samples_per_bit < chunk_samples'
	if ((uint32_t) ceil(decoder->samples_per_bit) > decoder->chunk_samples)
	{
		if (logger_api)
			logger_api->log_line(logger_handle,
					"'Samples per bit' too long. It must fit at least on a chunk");
		assert(0);
	}
	// Accept a bit if active 75% of symbol length
	// TODO: Logics should be 'float-based' instead of 'uint32_t-based'.
	//	i.e. float samples_hi_threshold...
	decoder->samples_hi_threshold = round(decoder->samples_per_bit * 3 / 4);
	decoder->samples_not_hi_threshold = round(
			decoder->samples_per_bit - decoder->samples_hi_threshold);
	// TODO: Use another counter to take into account the fractional part
	decoder->samples_per_bit = round(decoder->samples_per_bit);
	decoder->samples_per_bit_fraction = (decoder->samples_per_bit - decoder->samples_per_bit)
			* SAMPLES_PER_BIT_FRACTION_MAGNIFIER;
	decoder->samples_hi = 0;
	decoder->samples_lo = 0;
	decoder->buffer_out_filter_cur = decoder->buffer_out_filter;
	decoder->buffer_out_filter_end = decoder->buffer_out_filter_cur + decoder->chunk_samples;
	decoder->start_bit_found = 0;
	// If 'decoder->data_per_frame == 0' then continuous data after the first start bit detected
	// TODO: Make 'data_per_frame' and 'bits_per_data' configurable?
	decoder->data_per_frame = 1;
	decoder->data_per_frame_cur = 0;
	decoder->bits_per_data = 8;
	decoder->bits_per_data_cur = 0;
	decoder->data_in_process = 0;
}

void decoder_terminate(struct decoder *decoder)
{
	free(decoder->buffer_out_filter);
	decoder->buffer_out_filter = NULL;
	plc_signal_iir_release(decoder->signal_iir);
	decoder->signal_iir = NULL;
}

uint32_t decoder_parse_next_samples_buffer(struct decoder *decoder, const sample_rx_t *buffer_in,
		uint8_t *buffer_data_out, uint32_t buffer_data_out_count)
{
	plc_signal_iir_process_chunk(decoder->signal_iir, buffer_in, decoder->data_offset);
	uint32_t n;
	float *out_f = plc_signal_get_buffer_out(decoder->signal_iir);
	for (n = 0; n < decoder->chunk_samples; n++)
		decoder->buffer_out_filter[n] = (sample_rx_t) (abs(round(out_f[n])));
	if (buffer_data_out_count == 0)
		return 0;
	uint8_t *buffer_data_out_end = buffer_data_out + buffer_data_out_count;
	uint8_t *buffer_data_out_cur = buffer_data_out;
	while (1)
	{
		if (!decoder->start_bit_found)
		{
			for (; decoder->buffer_out_filter_cur < decoder->buffer_out_filter_end;
					decoder->buffer_out_filter_cur++)
			{
				if (*decoder->buffer_out_filter_cur >= decoder->data_hi_threshold)
				{
					decoder->samples_hi++;
					if (decoder->samples_hi == decoder->samples_hi_threshold)
					{
						decoder->start_bit_found = 1;
						decoder->samples_hi = 0;
						decoder->buffer_out_filter_cur += decoder->samples_not_hi_threshold;
						int next_chunk = (decoder->buffer_out_filter_cur
								>= decoder->buffer_out_filter_end);
						if (next_chunk)
							decoder->buffer_out_filter_cur -= decoder->chunk_samples;
						decoder->buffer_out_filter_next = decoder->buffer_out_filter_cur
								+ decoder->samples_per_bit;
						decoder->buffer_data_next_fraction = decoder->samples_per_bit_fraction;
						if (decoder->buffer_out_filter_next >= decoder->buffer_out_filter_end)
						{
							decoder->buffer_out_filter_next -= decoder->chunk_samples;
							assert(
									(decoder->buffer_out_filter_next >= decoder->buffer_out_filter)
											&& (decoder->buffer_out_filter_next
													< decoder->buffer_out_filter_end));
						}
						if (next_chunk)
							return buffer_data_out_cur - buffer_data_out;
						break;
					}
				}
				else
				{
					decoder->samples_hi = 0;
				}
			}
			if (!decoder->start_bit_found)
			{
				decoder->buffer_out_filter_cur = decoder->buffer_out_filter;
				return buffer_data_out_cur - buffer_data_out;
			}
		}
		int bit_found = 0;
		if (*decoder->buffer_out_filter_cur >= decoder->data_hi_threshold)
		{
			decoder->samples_hi++;
			if (decoder->samples_hi == decoder->samples_hi_threshold)
			{
				decoder->data_in_process = (decoder->data_in_process << 1) | 1;
				bit_found = 1;
			}
		}
		else
		{
			decoder->samples_lo++;
			if (decoder->samples_lo == decoder->samples_not_hi_threshold)
			{
				decoder->data_in_process <<= 1;
				bit_found = 1;
			}
		}
		if (bit_found)
		{
			bit_found = 0;
			decoder->samples_hi = 0;
			decoder->samples_lo = 0;
			// Auto-resynchronization: look for new nearest edge
			if (auto_resynchronization)
			{
				// TODO: Auto-synchro only done if fits on chunk -> extend to next chunk
				int auto_synchro_margin = decoder->samples_per_bit / 8;
				// Three possible edge cases:
				// * edge_ini < filter_cur --> on a different frame
				// * edge_end < filter_cur --> on a different frame
				// * edge_end > filter_end --> on a different frame
				int pos;
				if ((decoder->buffer_out_filter_next > decoder->buffer_out_filter_cur)
						&& (decoder->buffer_out_filter_next + auto_synchro_margin
								<= decoder->buffer_out_filter_end))
				{
					int candidate_symbol = (*decoder->buffer_out_filter_next
							>= decoder->data_hi_threshold);
					for (pos = 0; pos < auto_synchro_margin; pos++)
					{
						if (candidate_symbol == 1)
						{
							if (*(decoder->buffer_out_filter_next - pos)
									< decoder->data_hi_threshold)
								pos = -pos;
							if (*(decoder->buffer_out_filter_next + pos)
									>= decoder->data_hi_threshold)
								continue;
							break;
						}
						else
						{
							if (*(decoder->buffer_out_filter_next - pos)
									>= decoder->data_hi_threshold)
								pos = -pos;
							if (*(decoder->buffer_out_filter_next + pos)
									< decoder->data_hi_threshold)
								continue;
							break;
						}
					}
					if (abs(pos) < auto_synchro_margin)
					{
						decoder->buffer_out_filter_next += pos;
						// On resynchronization reset 'buffer_data_next_fraction'
						decoder->buffer_data_next_fraction = 0;
						if (decoder->buffer_out_filter_next >= decoder->buffer_out_filter_end)
							decoder->buffer_out_filter_next -= decoder->chunk_samples;
					}
					// TODO: Add this logging to a file for traceability purposes
					// It requires a chunk counter at the beginning of the function
					//	static uint32_t chunk_count = 0;
					//	chunk_count++;
					//	...
					//	logger_log_sequence_format(logger, "S%d@%d ", pos,
					//	(chunk_count-1)*decoder->chunk_samples+
					//		(decoder->buffer_out_filter_next-decoder->buffer_out_filter));
				}
			}
			// If 'buffer_out_filter_cur' jump overflows the buffer of the chunk return to process
			// the next one
			int next_chunk = (decoder->buffer_out_filter_next < decoder->buffer_out_filter_cur);
			decoder->buffer_out_filter_cur = decoder->buffer_out_filter_next;
			decoder->buffer_out_filter_next += decoder->samples_per_bit;
			decoder->buffer_data_next_fraction += decoder->samples_per_bit_fraction;
			if (decoder->buffer_data_next_fraction >= SAMPLES_PER_BIT_FRACTION_MAGNIFIER)
			{
				decoder->buffer_out_filter_next++;
				decoder->buffer_data_next_fraction -= SAMPLES_PER_BIT_FRACTION_MAGNIFIER;
			}
			else if (decoder->buffer_data_next_fraction <= -SAMPLES_PER_BIT_FRACTION_MAGNIFIER)
			{
				decoder->buffer_out_filter_next--;
				decoder->buffer_data_next_fraction += SAMPLES_PER_BIT_FRACTION_MAGNIFIER;
			}
			if (decoder->buffer_out_filter_next >= decoder->buffer_out_filter_end)
			{
				decoder->buffer_out_filter_next -= decoder->chunk_samples;
				assert(
						(decoder->buffer_out_filter_next >= decoder->buffer_out_filter)
								&& (decoder->buffer_out_filter_next < decoder->buffer_out_filter_end));
			}
			if (++decoder->bits_per_data_cur == decoder->bits_per_data)
			{
				decoder->bits_per_data_cur = 0;
				if (decoder->data_per_frame
						&& (++decoder->data_per_frame_cur == decoder->data_per_frame))
				{
					decoder->data_per_frame_cur = 0;
					decoder->start_bit_found = 0;
				}
				if (buffer_data_out_cur)
				{
					*buffer_data_out_cur = decoder->data_in_process;
					if (++buffer_data_out_cur == buffer_data_out_end)
						return buffer_data_out_cur - buffer_data_out;
				}
				decoder->data_in_process = 0;
			}
			if (next_chunk)
				return buffer_data_out_cur - buffer_data_out;
		}
		else
		{
			assert(decoder->buffer_out_filter_cur < decoder->buffer_out_filter_end);
			if (++decoder->buffer_out_filter_cur == decoder->buffer_out_filter_end)
			{
				decoder->buffer_out_filter_cur = decoder->buffer_out_filter;
				return buffer_data_out_cur - buffer_data_out;
			}
		}
	}
}

uint32_t decoder_parse_next_samples(struct decoder *decoder, const sample_rx_t *buffer_in,
		uint8_t *buffer_data_out, uint32_t buffer_data_out_count)
{
	uint32_t data_decoded = decoder_parse_next_samples_buffer(decoder, buffer_in, buffer_data_out,
			buffer_data_out_count);
	return data_decoded;
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

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
 *	Copyright (C) 2016 Jose Maria Ortega
 * 
 * @endcond
 */

#include <math.h>
#include "+common/api/+base.h"
#include "+common/api/logger.h"
#include "libraries/libplc-tools/api/file.h"

// TODO: Target file on a setting
#define ADC_FILE_CAPTURE_FILTER "adc_filter.csv"

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
// #define Q_BITS DAC_BITS
#define Q_RANGE (1 << Q_BITS)
#define Q_MAX (Q_RANGE-1)

#define SAMPLES_PER_BIT_FRACTION_MAGNIFIER 1000

// TODO: Make global variables an option. In any case, it seems that the resynchrno is not properly
//	implemented. Review
static const int auto_resynchronization = 0;

// Coefficients got from 'Octave' >> 'pkg load signal' >> '[iir_b,iir_a] = butter(filter_size, 0.1)'

// '[iir_b,iir_a] = butter(1, 0.1)'
// static const float iir_a_default[] = { 1.00000, -0.72654};
// static const float iir_b_default[] = { 0.13673, 0.13673};

// '[iir_b,iir_a] = butter(2, 0.1)'
static const float iir_a_default[] =
{ 1.00000, -1.56102, 0.64135 };
static const float iir_b_default[] =
{ 0.020083, 0.040167, 0.020083 };

// '[iir_b,iir_a] = butter(3, 0.1)'
//	static const float iir_a_default[] = { 1.00000, -2.37409, 1.92936, -0.53208};
//	static const float iir_b_default[] = { 0.0028982, 0.0086946, 0.0086946, 0.0028982};

// '[iir_b,iir_a] = butter(4, 0.1)'
//	static const float iir_a_default[] = {1.00000, -3.18064, 3.86119, -2.11216, 0.43827};
//	static const float iir_b_default[] =
//		{4.1660e-004, 1.6664e-003, 2.4996e-003, 1.6664e-003, 4.1660e-004};

// '[iir_b,iir_a] = butter(5, 0.1)'
//	static const float iir_a_default[] = {1.00000, -3.98454, 6.43487, -5.25362, 2.16513, -0.35993};
//	static const float iir_b_default[] =
//		{5.9796e-005, 2.9898e-004, 5.9796e-004, 5.9796e-004, 2.9898e-004, 5.9796e-005};

struct decoder
{
	// chunk mode
	uint32_t chunk_samples;
	float *iir_a;
	uint32_t iir_a_count;
	float *iir_b;
	uint32_t iir_b_count;
	float *buffer_in_f;
	float *buffer_out_f;
	sample_rx_t *buffer_out_filter;
	// data mode
	uint32_t demod_data_hi_threshold;
	uint32_t data_offset;
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
	// Filtered data to file option
	sample_rx_t *buffer_to_file_rx_filter;
	uint32_t buffer_to_file_rx_filter_remaining;
	sample_rx_t *buffer_to_file_rx_filter_cur;
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
	assert((decoder->iir_a == NULL) && (decoder->iir_b == NULL) && (decoder->buffer_in_f == NULL)
			&& (decoder->buffer_out_f == NULL) && (decoder->buffer_out_filter == NULL));
	free(decoder);
}

/*
 void filter2(const sample_rx_t *buffer_in, sample_rx_t *buffer_out_filter, uint32_t buffer_count)
 {
 float fir[FIR_SAMPLES];
 float fir_factor = 0.0;
 int n;
 // FIR
 fir_factor = 0.0;
 // float freq = 2.0*M_PI*FIR_CUTOFF_HZ/freq_adc_sps;
 float freq = 2.0*M_PI*0.02;
 for (n = 0; n < FIR_CENTER; n++)
 {
 fir[n] = sin(freq*(n-FIR_CENTER))/(M_PI*(n-FIR_CENTER));
 fir_factor += fir[n];
 }
 fir_factor *= 2.0;
 // For best performance instead of reapplying the formula apply simetry
 for (n = 0; n < FIR_CENTER; n++) fir[n+FIR_CENTER+1] = fir[FIR_CENTER-1-n];
 fir[FIR_CENTER] = 2.0*FIR_CUTOFF_HZ/freq_adc_sps;
 fir_factor += fir[FIR_CENTER];
 fir_factor = 2.0/fir_factor;
 // printf("FIR FACTOR = %f\n", fir_factor);
 // float amp_factor = *fir_factor*Q_MAX/ADC_MAX;
 float amp_factor = fir_factor*3.0;
 // Filtering -> Note: output delayed FIR_CENTER samples
 for (n = 0; n < FIR_SAMPLES-1; n++) buffer_out_filter[n] = 0.0;
 for (; n < buffer_count; n++)
 {
 float d = 0.0;
 int j;
 for (j = 0; j < FIR_SAMPLES; j++) d += buffer_in[n-j]*fir[j];
 // Quantification
 // buffer_out_filter[n] = (sample_rx_t)(d*amp_factor+adc_center+.5);
 buffer_out_filter[n] = (sample_rx_t)(d*amp_factor+.5);
 }
 // log_text("SRC = %d, OUT = %f, DST = %d, ADC Center: %f\n", buffer_in[50], buffer_out_filter[50],
 //			buffer_out_filter[50], adc_center);
 // print_samples_int(buffer_out_Q+FIR_SAMPLES, 30);
 }

 void decode_buffer(const uint16_t * buffer_in, uint16_t * buffer_out_filter,
		uint32_t buffer_count)
 {
 float *buffer_demod = (float*)malloc(sizeof(float)*buffer_count);
 int n;

 // Center
 float adc_center = 0.0;
 for (n = 0; n < buffer_count; n++)
 adc_center += (float)buffer_in[n];
 adc_center = adc_center/buffer_count;

 // Demodulation
 float freq = 2.0*M_PI*DEMOD_FREQ_HZ/freq_adc_sps;
 for (n = 0; n < buffer_count; n++)
 buffer_demod[n] = ((float)buffer_in[n]-adc_center)*cos(freq*n);

 // TODO: NEXT. No funciona demod pq freq's diferentes. Intentar calcular correlación
 for (n = 0; n < buffer_count; n++)
 buffer_out_filter[n] = (int)(buffer_demod[n]+adc_center+.5);
 return;

 filter(buffer_demod, buffer_out_filter, buffer_count);
 }
 */

// PRECONDITION: 'buffer_out_filter' with allocated space enough
// PRECONDITION: ceil(samples_per_bit) > decoder->chunk_samples
void decoder_initialize_demodulator(struct decoder *decoder, uint32_t chunk_samples,
		sample_rx_t demod_data_hi_threshold, sample_rx_t data_offset,
		float samples_per_bit, uint32_t samples_to_file)
{
	//
	// INIT_CHUNK_MODE
	//
	assert((decoder->iir_a == NULL) && (decoder->iir_b == NULL) && (decoder->buffer_in_f == NULL)
			&& (decoder->buffer_out_f == NULL) && (decoder->buffer_out_filter == NULL));
	decoder->chunk_samples = chunk_samples;
	decoder->iir_a = (float*) malloc(sizeof(iir_a_default));
	decoder->iir_a_count = ARRAY_SIZE(iir_a_default);
	memcpy(decoder->iir_a, iir_a_default, sizeof(iir_a_default));
	decoder->iir_b = (float*) malloc(sizeof(iir_b_default));
	decoder->iir_b_count = ARRAY_SIZE(iir_b_default);
	memcpy(decoder->iir_b, iir_b_default, sizeof(iir_b_default));
	decoder->buffer_in_f = malloc((chunk_samples + decoder->iir_b_count) * sizeof(float));
	decoder->buffer_out_f = malloc((chunk_samples + decoder->iir_a_count) * sizeof(float));
	int n;
	for (n = 0; n < decoder->iir_b_count; n++)
		decoder->buffer_in_f[n] = 0.0;
	for (n = 0; n < decoder->iir_a_count; n++)
		decoder->buffer_out_f[n] = 0.0;
	decoder->buffer_out_filter = malloc(chunk_samples * sizeof(sample_rx_t));
	// TODO: Intelligent threshold and offset
	decoder->demod_data_hi_threshold = demod_data_hi_threshold;
	decoder->data_offset = data_offset;
	//
	// CHUNK_INIT_DATA_MODE
	//
	// For simplicity in the circular buffer assume 'samples_per_bit < chunk_samples'
	if ((uint32_t) ceil(samples_per_bit) > decoder->chunk_samples)
	{
		if (logger_api)
			logger_api->log_line(logger_handle,
					"'Samples per bit' too long. It must fit at least on a chunk");
		assert(0);
	}
	// Accept a bit if active 75% of symbol length
	// TODO: Logics should be 'float-based' instead of 'uint32_t-based'.
	//	i.e. float samples_hi_threshold...
	decoder->samples_hi_threshold = round(samples_per_bit * 3 / 4);
	decoder->samples_not_hi_threshold = round(samples_per_bit - decoder->samples_hi_threshold);
	// TODO: Use another counter to take into account the fractional part
	decoder->samples_per_bit = round(samples_per_bit);
	decoder->samples_per_bit_fraction = (samples_per_bit - decoder->samples_per_bit)
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
	// Filtered data to file
	decoder->buffer_to_file_rx_filter_remaining = samples_to_file;
	if (samples_to_file)
		decoder->buffer_to_file_rx_filter = malloc(samples_to_file * sizeof(sample_rx_t));
	decoder->buffer_to_file_rx_filter_cur = decoder->buffer_to_file_rx_filter;
}

void decoder_terminate_demodulator(struct decoder *decoder)
{
	if (decoder->buffer_to_file_rx_filter)
	{
		int ret = plc_file_write_csv(ADC_FILE_CAPTURE_FILTER, sample_rx_csv_enum,
				decoder->buffer_to_file_rx_filter,
				decoder->buffer_to_file_rx_filter_cur - decoder->buffer_to_file_rx_filter, 0);
		assert(ret >= 0);
		free(decoder->buffer_to_file_rx_filter);
	}
	free(decoder->buffer_out_filter);
	decoder->buffer_out_filter = NULL;
	free(decoder->buffer_out_f);
	decoder->buffer_out_f = NULL;
	free(decoder->buffer_in_f);
	decoder->buffer_in_f = NULL;
	free(decoder->iir_b);
	decoder->iir_b = NULL;
	free(decoder->iir_a);
	decoder->iir_a = NULL;
}

uint32_t decoder_parse_next_samples_buffer(struct decoder *decoder, const sample_rx_t *buffer_in,
		uint8_t *buffer_data_out, uint32_t buffer_data_out_count)
{
	//
	// CHUNK_PUSH: Filtering
	// 
	int n;
	float *in_f = decoder->buffer_in_f + decoder->iir_b_count;
	float *out_f = decoder->buffer_out_f + decoder->iir_a_count;
	// Chain previous in/out values
	for (n = 0; n < decoder->iir_b_count; n++)
		decoder->buffer_in_f[n] = decoder->buffer_in_f[decoder->chunk_samples + n];
	for (n = 0; n < decoder->iir_a_count; n++)
		decoder->buffer_out_f[n] = decoder->buffer_out_f[decoder->chunk_samples + n];
	// Push new values
	// TODO: Automatic offset
	//	float buffer_offset = 0.0;
	//	for (n = 0; n < buffer_count; n++)
	//		buffer_offset += buffer_in[n];
	//	buffer_offset /= buffer_count;
	//	logger_log_sequence_format(logger, "Mean=%g\n", buffer_offset);
	// Demodulation
	for (n = 0; n < decoder->chunk_samples; n++)
	{
		in_f[n] = abs(buffer_in[n] - decoder->data_offset);
		out_f[n] = 0.0;
	}
	// Aplpy filter
	// Filtering algorithm (1-based):
	//	y[n]*iir_a[1] = Sum(k=0..M) [ iir_b[k+1]*x[n-k] ] - Sum(k=1..N) [ iir_a[k+1]*y[n-k] ]
	//	N=len(iir_a)-1; M=len(iir_b)-1
	// More info about filtering can be found in 'Octave' in 'doc filter'
	for (n = 0; n < decoder->chunk_samples; n++)
	{
		int k;
		for (k = 0; k < decoder->iir_b_count; k++)
			out_f[n] += decoder->iir_b[k] * in_f[n - k];
		for (k = 1; k < decoder->iir_a_count; k++)
			out_f[n] -= decoder->iir_a[k] * out_f[n - k];
		out_f[n] /= decoder->iir_a[0];
		// Quantification
		decoder->buffer_out_filter[n] = (sample_rx_t) round(out_f[n]);
	}

	//
	// CHUNK_SIGNAL_TO_DATA: Interpretation
	//
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
				if (*decoder->buffer_out_filter_cur >= decoder->demod_data_hi_threshold)
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
						// logger_log_sequence_format(logger, "[%u]",
						//		decoder->buffer_out_filter_cur-decoder->buffer_out_filter);
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
		if (*decoder->buffer_out_filter_cur >= decoder->demod_data_hi_threshold)
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
							>= decoder->demod_data_hi_threshold);
					for (pos = 0; pos < auto_synchro_margin; pos++)
					{
						if (candidate_symbol == 1)
						{
							if (*(decoder->buffer_out_filter_next - pos)
									< decoder->demod_data_hi_threshold)
								pos = -pos;
							if (*(decoder->buffer_out_filter_next + pos)
									>= decoder->demod_data_hi_threshold)
								continue;
							break;
						}
						else
						{
							if (*(decoder->buffer_out_filter_next - pos)
									>= decoder->demod_data_hi_threshold)
								pos = -pos;
							if (*(decoder->buffer_out_filter_next + pos)
									< decoder->demod_data_hi_threshold)
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
				assert((decoder->buffer_out_filter_next >= decoder->buffer_out_filter)
						&& (decoder->buffer_out_filter_next < decoder->buffer_out_filter_end));
			}
			// logger_log_sequence_format(logger, " %d<%u>", decoder->data_in_process,
			//	decoder->buffer_out_filter_next-decoder->buffer_out_filter);
			if (++decoder->bits_per_data_cur == decoder->bits_per_data)
			{
				decoder->bits_per_data_cur = 0;
				if (decoder->data_per_frame
						&& (++decoder->data_per_frame_cur == decoder->data_per_frame))
				{
					decoder->data_per_frame_cur = 0;
					decoder->start_bit_found = 0;
				}
				// logger_log_sequence_format(logger, "%02X ", decoder->data_in_process);
				/*
				 if (logger_api)
				 logger_api->log_char(logger_handle,
				 (decoder->data_in_process >=32)?decoder->data_in_process:'.');
				 */
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
	uint32_t data_decoded = decoder_parse_next_samples_buffer(decoder,
			buffer_in, buffer_data_out, buffer_data_out_count);
	if (decoder->buffer_to_file_rx_filter_remaining)
	{
		int samples_to_copy =
				(decoder->buffer_to_file_rx_filter_remaining <= decoder->chunk_samples) ?
						decoder->buffer_to_file_rx_filter_remaining : decoder->chunk_samples;
		memcpy(decoder->buffer_to_file_rx_filter_cur, decoder->buffer_out_filter,
				samples_to_copy * sizeof(sample_rx_t));
		decoder->buffer_to_file_rx_filter_cur += samples_to_copy;
		decoder->buffer_to_file_rx_filter_remaining -= samples_to_copy;
		if (decoder->buffer_to_file_rx_filter_remaining == 0)
			if (logger_api)
				logger_api->log_line(logger_handle, "RX-filter file captured");
	}
	return data_decoded;
}

// NOTE: if the flag '-fwhole-program' is used then the '__attribute__((externally_visible))' is
//	required to make it public
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

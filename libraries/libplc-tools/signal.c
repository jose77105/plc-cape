/**
 * @file
 * 
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include <math.h>		// round
#include "+common/api/+base.h"
#include "api/file.h"
#include "api/signal.h"

//
// FILTERING COEFICIENTS
//

// Coefficients got from 'Octave':
//	>> pkg load signal
//	>> [iir_b,iir_a] = butter(filter_size, 0.1)

// Pass-all filter
//	static const float iir_a[] = { 1.00000 };
//	static const float iir_b[] = { 1.00000 };

// butter(1, 0.1)
//	static const float iir_a[] = { 1.00000, -0.72654 };
//	static const float iir_b[] = { 0.13673, 0.13673 };

// butter(2, 0.1)
//	static const float iir_a[] = { 1.00000, -1.56102, 0.64135 };
//	static const float iir_b[] = { 0.020083, 0.040167, 0.020083 };

// butter(3, 0.1)
//	static const float iir_a[] = { 1.00000, -2.37409, 1.92936, -0.53208};
//	static const float iir_b[] = { 0.0028982, 0.0086946, 0.0086946, 0.0028982};

// TODO: Make ADC_FILE_CAPTURE_FILTER a configurable setting
#define ADC_FILE_CAPTURE_FILTER "adc_filter.csv"

struct plc_signal_iir
{
	float *buffer_in_f;
	float *buffer_out_f;
	uint32_t chunk_samples;
	// Demodulation parameters
	uint32_t samples_counter;
	enum plc_signal_iir_demodulator_enum demodulator;
	float demodulation_frequency;
	// IIR filtering parameters
	float *iir_a;
	uint32_t iir_a_count;
	float *iir_b;
	uint32_t iir_b_count;
	// sample_to_file option
	float *buffer_to_file_rx_filter;
	uint32_t buffer_to_file_rx_filter_remaining;
	float *buffer_to_file_rx_filter_cur;
};

ATTR_EXTERN struct plc_signal_iir *plc_signal_iir_create(uint32_t chunk_samples, const float *iir_a,
		uint32_t iir_a_count, const float *iir_b, uint32_t iir_b_count)
{
	struct plc_signal_iir *plc_signal_iir = calloc(1, sizeof(struct plc_signal_iir));
	memset(plc_signal_iir, 0, sizeof(*plc_signal_iir));
	plc_signal_iir->buffer_in_f = malloc((chunk_samples + iir_b_count) * sizeof(float));
	plc_signal_iir->buffer_out_f = malloc((chunk_samples + iir_a_count) * sizeof(float));
	plc_signal_iir->chunk_samples = chunk_samples;
	uint32_t iir_a_size = sizeof(*iir_a) * iir_a_count;
	plc_signal_iir->iir_a = (float*) malloc(iir_a_size);
	plc_signal_iir->iir_a_count = iir_a_count;
	memcpy(plc_signal_iir->iir_a, iir_a, iir_a_size);
	uint32_t iir_b_size = sizeof(*iir_b) * iir_b_count;
	plc_signal_iir->iir_b = (float*) malloc(iir_b_size);
	plc_signal_iir->iir_b_count = iir_b_count;
	memcpy(plc_signal_iir->iir_b, iir_b, iir_b_size);
	int n;
	for (n = 0; n < plc_signal_iir->iir_b_count; n++)
		plc_signal_iir->buffer_in_f[n] = 0.0;
	for (n = 0; n < plc_signal_iir->iir_a_count; n++)
		plc_signal_iir->buffer_out_f[n] = 0.0;
	return plc_signal_iir;
}

ATTR_EXTERN void plc_signal_iir_release(struct plc_signal_iir *plc_signal_iir)
{
	if (plc_signal_iir->buffer_to_file_rx_filter)
	{
		int ret = plc_file_write_csv(ADC_FILE_CAPTURE_FILTER, csv_float,
				plc_signal_iir->buffer_to_file_rx_filter,
				plc_signal_iir->buffer_to_file_rx_filter_cur
						- plc_signal_iir->buffer_to_file_rx_filter, 0);
		assert(ret >= 0);
		free(plc_signal_iir->buffer_to_file_rx_filter);
	}
	free(plc_signal_iir->iir_b);
	free(plc_signal_iir->iir_a);
	free(plc_signal_iir->buffer_out_f);
	free(plc_signal_iir->buffer_in_f);
	free(plc_signal_iir);
}

ATTR_EXTERN float* plc_signal_get_buffer_out(struct plc_signal_iir *plc_signal_iir)
{
	return plc_signal_iir->buffer_out_f + plc_signal_iir->iir_a_count;
}

ATTR_EXTERN void plc_signal_iir_set_samples_to_file(struct plc_signal_iir *plc_signal_iir,
		uint32_t samples_to_file)
{
	plc_signal_iir->buffer_to_file_rx_filter_remaining = samples_to_file;
	if (samples_to_file)
		plc_signal_iir->buffer_to_file_rx_filter = malloc(samples_to_file * sizeof(float));
	plc_signal_iir->buffer_to_file_rx_filter_cur = plc_signal_iir->buffer_to_file_rx_filter;

}

ATTR_EXTERN void plc_signal_iir_set_demodulator(struct plc_signal_iir *plc_signal_iir,
		enum plc_signal_iir_demodulator_enum demodulator)
{
	plc_signal_iir->demodulator = demodulator;
}

ATTR_EXTERN void plc_signal_iir_set_demodulation_frequency(struct plc_signal_iir *plc_signal_iir,
		float digital_frequency)
{
	plc_signal_iir->demodulation_frequency = digital_frequency;
}

ATTR_EXTERN void plc_signal_iir_reset(struct plc_signal_iir *plc_signal_iir)
{
	plc_signal_iir->samples_counter = 0;
}

ATTR_EXTERN void plc_signal_iir_process_chunk(struct plc_signal_iir *plc_signal_iir,
		const sample_rx_t *buffer_in, sample_rx_t buffer_in_offset)
{
	uint32_t n;
	float *in_f = plc_signal_iir->buffer_in_f + plc_signal_iir->iir_b_count;
	float *out_f = plc_signal_iir->buffer_out_f + plc_signal_iir->iir_a_count;
	//
	// Chain previous in/out values
	//
	for (n = 0; n < plc_signal_iir->iir_b_count; n++)
		plc_signal_iir->buffer_in_f[n] = plc_signal_iir->buffer_in_f[plc_signal_iir->chunk_samples
				+ n];
	for (n = 0; n < plc_signal_iir->iir_a_count; n++)
		plc_signal_iir->buffer_out_f[n] = plc_signal_iir->buffer_out_f[plc_signal_iir->chunk_samples
				+ n];
	//
	// Demodulation
	//
	switch (plc_signal_iir->demodulator)
	{
	case plc_signal_iir_demodulator_none:
		break;
	case plc_signal_iir_demodulator_abs:
		for (n = 0; n < plc_signal_iir->chunk_samples; n++)
		{
			in_f[n] = abs((int16_t) (buffer_in[n] - buffer_in_offset));
			out_f[n] = 0.0;
		}
		break;
	case plc_signal_iir_demodulator_cos:
	{
		// TODO: For each symbol synchronize the carrier phase
		//	If we use a continuous 'sin' carrier we can use a 'phase_synchronization = -M_PI / 2'
		float phase_synchronization = -M_PI / 2;
		for (n = 0; n < plc_signal_iir->chunk_samples; n++, plc_signal_iir->samples_counter++)
		{
			in_f[n] = (float) ((int16_t) (buffer_in[n]) - (int16_t) (buffer_in_offset))
					* cos(
							2 * M_PI * plc_signal_iir->demodulation_frequency
									* plc_signal_iir->samples_counter + phase_synchronization);
			out_f[n] = 0.0;
		}
		break;
	}
	}
	//
	// Apply IIR filtering (https://en.wikipedia.org/wiki/Infinite_impulse_response).
	// Typical formula (got from Octave >> 'doc filter'):
	//
	//   N                   M
	//	SUM a(k) y(n-k) = SUM b(k) x(n-k)    for 0 <= n <= len(x)-1
	//	k=0                 k=0
	//
	// Or in y[n] terms:
	//
	//            N                   M
	//  y(n) = - SUM c(k) y(n-k) + SUM d(k) x(n-k)  for 0 <= n <= len(x)-1
	//           k=1                 k=0
	//
    // where c = a/a(0), d = b/a(0), N = len(iir_a)-1, M = len(iir_b)-1.
	//
	for (n = 0; n < plc_signal_iir->chunk_samples; n++)
	{
		int k;
		for (k = 0; k < plc_signal_iir->iir_b_count; k++)
			out_f[n] += plc_signal_iir->iir_b[k] * in_f[n - k];
		for (k = 1; k < plc_signal_iir->iir_a_count; k++)
			out_f[n] -= plc_signal_iir->iir_a[k] * out_f[n - k];
		out_f[n] /= plc_signal_iir->iir_a[0];
	}
	//
	// Record to file
	//
	if (plc_signal_iir->buffer_to_file_rx_filter_remaining)
	{
		int samples_to_copy =
				(plc_signal_iir->buffer_to_file_rx_filter_remaining <= plc_signal_iir->chunk_samples) ?
						plc_signal_iir->buffer_to_file_rx_filter_remaining :
						plc_signal_iir->chunk_samples;
		memcpy(plc_signal_iir->buffer_to_file_rx_filter_cur, out_f,
				samples_to_copy * sizeof(float));
		plc_signal_iir->buffer_to_file_rx_filter_cur += samples_to_copy;
		plc_signal_iir->buffer_to_file_rx_filter_remaining -= samples_to_copy;
	}
}

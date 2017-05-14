/**
 * @file
 * @brief	**Main** file
 *
 * @see		@ref application-plc-cape-freq-response
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
 *	Copyright (C) 2017 Jose Maria Ortega
 * 
 * @endcond
 */

#define _GNU_SOURCE					// Required for 'asprintf' declaration
#include <ctype.h>					// toupper
#include <errno.h>					// errno
#include <fftw3.h>					// fftw_execute
#include <math.h>					// sin
#include <unistd.h>					// sleep
#include "+common/api/+base.h"
#include "+common/api/bbb.h"		// ADC_MAX_CAPTURE_RATE_SPS
#include "libraries/libplc-adc/api/adc.h"
#include "libraries/libplc-cape/api/afe.h"
#include "libraries/libplc-cape/api/cape.h"
#include "libraries/libplc-cape/api/tx.h"
#include "libraries/libplc-tools/api/cmdline.h"
#include "libraries/libplc-tools/api/file.h"
#include "libraries/libplc-tools/api/time.h"
#include "libraries/libplc-tools/api/trace.h"

// Enable VERBOSE for some extra debugging information
//	#define VERBOSE

#define FILE_ADC_CAPTURE "adc.csv"
#define FILE_ADC_CAPTURE_FFT "adc_fft.csv"
#define FILE_FREQ_RESPONSE "freq_response.csv"

#define PLC_DRIVERS 1

#define SAMPLES_TX_BUFFER_COUNT 50000
#define SAMPLE_RX_BUFFER_COUNT 10000
#define TX_DMA_BUFFER_COUNT 1024
#define RX_ADC_BUFFER_COUNT 2048

static const int timed_tx = 0;
static const int write_fft_to_file = 0;

enum progress_notification_enum
{
	progress_notification_none = 0,
	progress_notification_dots,
	progress_notification_clock
};
static const enum progress_notification_enum tx_progress_notification_mode =
		progress_notification_none;

static enum plc_tx_device_enum plc_tx_device;
static enum plc_rx_device_enum plc_rx_device;
static float freq_dac_requested_sps;
static float freq_adc_requested_sps;
static float freq_ini, freq_end, freq_ref;
static float adc_mean_for_transfer_one = 0.0;
static uint32_t freq_iterations;
static enum afe_calibration_enum afe_calibration;
static int afe_cenelec_a = 0;
static enum afe_gain_tx_pga_enum afe_gain_tx_pga;
static enum afe_gain_rx_pga1_enum afe_gain_rx_pga1;
static enum afe_gain_rx_pga2_enum afe_gain_rx_pga2;
static sample_tx_t dac_max;
static float dac_min;
static float dac_range;
static float dac_offset;

static struct plc_cape *plc_cape = NULL;
static sample_rx_t *samples_adc = NULL;
static sample_rx_t *samples_adc_cur = NULL;
static sample_rx_t *samples_adc_end = NULL;
static sample_rx_t *samples_adc_data_to_save = NULL;
static float samples_adc_data_to_save_freq = 0.0;

// Marked volatile because to avoid single-thread optimizations (as operated from different threads)
static volatile int samples_adc_filled = 0;
static float freq_dac_effective_sps;
static float freq_adc_effective_sps;

// Volatile to avoid compiler optimizations
volatile static float wave_freq_rad;
static uint32_t tx_counter = 0;

// For on-the-fly loading use 'tx_preloading = 0' but more warnings coming from the driver are
// probable
static const int tx_preloading = 1;
static sample_tx_t *samples_tx_preloaded = NULL;
static sample_tx_t *samples_tx_preloaded_cur = NULL;
static sample_tx_t *samples_tx_preloaded_end = NULL;

// Discard first memorized buffer to avoid some possible initialization transition period
static int capture_iteration;
// TODO: Analyze why a few ADC iterations need to be discarded for proper execution
#define CAPTURE_ITERATIONS_TO_DISCARD 2

#ifdef DEBUG
// To allow TRACE macros declare 'plc_debug_level' and 'plc_trace'
int plc_debug_level = 3;
void (*plc_trace)(const char *function_name, const char *format, ...) = plc_trace_default;
#endif

// '--help' message
static const char usage_message[] = "Usage: plc-cape-freq-response [OPTIONS]\n"
		"Tool to get the frequency response H(f) of a given TX-RX path via the PlcCape board\n\n"
		"  -A:FREQUENCY  Initial frequency [Hz]\n"
		"  -B:FREQUENCY  Final frequency [Hz]\n"
		"  -C:COUNT      Number of sub-intervals between the initial and final frequencies\n"
		"  -D:FREQUENCY  Reference frequency in which to also store the captured samples\n"
		"  -F:FILTER     0 for CENELEC BCD, 1 for CENELEC A\n"
		"  -H:ADC_RANGE  Range (peak-to-peak) used for the H(f)=1 reference\n"
		"  -O:DAC_VALUE  Offset value [in DAC units]\n"
		"  -R:DAC_RANGE  Range (peak-to-peak) value [in DAC units]\n"
		"  -T:MODE       Calibration mode index [0 to 3]:\n"
		"                  0:none; 1:tx+tx_filter; 2:tx+tx_filter+rx_filter+rx_pga2; 3:tx\n"
		"  -U:GAIN       TX_PGA gain index [0 to 3: 0.25, 0.5, 0.71, 1.0]\n"
		"  -V:GAIN       RX_PGA1 gain index [0 to 3: 0.25, 0.5, 1.0, 2.0]\n"
		"  -W:GAIN       RX_PGA2 gain index [0 to 3: 1, 4, 16, 64]\n"
		"     --help     display this help and exit\n\n"
		"For the arguments requiring an index from a list of options you can get more\n"
		"information specifiying the parameter followed by just a colon\n";

void pexit(const char *msg)
{
	fprintf(stderr, msg);
	exit(EXIT_FAILURE);
}

void cmdline_parse_args(int argc, char *argv[])
{
	int n;
	for (n = 1; n < argc; n++)
		if (strcmp(argv[n], "--help") == 0)
		{
			printf(usage_message);
			exit(EXIT_SUCCESS);
		}
	int c;
	char *error_msg = NULL;
	while ((c = getopt(argc, argv, "A:B:C:D:F:H:O:R:T:U:V:W:")) != -1)
		switch (c)
		{
		case 'A':
			freq_ini = atoi(optarg + 1);
			break;
		case 'B':
			freq_end = atoi(optarg + 1);
			break;
		case 'C':
			freq_iterations = atoi(optarg + 1);
			break;
		case 'D':
			freq_ref = atoi(optarg + 1);
			break;
		case 'H':
			adc_mean_for_transfer_one = atoi(optarg + 1)/M_PI; 
			break;
		case 'F':
			afe_cenelec_a = (atoi(optarg + 1) == 1);
			break;
		case 'O':
			dac_offset = atoi(optarg + 1);
			break;
		case 'R':
			dac_range = atoi(optarg + 1);
			break;
		case 'T':
			if ((error_msg = plc_cmdline_set_enum_value_with_checking(optarg, (int*) &afe_calibration,
					"AFE CALIBRATION", afe_calibration_enum_text, afe_calibration_COUNT)) != NULL)
				pexit(error_msg);
			break;
		case 'U':
			if ((error_msg = plc_cmdline_set_enum_value_with_checking(optarg, (int*) &afe_gain_tx_pga,
					"TX_PGA", afe_gain_tx_pga_enum_text, afe_gain_tx_pga_COUNT)) != NULL)
				pexit(error_msg);
			break;
		case 'V':
			if ((error_msg = plc_cmdline_set_enum_value_with_checking(optarg, (int*) &afe_gain_rx_pga1,
					"RX_PGA1", afe_gain_rx_pga1_enum_text, afe_gain_rx_pga1_COUNT)) != NULL)
				pexit(error_msg);
			break;
		case 'W':
			if ((error_msg = plc_cmdline_set_enum_value_with_checking(optarg, (int*) &afe_gain_rx_pga2,
					"RX_PGA2", afe_gain_rx_pga2_enum_text, afe_gain_rx_pga2_COUNT)) != NULL)
				pexit(error_msg);
			break;
		case ':':
			fprintf(stderr, "Option -%c requires an argument\n", optopt);
			break;
		case '?':
			// Unknown option. The proper message should have been already printed by getopt
			exit(EXIT_FAILURE);
		default:
			// The 'default' should never be reached
			assert(0);
		}
	if (optind < argc)
	{
		fprintf(stderr, "Non-option argument %s\n", argv[optind]);
		exit(EXIT_FAILURE);
	}
}

void set_test_defaults(void)
{
	if (plc_cape_in_emulation(plc_cape))
	{
		static const float FREQ_DAC_ALSA_SPS = 8000.0;
		static const float FREQ_ADC_ALSA_SPS = 8000.0;
		plc_tx_device = plc_tx_device_alsa;
		freq_dac_requested_sps = FREQ_DAC_ALSA_SPS;
		plc_rx_device = plc_rx_device_alsa;
		freq_adc_requested_sps = FREQ_ADC_ALSA_SPS;
		freq_iterations = 10;
		freq_ini = (FREQ_DAC_ALSA_SPS / 2) / freq_iterations;
		freq_end = FREQ_DAC_ALSA_SPS / 2;
		freq_ref = FREQ_DAC_ALSA_SPS / 4;
		afe_calibration = afe_calibration_none;
		dac_offset = 512.0;
		dac_range = 1022.0;
	}
	else
	{
		static const float FREQ_DAC_BBB_SPS = 500000.0;
		static const float FREQ_ADC_BBB_SPS = ADC_MAX_CAPTURE_RATE_SPS;
		plc_tx_device = plc_tx_device_plc_cape;
		freq_dac_requested_sps = FREQ_DAC_BBB_SPS;
		plc_rx_device = plc_rx_device_adc_bbb;
		freq_adc_requested_sps = FREQ_ADC_BBB_SPS;
		freq_iterations = 41;
		freq_ini = 50.0;
		freq_end = 200050.0;
		freq_ref = 50000.0;
		afe_calibration = afe_calibration_dac_txpga_rxpga1_rxfilter_rxpga2;
		dac_offset = 512.0;
		dac_range = 1022.0;
	}
	// AFE DAC = 10-bits = 1024 values
	dac_max = 1023.0;
	dac_min = 0.0;
	afe_gain_tx_pga = afe_gain_tx_pga_100;
	// NOTE: I have tested with different values of AFE_GAIN_RX_PGA1 but are ignored!!
	//	It seems it is not in the calibration path (bug on AFE datasheet?)
	afe_gain_rx_pga1 = afe_gain_rx_pga1_025;
	afe_gain_rx_pga2 = afe_gain_rx_pga2_1;
}

// PROCONDITION: 'buffer_count <= samples_tx_preloaded_end-samples_tx_preloaded'
void tx_fill_cycle_callback_preloaded(void *handle, sample_tx_t *buffer, uint32_t buffer_count)
{
	assert(buffer_count <= samples_tx_preloaded_end - samples_tx_preloaded);
	sample_tx_t *samples_tx_preloaded_next = samples_tx_preloaded_cur + buffer_count;
	if (samples_tx_preloaded_next <= samples_tx_preloaded_end)
	{
		memcpy(buffer, samples_tx_preloaded_cur, buffer_count * sizeof(sample_tx_t));
		samples_tx_preloaded_cur = samples_tx_preloaded_next;
	}
	else
	{
		uint32_t ending_samples = samples_tx_preloaded_end - samples_tx_preloaded_cur;
		memcpy(buffer, samples_tx_preloaded_cur, ending_samples * sizeof(sample_tx_t));
		uint32_t initial_samples = buffer_count - ending_samples;
		memcpy(buffer + ending_samples, samples_tx_preloaded,
				initial_samples * sizeof(sample_tx_t));
		samples_tx_preloaded_cur = samples_tx_preloaded + initial_samples;
	}
}

void tx_fill_cycle_callback(void *handle, sample_tx_t *buffer, uint32_t buffer_count)
{
	float dac_amp = dac_range/2.0;
	for (; buffer_count > 0; buffer_count--, tx_counter++)
	{
		float sample = round(dac_offset + dac_amp * sin(wave_freq_rad * tx_counter));
		if (sample > dac_max) sample = dac_max;
		else if (sample < dac_min) sample = dac_min;
		*buffer++ = (sample_tx_t) sample;
	}
}

void tx_on_buffer_sent_callback(void *handle, sample_tx_t *buffer, uint32_t buffer_count)
{
	switch (tx_progress_notification_mode)
	{
	case progress_notification_none:
		break;
	case progress_notification_dots:
		putc('.', stdout);
		// 'fflush' required to have live progress information
		// Otherwise the output is cached until '\n'
		fflush(stdout);
		break;
	case progress_notification_clock:
	{
		static int index = 0;
		static const char pattern[] = {
			'-', '\\', '|', '/' };
		static uint32_t stamp_pattern = 0;
		uint32_t stamp = plc_time_get_tick_ms();
		// 125 ms per 8 symbols = 1 second per cycle
		if (stamp - stamp_pattern >= 125)
		{
			stamp_pattern = stamp;
			// Progress: draw an evolving symbol + move cursor backwards
			printf("%c\033[D", pattern[(index++) % 0x3]);
			fflush(stdout);
		}
		break;
	}
	}
}

void test_valid_pointer_errno(void *pointer, const char *component)
{
	if (pointer == NULL)
	{
		char *error_msg;
		asprintf(&error_msg, "Error while initializing the '%s' component: %s\n", component,
				strerror(errno));
		pexit(error_msg);
	}
}

static void on_buffer_completed(void *handle, sample_rx_t *samples_buffer,
		uint32_t samples_buffer_count)
{
	if ((capture_iteration++ < CAPTURE_ITERATIONS_TO_DISCARD) || samples_adc_filled)
		return;
	int samples_to_copy = samples_adc_end - samples_adc_cur;
	if (samples_to_copy > samples_buffer_count)
		samples_to_copy = samples_buffer_count;
	memcpy(samples_adc_cur, samples_buffer, samples_to_copy * sizeof(sample_rx_t));
	samples_adc_cur += samples_to_copy;
	if (samples_adc_cur == samples_adc_end)
		samples_adc_filled = 1;
}

// A possible way to get the frequency response of a system is by calculating the gain factor
// applied to a range of pure frequencies (sinusoids).
// To calculate the gain for a given frequency we can measure the amplitude of the captured signal
// over a long-enough period (for accuracy) following these steps:
//	1. Eliminate the DC component
//	2. Accumulate the absolute value of the samples
//	3. Get the magnitude of the DC component by simple averaging
void calculate_means(uint16_t *buffer, uint32_t buffer_count, float *dc_mean, float *ac_mean)
{
	int i;
	// For compulational effort convenience the dc_mean is calculate accumulating 'uint16_t' samples
	// in a 'uint32_t'. This may overflow. Use a safety assert.
	// TODO: Relax the assert. Adding a bits-used value (12-bits instead of 16-bits) would allow
	//	relaxing the buffer_count (4-bits more)
	assert(buffer_count < 0x10000);
	uint32_t mean_value_int = 0;
	*dc_mean = 0.0;
	uint16_t *buffer_ini = buffer;
	for (i = buffer_count; i > 0; i--)
		mean_value_int += *buffer++;
	*dc_mean = (float) mean_value_int / buffer_count;
	*ac_mean = 0.0;
	buffer = buffer_ini;
	for (i = buffer_count; i > 0; i--)
		*ac_mean += abs((float) (*buffer++) - *dc_mean);
	*ac_mean /= buffer_count;
}

static void calculate_fft(uint16_t *buffer, uint32_t buffer_count, float freq)
{
	// FFTW
	fftw_complex *in, *out;
	fftw_plan p;
	in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * buffer_count);
	out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * buffer_count);
	p = fftw_plan_dft_1d(buffer_count, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
	uint32_t n;
	for (n = 0; n < buffer_count; n++)
	{
		in[n][0] = buffer[n];
		in[n][1] = 0;
	}
	fftw_execute(p);
	// Search the max value
	float *samples_fft = malloc(buffer_count * sizeof(float));
	float samples_fft_max_value = 0.0;
	uint32_t samples_fft_max_index = 0;
	// Ignore the continuous component (freq = 0)
	samples_fft[0] = 0.0;
	for (n = 1; n < buffer_count; n++)
	{
		samples_fft[n] = sqrt(out[n][0] * out[n][0] + out[n][1] * out[n][1]);
		if (samples_fft[n] > samples_fft_max_value)
		{
			samples_fft_max_value = samples_fft[n];
			samples_fft_max_index = n;
		}
	}
	// Save file
	if (write_fft_to_file)
	{
		int ret = plc_file_write_csv(FILE_ADC_CAPTURE_FFT, csv_float, samples_fft, buffer_count, 0);
		assert(ret >= 0);
		printf("Abs[FFT(ADC samples)] stored in '%s' [%d samples]\n", FILE_ADC_CAPTURE_FFT,
				buffer_count);
	}
	// Display max value
	printf("FFT max expected at index %.2f (freq %.2f Hz)\n"
			"FFT max at index %d (freq %.2f Hz), abs(H)/N = %.2f\n"
			"NOTE: FFT steps = %.2f Hz\n",
			freq * buffer_count / freq_adc_effective_sps, freq,
			samples_fft_max_index, freq_adc_effective_sps * samples_fft_max_index / buffer_count,
			samples_fft_max_value / buffer_count,
			freq_adc_effective_sps / buffer_count);
	// Release objects
	free(samples_fft);
	fftw_destroy_plan(p);
	fftw_free(in);
	fftw_free(out);
}

// PURPOSE: Calculate the minimum best multiple period (in samples) minimizing the gap between
//	cycles considering the available buffer
uint32_t calculate_best_number_of_cycles(float wave_period_samples, uint32_t samples_buffer_count)
{
	float integral;
	uint32_t max_number_of_cycles_within_buffer = floor(
			(float) samples_buffer_count / wave_period_samples);
	// Note that there is another condition that should be met for simplicity in the implementation:
	// per precondition in 'tx_fill_cycle_callback_preloaded' there must be more preloaded data than
	// TX_DMA_BUFFER_COUNT
	uint32_t wave_period_close_to_zero_cycles = ceil(
			(float) TX_DMA_BUFFER_COUNT / wave_period_samples);
	// Initially 'wave_period_close_to_zero_delta' with a maximum value
	float wave_period_close_to_zero_delta = 1.0;
	int i;
	for (i = wave_period_close_to_zero_cycles; i <= max_number_of_cycles_within_buffer; i++)
	{
		// +0.5 & -0.5 is to consider both close_to_zero: above & below
		// Thus 0.9 will be converted to -0.1
		float delta = fabs(modff(wave_period_samples * i + 0.5, &integral) - 0.5);
		if (delta <= wave_period_close_to_zero_delta)
		{
			wave_period_close_to_zero_delta = delta;
			wave_period_close_to_zero_cycles = i;
		}
	}
	uint32_t wave_close_to_zero_samples = (uint32_t) round(
			wave_period_samples * wave_period_close_to_zero_cycles);
#ifdef VERBOSE
	printf("Wave period %f; Max cycles = %d; Best cycles = %d; Best samples = %d samples; "
			"Chaining gap = %f\n",
			wave_period_samples, max_number_of_cycles_within_buffer,
			wave_period_close_to_zero_cycles, wave_close_to_zero_samples,
			wave_period_close_to_zero_delta);
#endif
	return wave_close_to_zero_samples;
}

void execute_test(void)
{
	float dac_to_adc_factor, dac_to_adc_offset_zero, dac_to_adc_offset_factor;
	float dac_freq_hz, dac_freq_step;
	int ret;
	printf("SETUP\n-----\n");
	static const float dac_to_adc_factor_multiplier_tx_pga[afe_gain_tx_pga_COUNT] =
		{ 0.25, 0.5, 0.71, 1.0 };
	static const float dac_to_adc_factor_multiplier_rx_pga1[afe_gain_rx_pga1_COUNT] =
		{ 0.25, 0.5, 1.0, 2.0 };
	static const float dac_to_adc_factor_multiplier_rx_pga2[afe_gain_rx_pga2_COUNT] =
		{ 1.0, 4.0, 16.0, 64.0 };
	switch (afe_calibration)
	{
	case afe_calibration_none:
		//
		// To find the 'dac_to_adc_factor' in 'afe_calibration_none' I have bridged in 'PlcCape-v1'
		// TX_F_OUT with TXRX (and removed C12 to isolate the loop) and executed a test with these
		// parameters: TX_PGA = 0.25 (to avoid any saturation), RX_PGA1 = 1.00, RX_PGA2 = 1.
		// I use RX_PGA1 = 1.0 because in the calibration mode test RX_PGA1 doesn't partipate
		// This means the following command line:
		//	-T:0 -A:50 -B:200050 -C:21 -D:50050 -F:0 -O:600 -R:800 -U:0 -V:2 -W:0
		// The result for 50k gives a DC = 1914 and an AC Range ~316
		// This means a dac_to_adc_factor = 316/(800*0.25) = 1.58
		//
		dac_to_adc_factor = 1.58 * dac_to_adc_factor_multiplier_tx_pga[afe_gain_tx_pga]
			* dac_to_adc_factor_multiplier_rx_pga1[afe_gain_rx_pga1]
			* dac_to_adc_factor_multiplier_rx_pga2[afe_gain_rx_pga2];
		dac_to_adc_offset_zero = 1912.0;
		dac_to_adc_offset_factor = 0.0;
		break;
	case afe_calibration_dac_txpga_rxpga1_rxfilter_rxpga2:
		//
		// For 'dac_to_adc_offset_zero' it would corresponds to half the working voltage:
		//	In AFE031 half working voltage = 3.3V/2 = 1.65V
		//	In BBB ADC 12-bits max range [4096] = 1.8V
		//	ADCIN from AFE031 to BBB traverses a 1/2 voltage divider (1.65/2)
		//	Offset = (1.65/2)*4096/1.8 = 1877
		// The real values are got from experimentation. Note that the resistor voltage divider is
		// not perfect, so tolerate some differences:
		// * In PlcCape-v1: 1912
		//
		// For the -T:2 -U:0 -R:800 -O:620 case -> DC = 1912. AC Range ~ 700
		// A DAC range of 800 with TX_PGA1 = 0.25 results in ADC Range = 700
		// This means a dac_to_adc_factor = 700/(800*0.25) = 3.5
		//
		dac_to_adc_factor = 3.5 * dac_to_adc_factor_multiplier_tx_pga[afe_gain_tx_pga]
			* dac_to_adc_factor_multiplier_rx_pga2[afe_gain_rx_pga2];
		dac_to_adc_offset_zero = 1912.0;
		// When the RX Chain is enabled there is a series capacitor that removes DC
		dac_to_adc_offset_factor = 0.0;
		break;
	case afe_calibration_dac_txpga:
		//
		// Calibration modes for 'dac_txpga*' are more caotic
		// I have checked with a continuous signals here (no series capacitor) changing the TX_PGA
		// gain (-T) and offset (-O) zero and 1023 and I have got:
		//	./plc-cape-freq-response -C:1 -D:10000 -T:3 -U:0 -R:0 -O:1023
		//
		//		-U:0 0.25: 0 = 9; 220 = 10; 230 = 20; 1023 = 1336 -> Lo-saturation until 220
		//		-U:1 0.50: 0 = 9; 540 = 10; 550 = 42; 1023 = 2005 -> Lo-Saturation until 540
		//		-U:2 0.71: 0 = 9; 630 = 10; 640 = 45; 1023 = 2350 -> Lo-saturation until 640
		//		-U:3 1.00: 0 = 28; 700 = 12; 710 = 294; 1023 = 2668 -> Lo-saturation until 710
		// 
		// So, params aprox recommended here for no saturation and maximum ADC range:
		//		-T:3 -U:0 -R:800 -O:620
		//		-T:3 -U:1 -R:480 -O:780
		//		-T:3 -U:2 -R:380 -O:830
		//		-T:3 -U:3 -R:310 -O:860
		//
		// Anyway, for bigger gains (-U:3) even if no saturation there is noticeable distorsion
		// 'dac_to_adc_offset_factor' and 'dac_to_adc_offset_zero' should be evaluated
		//
		// For the -T:3 -U:0 -R:800 -O:620 case -> DC = 670. AC Range ~ 1249
		// dac_to_adc_factor = 1249/(800*0.25) = 6.2
		//
		dac_to_adc_factor = 6.2 * dac_to_adc_factor_multiplier_tx_pga[afe_gain_tx_pga];
		dac_to_adc_offset_zero = 0.0;
		dac_to_adc_offset_factor = dac_to_adc_factor;
		break;
	case afe_calibration_dac_txpga_txfilter:
		//
		//	./plc-cape-freq-response -C:1 -D:10000 -T:1 -U:0 -R:0 -O:1023
		//
		//		-U:0 0.25: 0 = 1083; 100 = 1151; 923 = 1791; 1023 = 1865 -> OK full range
		//		-U:1 0.50: 0 = 741; 100 = 877; 923 = 2085; 1023 = 2212 -> OK full range
		//		-U:2 0.71: 0 = 449; 100 = 646; 923 = 2294; 950 = 2329; 990 = 2343; 1023 = 2347
		//			-> Hi-Saturation from 950
		//		-U:3 1.00: 0 = 28; 100 = 311; 820 = 2327; 850 = 2343; 923 = 2347; 1023 = 2348
		//			-> Hi-saturaion from 850
		//
		// So, params aprox recommended here for no saturation and maximum ADC range:
		//		-T:1 -U:0 -R:1022 -O:511
		//		-T:1 -U:1 -R:1022 -O:511
		//		-T:1 -U:2 -R:940 -O:470
		//		-T:1 -U:3 -R:840 -O:420
		//
		// For the -T:1 -U:0 -R:800 -O:620 case -> DC = 1551. AC Range ~ 637
		// dac_to_adc_factor = 637/(800*0.25) = 3.2
		//
		dac_to_adc_factor = 3.2 * dac_to_adc_factor_multiplier_tx_pga[afe_gain_tx_pga];
		dac_to_adc_offset_zero = 0.0;
		dac_to_adc_offset_factor = dac_to_adc_factor;
		break;
	default:
		assert(0);
	}
	if (freq_iterations <= 1)
	{
		freq_iterations = 1;
		dac_freq_hz = freq_ref;
		printf("Test: freq = %u Hz\n", (uint32_t) round(freq_ref));
	}
	else
	{
		dac_freq_hz = freq_ini;
		dac_freq_step = (float) (freq_end - freq_ini) / (freq_iterations - 1);
		printf("Test: f_ini = %u Hz, f_end = %u Hz, iterations = %u\n",
				(uint32_t) round(freq_ini), (uint32_t) round(freq_end),
				(uint32_t) round(freq_iterations));
	}
	float wave_ac_mean = dac_range / M_PI;
	printf("DAC values (in DAC units)\n  Offset = %f, Range = %f, AC Mean = %f\n", dac_offset,
		dac_range, wave_ac_mean);
	// For a sinusoid the AC mean (integral = of the whole 'abs' signal) is the integral of half a
	// positive cycle:
	//	INT[sin(x)] = -cos(x) + C -> Mean = INT[A*sin in 1/2 cycle]/PI = A*[-cos(pi)-(-cos(0))]
	//		= A*(1+1)/PI = A*2/PI
	float ac_mean_expected = wave_ac_mean * dac_to_adc_factor;
	printf("EXPECTED ADC values [in ADC units]\n  DC mean = %f, Range = %f, AC mean = %f\n",
			dac_to_adc_offset_zero + dac_offset * dac_to_adc_offset_factor,
			dac_range*dac_to_adc_factor, ac_mean_expected);
	if ((round(dac_offset + dac_range/2.0) > dac_max) || (round(dac_offset - dac_range/2.0) < dac_min))
		printf("WARNING! DAC values will overflow the available range (saturation effect)\n");
	struct plc_afe *plc_afe = plc_cape_get_afe(plc_cape);
	assert(plc_afe);
	static const uint32_t tx_dma_buffer_count = TX_DMA_BUFFER_COUNT;
	struct plc_tx *plc_tx = plc_tx_create(plc_tx_device,
			tx_preloading ? tx_fill_cycle_callback_preloaded : tx_fill_cycle_callback,
			NULL, tx_on_buffer_sent_callback, NULL, spi_tx_mode_ping_pong_dma, freq_dac_requested_sps,
			tx_dma_buffer_count, plc_afe);
	test_valid_pointer_errno(plc_afe, "TX");
	freq_dac_effective_sps = plc_tx_get_effective_sampling_rate(plc_tx);
	printf("Sampling rate = %u ksps\n", (uint32_t) round(freq_dac_effective_sps / 1000));
	struct plc_adc *plc_adc = plc_adc_create(plc_rx_device);
	test_valid_pointer_errno(plc_afe, "ADC");
	static const uint32_t samples_tx_preloaded_count = SAMPLES_TX_BUFFER_COUNT;
	samples_tx_preloaded = malloc(sizeof(sample_tx_t) * samples_tx_preloaded_count);
	static const uint32_t samples_adc_count = SAMPLE_RX_BUFFER_COUNT;
	samples_adc = malloc(sizeof(sample_rx_t) * samples_adc_count);
	samples_adc_cur = samples_adc;
	samples_adc_end = samples_adc + samples_adc_count;
	samples_adc_data_to_save = calloc(1, sizeof(sample_rx_t) * samples_adc_count);
	float *freq_response_x = malloc(freq_iterations * sizeof(float));
	float *freq_response_y = malloc(freq_iterations * sizeof(float));
	// Configure the SPI
	wave_freq_rad = 2.0 * M_PI * dac_freq_hz / freq_dac_effective_sps;
	// Configure the ADC
	plc_adc_set_rx_buffer_completed_callback(plc_adc, on_buffer_completed, NULL);
	// Generating TX wave and capturing process
	plc_afe_set_standy(plc_afe, 0);
	struct afe_settings afe_settings;
	memset(&afe_settings, 0, sizeof(afe_settings));
	afe_settings.cenelec_a = afe_cenelec_a;
	afe_settings.gain_tx_pga = afe_gain_tx_pga;
	afe_settings.gain_rx_pga1 = afe_gain_rx_pga1;
	afe_settings.gain_rx_pga2 = afe_gain_rx_pga2;
	plc_afe_configure(plc_afe, &afe_settings);
	switch (afe_calibration)
	{
	case afe_calibration_dac_txpga:
	case afe_calibration_dac_txpga_txfilter:
		// 'afe_calibration_dac_txpga' doesn't enable TX_F_OUT (accessible signal from outside)
		plc_afe_activate_blocks(plc_afe, afe_block_dac | afe_block_tx | afe_block_ref2);
		break;
	case afe_calibration_dac_txpga_rxpga1_rxfilter_rxpga2:
		// TODO: Review if 'afe_block_pa' and 'afe_block_pa_out' are required
		plc_afe_activate_blocks(plc_afe, afe_block_dac | afe_block_tx | afe_block_pa |
			afe_block_ref1 | afe_block_ref2 | afe_block_pa_out | afe_block_rx);
		break;
	case afe_calibration_none:
		plc_afe_activate_blocks(plc_afe, afe_block_dac | afe_block_tx | afe_block_pa |
			afe_block_ref1 | afe_block_ref2 | afe_block_pa_out | afe_block_rx);
		break;
	default:
		assert(0);
	}
	plc_afe_set_calibration_mode(plc_afe, afe_calibration);
	// Alternative way to change settings on-the-fly without 'plc_afe_configure'
	// plc_afe_set_gain_tx(plc_afe, afe_gain_tx_pga_025);
	// plc_afe_set_gains_rx(plc_afe, afe_gain_rx_pga1_025, afe_gain_rx_pga2_1);
	plc_afe_set_dac_mode(plc_afe, 1);
	printf("\nTRANSFER FUNCTION\n-----------------\n");
	int i;
	for (i = 0; i < freq_iterations; i++)
	{
		uint32_t samples_tx_minimizing_chaining_gap;
		if (dac_freq_hz != 0.0)
		{
			float wave_period_samples = freq_dac_effective_sps / dac_freq_hz;
			// 'calculate_best_number_of_cycles' is only required to chain preloaded data but kept
			// also in on-the-fly mode for simplicity
			samples_tx_minimizing_chaining_gap = calculate_best_number_of_cycles(
					wave_period_samples, samples_tx_preloaded_count);
		}
		else
		{
			samples_tx_minimizing_chaining_gap = samples_tx_preloaded_count;
		}
		samples_tx_preloaded_cur = samples_tx_preloaded;
		// Use 'samples_tx_preloaded_count' to use the whole buffer ignoring the chaining gap
		//	samples_tx_preloaded_end = samples_tx_preloaded + samples_tx_preloaded_count;
		samples_tx_preloaded_end = samples_tx_preloaded + samples_tx_minimizing_chaining_gap;
		if (tx_preloading)
			tx_fill_cycle_callback(NULL, samples_tx_preloaded, samples_tx_minimizing_chaining_gap);
		int err = plc_tx_start_transmission(plc_tx);
		assert(err == 0);
		// TODO: Revise why this delay improves the TX response. If removed an "APP WARNING" is
		//	sometimes triggered
		usleep(300000);
		capture_iteration = 0;
		samples_adc_cur = samples_adc;
		int ret = plc_adc_start_capture(plc_adc, RX_ADC_BUFFER_COUNT, 1, freq_adc_requested_sps);
		assert(ret == 0);
		// 'plc_adc_get_sampling_frequency' should be called after 'plc_adc_start_capture'
		freq_adc_effective_sps = plc_adc_get_sampling_frequency(plc_adc);
		samples_adc_filled = 0;
		printf("Running freq = %6.0f Hz...", dac_freq_hz);
		if (timed_tx)
		{
			uint32_t stamp = plc_time_get_tick_ms();
			while (plc_time_get_tick_ms() - stamp < 5000)
				usleep(100000);
		}
		else
		{
			uint32_t stamp = plc_time_get_tick_ms();
			while (!samples_adc_filled)
			{
				// Cancels if too much time waiting (mistake somewhere)
				if (plc_time_get_tick_ms() - stamp > 2000)
				{
					printf("Waiting aborted...\n");
					samples_adc_filled = 1;
				}
				else
				{
					usleep(100000);
				}
			}
		}
		plc_adc_stop_capture(plc_adc);
		// Stop the transmission to stop the filling in background and prepare a new buffer
		plc_tx_stop_transmission(plc_tx);
		float dc_mean, ac_mean;
		calculate_means(samples_adc, samples_adc_cur - samples_adc, &dc_mean, &ac_mean);
		printf("Samples captured. DC mean = %.2f, AC mean = %.2f\n", dc_mean, ac_mean);
		freq_response_x[i] = dac_freq_hz;
		if (adc_mean_for_transfer_one == 0.0)
			freq_response_y[i] = ac_mean / ac_mean_expected;
		else
			freq_response_y[i] = ac_mean / adc_mean_for_transfer_one;
		// If the frequency of interest is reached, hold buffer
		// For better performance just toggle buffers instead of coyping data
		if ((freq_iterations == 1)
				|| ((samples_adc_data_to_save_freq == 0.0) && (dac_freq_hz >= freq_ref)))
		{
			sample_rx_t *samples_adc_tmp = samples_adc_data_to_save;
			samples_adc_data_to_save = samples_adc;
			samples_adc_data_to_save_freq = dac_freq_hz;
			samples_adc = samples_adc_tmp;
			samples_adc_cur = samples_adc;
			samples_adc_end = samples_adc + samples_adc_count;
		}
		// Restart write buffer
		tx_counter = 0;
		dac_freq_hz += dac_freq_step;
		wave_freq_rad = 2.0 * M_PI * dac_freq_hz / freq_dac_effective_sps;
	}
	// Though not necessary, after ending TX reset the DAC value to 0 (instead of preserving the
	//	last value sent)
	plc_afe_transfer_dac_sample(plc_afe, 0);
	plc_afe_set_dac_mode(plc_afe, 0);
	plc_afe_disable_all(plc_afe);
	plc_afe_set_standy(plc_afe, 1);
	plc_adc_release(plc_adc);
	plc_tx_release(plc_tx);
	plc_cape_release(plc_cape);
	printf("\n");
	// Save file with freq_response_y
	if (freq_iterations > 1)
	{
		ret = plc_file_write_csv_xy(FILE_FREQ_RESPONSE, csv_float, freq_response_x, csv_float,
				freq_response_y, freq_iterations, 0);
		assert(ret >= 0);
		printf("Frequency response from %d to %d Hz (%d samples) stored in '%s'\n",
			(uint32_t) round(freq_ini), (uint32_t) round(freq_end), freq_iterations,
			FILE_FREQ_RESPONSE);
	}
	// Save file with samples
	ret = plc_file_write_csv(FILE_ADC_CAPTURE, csv_u16, samples_adc_data_to_save,
			samples_adc_count, 0);
	assert(ret >= 0);
	printf("ADC capture for frequency %.0f (%d samples) stored in '%s'\n",
			samples_adc_data_to_save_freq, samples_adc_count, FILE_ADC_CAPTURE);
	calculate_fft(samples_adc_data_to_save, samples_adc_count, samples_adc_data_to_save_freq);
	// Release objects
	free(freq_response_y);
	free(freq_response_x);
	free(samples_adc_data_to_save);
	free(samples_adc);
}

int main(int argc, char *argv[])
{
	plc_cape = plc_cape_create(PLC_DRIVERS);
	test_valid_pointer_errno(plc_cape, "AFE");
	set_test_defaults();
	cmdline_parse_args(argc, argv);
	execute_test();
	printf("\nEND with success\n");
	return EXIT_SUCCESS;
}

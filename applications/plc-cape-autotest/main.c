/**
 * @file
 * @brief	**Main** file
 *
 * @see		@ref application-plc-cape-autotest
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

#define _GNU_SOURCE			// Required for 'asprintf' declaration
#include <signal.h>			// signal
#include <unistd.h>			// sleep
#include "+common/api/+base.h"
#include "libraries/libplc-adc/api/adc.h"
#include "libraries/libplc-cape/api/afe.h"
#include "libraries/libplc-cape/api/leds.h"
#include "libraries/libplc-cape/api/cape.h"
#include "libraries/libplc-cape/api/tx.h"
#include "libraries/libplc-gpio/api/gpio.h"
#include "libraries/libplc-tools/api/application.h"
#include "libraries/libplc-tools/api/file.h"
#include "libraries/libplc-tools/api/terminal_io.h"
#include "libraries/libplc-tools/api/time.h"
#include "libraries/libplc-tools/api/trace.h"

#define PLC_DRIVERS 1

#define KEY_CODE_ESCAPE 27

#define DAC_RANGE 1024
#define DAC_WAVE_MIN_VALUE 700
#define DAC_WAVE_MAX_VALUE 900
#define DAC_WAVE_MEAN_VALUE ((DAC_WAVE_MAX_VALUE+DAC_WAVE_MIN_VALUE)/2)

#define ADC_CAPTURE_FILENAME "adc.csv"

#define TX_BUFFER_LEN 1024
#define RX_BUFFER_LEN 1024

static struct plc_terminal_io *plc_terminal_io;
static struct plc_cape *plc_cape = NULL;
// Use 'plc_afe' as a shortcut to access AFE functionality
static struct plc_afe *plc_afe = NULL;

// We can rely on the default tracing support offered by 'lipplc-tools'
int plc_debug_level = 3;
void (*plc_trace)(const char *function_name, const char *format, ...) = plc_trace_default;

int set_error_msg(const char *msg)
{
	return -1;
}

void test_leds(void)
{
	struct plc_leds *leds = plc_cape_get_leds(plc_cape);
	plc_leds_set_app_activity(leds, 0);
	plc_leds_set_tx_activity(leds, 0);
	plc_leds_set_rx_activity(leds, 0);
	puts("LEDs OFF");
	sleep(1);
	puts("LED activity ON");
	plc_leds_set_app_activity(leds, 1);
	sleep(1);
	puts("LED tx ON");
	plc_leds_set_tx_activity(leds, 1);
	sleep(1);
	puts("LED rx ON");
	plc_leds_set_rx_activity(leds, 1);
	sleep(1);
	puts("LEDs OFF");
	plc_leds_set_app_activity(leds, 0);
	plc_leds_set_tx_activity(leds, 0);
	plc_leds_set_rx_activity(leds, 0);
}

void test_afe_info(void)
{
	puts("Getting AFE information...");
	char *afe_info = plc_afe_get_info(plc_afe);
	// Use 'fputs' instead of 'puts' to avoid the extra new-line added by 'puts'
	fputs(afe_info, stdout);
	free(afe_info);
}

void test_adc(void)
{
	puts("ADC real-time value displayed\nPress a key to stop the loop...");
	struct plc_adc *plc_adc = plc_adc_create(PLC_DRIVERS);
	while (!plc_terminal_io_kbhit(plc_terminal_io))
	{
		sample_rx_t adc_value = plc_adc_read_sample(plc_adc);
		// Move cursor one line up after 
		printf("ADC value = %5d\n\033[1A", adc_value);
		usleep(100000);
	}
	plc_adc_release(plc_adc);
	puts("");
}

int test_adc_time(struct plc_adc *plc_adc, int milliseconds)
{
	uint32_t stamp_ini_ms = plc_time_get_tick_ms();
	while (plc_time_get_tick_ms() - stamp_ini_ms < milliseconds)
	{
		sample_rx_t adc_value = plc_adc_read_sample(plc_adc);
		// Move cursor one line up after 
		printf("ADC value = %5d\n\033[1A", adc_value);
		usleep(100000);
		if (plc_terminal_io_kbhit(plc_terminal_io))
			return -1;
	}
	return 0;
}

void test_afe_dac_adc_by_steps(void)
{
	puts("Testing DAC + ADC with increasing step values with internal AFE routing\n"
			"Press a key to cancel...");
	struct plc_adc *plc_adc = plc_adc_create(PLC_DRIVERS);
	plc_afe_activate_blocks(plc_afe, afe_block_dac | afe_block_tx | afe_block_ref2);
	plc_afe_set_calibration_mode(plc_afe, afe_calibration_dac_txpga);
	plc_afe_set_dac_mode(plc_afe, 1);
	int dac_value = 0;
	int abort_test = 0;
	for (; (dac_value < DAC_RANGE) && !abort_test; dac_value += 250)
	{
		printf("DAC value = %d\n", dac_value);
		plc_afe_transfer_dac_sample(plc_afe, dac_value);
		abort_test = (test_adc_time(plc_adc, 3000) == -1);
		puts("");
	}
	plc_afe_set_dac_mode(plc_afe, 0);
	plc_adc_release(plc_adc);
}

void prepare_next_samples_f05_callback(void *handle, uint16_t *buffer, uint32_t buffer_count)
{
	for (; buffer_count > 0; buffer_count--, buffer++)
		*buffer = (buffer_count & 1) ? DAC_WAVE_MIN_VALUE : DAC_WAVE_MAX_VALUE;
}

void prepare_next_samples_f025_callback(void *handle, sample_tx_t *buffer, uint32_t buffer_count)
{
	const sample_tx_t pattern[] =
	{ DAC_WAVE_MEAN_VALUE, DAC_WAVE_MAX_VALUE, DAC_WAVE_MEAN_VALUE, DAC_WAVE_MIN_VALUE };
	int pattern_index = 0;
	for (; buffer_count > 0; buffer_count--, buffer++)
	{
		*buffer = pattern[pattern_index];
		if (++pattern_index == ARRAY_SIZE(pattern))
			pattern_index = 0;
	}
}

void tx_on_buffer_sent_callback(void *handle, sample_tx_t *buffer, uint32_t buffer_count)
{
	putc('.', stdout);
	// NOTE: 'fflush' required to have live progress information
	//	Otherwise the stream is cached until '\n'
	fflush(stdout);
}

void test_afe_dac(void (*prepare_next_samples_callback) (void *, sample_tx_t *, uint32_t))
{
	static const int INTERVAL_MS = 5000;
	puts("Generating TX output\nPress a key to cancel...");
	static const uint32_t tx_freq_bps = 750000;
	static const uint32_t tx_samples_delay_us = 0;
	plc_afe_configure_spi(plc_afe, tx_freq_bps, tx_samples_delay_us);
	plc_afe_set_standy(plc_afe, 0);
	plc_afe_activate_blocks(plc_afe, afe_block_dac | afe_block_tx | afe_block_ref2);
	plc_afe_set_calibration_mode(plc_afe, afe_calibration_none);
	plc_afe_set_dac_mode(plc_afe, 1);
	struct plc_tx *plc_tx = plc_tx_create(prepare_next_samples_callback, NULL,
			tx_on_buffer_sent_callback, NULL, spi_tx_mode_ping_pong_dma, 1024, plc_afe);
	if (plc_tx == NULL)
		return;
	plc_tx_start_transmission(plc_tx);
	uint32_t stamp_ini_ms = plc_time_get_tick_ms();
	while ((plc_time_get_tick_ms() - stamp_ini_ms < INTERVAL_MS)
			&& !plc_terminal_io_kbhit(plc_terminal_io))
	{
		usleep(100000);
	}
	plc_tx_stop_transmission(plc_tx);
	plc_tx_release(plc_tx);
	plc_afe_set_dac_mode(plc_afe, 0);
	plc_afe_disable_all(plc_afe);
	plc_afe_set_standy(plc_afe, 1);
	puts("");
}

struct rx_callback_data
{
	sample_rx_t *buffer_for_data;
	volatile int adc_data_rx_completed;
};

void rx_buffer_completed_callback(
		void *data, sample_rx_t *samples_buffer, uint32_t samples_buffer_count)
{
	putc('+', stdout);
	// NOTE: 'fflush' required to have live progress information
	//	Otherwise the stream is cached until '\n'
	fflush(stdout);

	struct rx_callback_data *rx_callback_data = data;
	memcpy(rx_callback_data->buffer_for_data, samples_buffer, 
			samples_buffer_count*sizeof(sample_rx_t));

	rx_callback_data->adc_data_rx_completed = 1;
}

void test_afe_dac_adc_by_file(
		void (*prepare_next_samples_callback) (void *, sample_tx_t *, uint32_t))
{
	puts("Testing DAC + ADC loop to file");
	static const uint32_t tx_freq_bps = 750000;
	static const uint32_t tx_samples_delay_us = 0;
	plc_afe_configure_spi(plc_afe, tx_freq_bps, tx_samples_delay_us);
	plc_afe_set_standy(plc_afe, 0);
	plc_afe_activate_blocks(plc_afe, afe_block_dac | afe_block_tx | afe_block_ref2);
	plc_afe_set_calibration_mode(plc_afe, afe_calibration_dac_txpga);
	plc_afe_set_dac_mode(plc_afe, 1);
	// Configure TX
	struct plc_tx *plc_tx = plc_tx_create(prepare_next_samples_callback, NULL,
			tx_on_buffer_sent_callback, NULL, spi_tx_mode_ping_pong_dma, TX_BUFFER_LEN, plc_afe);
	if (plc_tx == NULL)
		return;
	// Configure ADC
	struct plc_adc *plc_adc = plc_adc_create(PLC_DRIVERS);
	struct rx_callback_data rx_callback_data;
	rx_callback_data.buffer_for_data = malloc(RX_BUFFER_LEN*sizeof(sample_rx_t));
	rx_callback_data.adc_data_rx_completed = 0;
	plc_adc_set_rx_buffer_completed_callback(plc_adc, rx_buffer_completed_callback,
			&rx_callback_data);
	// Start TX-RX process
	plc_tx_start_transmission(plc_tx);
	plc_adc_start_capture(plc_adc, RX_BUFFER_LEN, 1);
	while (!rx_callback_data.adc_data_rx_completed && !plc_terminal_io_kbhit(plc_terminal_io))
		usleep(100000);
	// Stop TX-RX process
	plc_adc_stop_capture(plc_adc);
	sleep(1);
	plc_tx_stop_transmission(plc_tx);
	// Advanced to one fresh line
	puts("");
	// Release objects
	plc_adc_release(plc_adc);
	plc_tx_release(plc_tx);
	plc_afe_set_dac_mode(plc_afe, 0);
	plc_afe_disable_all(plc_afe);
	plc_afe_set_standy(plc_afe, 1);
	// Store data to file
	char *output_dir = plc_application_get_output_abs_dir();
	char *output_path;
	asprintf(&output_path, "%s/%s", output_dir, ADC_CAPTURE_FILENAME);
	plc_file_write_csv(output_path, csv_u16, rx_callback_data.buffer_for_data, RX_BUFFER_LEN, 0);
	printf("%d samples stored in '%s'\n", RX_BUFFER_LEN, output_path);
	free(output_path);
	free(output_dir);
	free(rx_callback_data.buffer_for_data);
}

int main(void)
{
	// Initialize improved terminal input output
	plc_terminal_io = plc_terminal_io_create();
	// Initiate the PlcCape
	plc_cape = plc_cape_create(PLC_DRIVERS);
	plc_afe = plc_cape_get_afe(plc_cape);
	// Menu loop
	while (1)
	{
		puts(
				"=================\n"
				"== SELECT TEST ==\n"
				"=================\n"
				"1. AFE LEDs\n"
				"2. AFE Info\n"
				"3. BBB ADC\n"
				"4. AFE DAC steps + BBB ADC [AFE in TX_RX_LOOP]\n"
				"5. AFE DAC freq=0.5 [AFE in TX]\n"
				"6. AFE DAC freq=0.25 [AFE in TX]\n"
				"7. AFE DAC freq=0.25 + BBB ADC + File-capturing [AFE in TX_RX_LOOP]\n"
				"[ESC] Quit");

		int key = plc_terminal_io_getchar(plc_terminal_io);
		puts("");
		if (key == KEY_CODE_ESCAPE)
			break;
		switch (key)
		{
		case '1':
			test_leds();
			break;
		case '2':
			test_afe_info();
			break;
		case '3':
			test_adc();
			break;
		case '4':
			test_afe_dac_adc_by_steps();
			break;
		case '5':
			test_afe_dac(prepare_next_samples_f05_callback);
			break;
		case '6':
			test_afe_dac(prepare_next_samples_f025_callback);
			break;
		case '7':
			test_afe_dac_adc_by_file(prepare_next_samples_f025_callback);
			break;
		default:
			puts("Unknown option");
		}
	}
	plc_cape_release(plc_cape);
	// Restore the original configuration of the terminal
	plc_terminal_io_release(plc_terminal_io);
	return EXIT_SUCCESS;
}

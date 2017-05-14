/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#define _GNU_SOURCE		// Required for 'vasprintf' declaration
#include <pthread.h>
#include <unistd.h>
#include "+common/api/+base.h"
#include "common.h"
#include "monitor.h"
#include "libraries/libplc-adc/api/adc.h"
#include "libraries/libplc-adc/api/analysis.h"
#include "libraries/libplc-cape/api/tx.h"
#include "libraries/libplc-tools/api/time.h"
#include "ui.h"

#define UI_REFRESH_INTERVAL_MS 250
#define UI_REFRESH_INTERVAL_US (UI_REFRESH_INTERVAL_MS*1000)

struct monitor
{
	struct plc_adc *plc_adc;
	struct plc_rx_analysis *plc_rx_analysis;
	struct plc_tx *plc_tx;
	struct ui *ui;
	pthread_t thread;
	int end_thread;
	enum monitor_profile_enum monitor_profile;
	uint32_t buffers_tx_count;
	uint32_t buffers_rx_count;
	sample_rx_t buffer_rx_last_value;
};

struct monitor *monitor_create(struct ui *ui)
{
	struct monitor *monitor = (struct monitor*) calloc(1, sizeof(struct monitor));
	monitor->ui = ui;
	return monitor;
}

void monitor_release(struct monitor *monitor)
{
	free(monitor);
}

void monitor_set_adc(struct monitor *monitor, struct plc_adc *plc_adc)
{
	monitor->plc_adc = plc_adc;
}

void monitor_set_tx(struct monitor *monitor, struct plc_tx *plc_tx)
{
	monitor->plc_tx = plc_tx;
}

void monitor_set_rx_analysis(struct monitor *monitor, struct plc_rx_analysis *plc_rx_analysis)
{
	monitor->plc_rx_analysis = plc_rx_analysis;
}

static void *monitor_thread(void *arg)
{
	struct monitor *monitor = arg;
	uint32_t ui_tick_last_refresh = 0;
	while (!monitor->end_thread)
	{
		usleep(UI_REFRESH_INTERVAL_US);
		// 'sleep' can be aborted by a signal -> contention test to avoid refreshment overflow
		if (plc_time_get_tick_ms() - ui_tick_last_refresh >= UI_REFRESH_INTERVAL_MS)
		{
			ui_tick_last_refresh = plc_time_get_tick_ms();
			// Set signals information
			char *text = NULL;
			const struct rx_statistics *rx_stat;
			const struct tx_statistics *tx_stat;
			switch (monitor->monitor_profile)
			{
			case monitor_profile_none:
				break;
			case monitor_profile_buffers_processed:
				asprintf(&text, "Buffers processed: TX: %u, RX: %u, Last RX value: %u",
						monitor->buffers_tx_count,
						monitor->buffers_rx_count,
						monitor->buffer_rx_last_value);
				break;
			case monitor_profile_tx_values:
				if ((monitor->plc_tx) && (tx_stat = plc_tx_get_tx_statistics(monitor->plc_tx)))
					asprintf(&text, "TX: Buffers overflow/handled: %u/%u",
							tx_stat->buffers_overflow, tx_stat->buffers_handled);
				break;
			case monitor_profile_tx_time:
				if ((monitor->plc_tx) && (tx_stat = plc_tx_get_tx_statistics(monitor->plc_tx)))
					asprintf(&text,
							"TX timings [us] (min, cur, max): "
							"Processing (%u,%u,%u), Cycle: (%u,%u,%u)",
							tx_stat->buffer_preparation_min_us, tx_stat->buffer_preparation_us,
							tx_stat->buffer_preparation_max_us, tx_stat->buffer_cycle_min_us,
							tx_stat->buffer_cycle_us, tx_stat->buffer_cycle_max_us);
				break;
			case monitor_profile_rx_values:
				if ((monitor->plc_rx_analysis) && 
					(rx_stat = plc_rx_analysis_get_statistics(monitor->plc_rx_analysis)))
					asprintf(&text,
						"RX: Buffers received: %u, "
						"DC Mean: %.1f, AC Mean: %.1f, Range: %u (%u..%u)",
						rx_stat->buffers_handled, rx_stat->buffer_dc_mean, rx_stat->buffer_ac_mean,
						rx_stat->buffer_max - rx_stat->buffer_min, rx_stat->buffer_min,
						rx_stat->buffer_max);
				break;
			case monitor_profile_rx_time:
				if ((monitor->plc_rx_analysis) &&
					(rx_stat = plc_rx_analysis_get_statistics(monitor->plc_rx_analysis)))
					asprintf(&text,
						"RX timings [us] (min, cur, max): Processing (%u,%u,%u), Cycle: (%u,%u,%u)",
						rx_stat->buffer_preparation_min_us, rx_stat->buffer_preparation_us,
						rx_stat->buffer_preparation_max_us, rx_stat->buffer_cycle_min_us,
						rx_stat->buffer_cycle_us, rx_stat->buffer_cycle_max_us);
				break;
			default:
				assert(0);
				break;
			}
			if (text)
				ui_set_status_bar(monitor->ui, text);
		}
	}
	return NULL;
}

void monitor_set_profile(struct monitor *monitor, enum monitor_profile_enum profile)
{
	monitor->monitor_profile = profile;
	switch(profile)
	{
	case monitor_profile_rx_values:
		if (monitor->plc_rx_analysis)
			plc_rx_analysis_set_statistics_mode(monitor->plc_rx_analysis, plc_rx_statistics_values);
		break;
	case monitor_profile_rx_time:
		if (monitor->plc_rx_analysis)
			plc_rx_analysis_set_statistics_mode(monitor->plc_rx_analysis, plc_rx_statistics_time);
		break;
	default:
		if (monitor->plc_rx_analysis)
			plc_rx_analysis_set_statistics_mode(monitor->plc_rx_analysis, plc_rx_statistics_none);
		break;
	}
	// Clear the status bar
	ui_set_status_bar(monitor->ui, "");
}

void monitor_start(struct monitor *monitor)
{
	monitor->end_thread = 0;
	int ret = pthread_create(&monitor->thread, NULL, monitor_thread, monitor);
	assert(ret == 0);
}

void monitor_stop(struct monitor *monitor)
{
	monitor->end_thread = 1;
	int ret = pthread_join(monitor->thread, NULL);
	assert(ret == 0);
}

void monitor_on_buffer_sent(struct monitor *monitor, uint16_t *samples_buffer,
		uint32_t samples_buffer_count)
{
	monitor->buffers_tx_count++;
}

void monitor_on_buffer_received(struct monitor *monitor, sample_rx_t *samples_buffer,
		uint32_t samples_buffer_count)
{
	monitor->buffers_rx_count++;
	monitor->buffer_rx_last_value = *samples_buffer;
}

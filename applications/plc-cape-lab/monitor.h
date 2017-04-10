/**
 * @file
 * @brief	Different types of monitoring
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef MONITOR_H
#define MONITOR_H

enum monitor_profile_enum
{
	monitor_profile_none = 0,
	monitor_profile_buffers_processed,
	monitor_profile_tx_values,
	monitor_profile_tx_time,
	monitor_profile_rx_values,
	monitor_profile_rx_time,
};

struct monitor;
struct plc_adc;
struct plc_rx_analysis;
struct plc_tx;
struct ui;

struct monitor *monitor_create(struct ui *ui);
void monitor_release(struct monitor *monitor);
void monitor_set_adc(struct monitor *monitor, struct plc_adc *plc_adc);
void monitor_set_tx(struct monitor *monitor, struct plc_tx *plc_tx);
void monitor_set_rx_analysis(struct monitor *monitor, struct plc_rx_analysis *plc_rx_analysis);
void monitor_set_profile(struct monitor *monitor, enum monitor_profile_enum profile);
void monitor_start(struct monitor *monitor);
void monitor_stop(struct monitor *monitor);
void monitor_on_buffer_sent(struct monitor *monitor, uint16_t *samples_buffer,
		uint32_t samples_buffer_count);
void monitor_on_buffer_received(struct monitor *monitor, uint16_t *samples_buffer,
		uint32_t samples_buffer_count);

#endif /* MONITOR_H */

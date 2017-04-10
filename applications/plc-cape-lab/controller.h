/**
 * @file
 * @brief	Core engine to control the main application functionalities
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "monitor.h"	// enum monitor_profile_enum

extern const char *configuration_profile_enum_text[];
enum configuration_profile_enum
{
	cfg_none = 0,
	cfg_txrx_cal_sin_2kHz_defaults,		// TX_PGA_CAL + RX, TX_PGA_OUT approx = 0.2*sin(2Khz)
	cfg_txrx_cal_sin_2kHz,
	cfg_txrx_cal_ramp,
	cfg_txrx_cal_wave_ook_10kHz,
	cfg_txrx_cal_wave_ook_Hi_10kHz,
	cfg_txrx_cal_msg_ook_10kHz_preload,
	cfg_txrx_cal_msg_pwm_10kHz_preload,
	cfg_txrx_cal_filter_msg_pwm_110kHz_preload,
	cfg_txrx_cal_filter_msg_pwm_110kHz_preload_timed,
	cfg_tx_sin_2kHz,
	cfg_tx_sin_100kHz_3Mbps,
	cfg_tx_file_93750bps,
	cfg_tx_freq_sweep,
	cfg_tx_pa_sin_10kHz,
	cfg_tx_pa_sin_110kHz_6Mbps_preload,
	cfg_tx_pa_sin_quarter,
	cfg_tx_pa_freq_sweep,
	cfg_tx_pa_freq_sweep_preload,
	cfg_tx_pa_sin_ook_20kHz,
	cfg_tx_pa_msg_pwm_110kHz_preload,
	cfg_tx_pa_constant,
	cfg_tx_pa_rx_sin_2kHz,				// TX + PA + RX, ADC, TX_TRANSFO approx = 1*sin(2Khz)
	cfg_tx_pa_rx_sin_ook_9kHz,
	cfg_rx,
	cfg_rx_msg_pwm_110kHz,
	cfg_COUNT
};

struct settings;
struct ui;

// 'controller' is a singleton object
void controller_create(struct settings *settings_arg, struct ui *ui_arg);
void controller_release(void);
char *controller_get_settings_text(void);
void controller_activate_async(void);
void controller_deactivate(void);
int controlles_is_active(void);
void controller_update_configuration(void);
void controller_set_configuration_profile(enum configuration_profile_enum configuration_profile);
void controller_set_monitoring_profile(enum monitor_profile_enum profile);
void controller_clear_overloads(void);
void controller_log_overloads(void);
void controller_view_current_settings(void);
void controller_measure_encoder_time(void);
struct plc_plugin_list *controller_get_encoder_plugins(void);
struct plc_plugin_list *controller_get_decoder_plugins(void);

#endif /* LED_H */

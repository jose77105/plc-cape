/**
 * @file
 * @brief	Configuration of the PlcCape board
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include "libraries/libplc-cape/api/afe.h"		// gain_XXX_enum
#include "libraries/libplc-cape/api/tx.h"		// spi_tx_mode_enum;
#include "libraries/libplc-tools/api/settings.h"	// plc_setting_named_list
#include "monitor.h"							// monitor_profile_enum
#include "rx.h"									// rx_mode_enum

struct encoder;
struct decoder;

extern const char *operating_mode_enum_text[];
enum operating_mode_enum
{
	operating_mode_none = 0,
	operating_mode_tx_dac,
	operating_mode_tx_dac_txpga_txfilter,
	operating_mode_tx_dac_txpga_txfilter_pa,
	operating_mode_tx_dac_txpga_txfilter_pa_rx,
	operating_mode_calib_dac_txpga,
	operating_mode_calib_dac_txpga_txfilter,
	operating_mode_calib_dac_txpga_rxpga1_rxfilter_rxpag2,
	operating_mode_rx,
	operating_mode_COUNT
};

struct settings_tx
{
	enum plc_tx_device_enum device;
	enum afe_calibration_enum afe_calibration_mode;
	float sampling_rate_sps;
	// TODO: Remove 'samples_delay_us'?
	uint16_t samples_delay_us;
	uint32_t preload_buffer_len;
	uint32_t tx_buffers_len;
	enum spi_tx_mode_enum tx_mode;
	enum afe_gain_tx_pga_enum gain_tx_pga;
};

struct settings_rx
{
	enum rx_mode_enum rx_mode;
	enum plc_rx_device_enum device;
	float sampling_rate_sps;
	char *samples_filename;
	char *data_filename;
	uint32_t samples_to_file;
	enum demod_mode_enum demod_mode;
	sample_rx_t data_offset;
	sample_rx_t data_hi_threshold_detection;
	enum afe_gain_rx_pga1_enum gain_rx_pga1;
	enum afe_gain_rx_pga2_enum gain_rx_pga2;
};

struct settings
{
	int std_driver;
	enum operating_mode_enum operating_mode;
	uint32_t cenelec_a;
	uint32_t bit_width_us;
	uint32_t autostart;
	uint32_t quietmode;
	uint32_t communication_timeout_ms;
	uint32_t communication_interval_ms;
	char *configuration_profile;
	enum monitor_profile_enum monitor_profile;
	struct settings_tx tx;
	struct settings_rx rx;
};

struct settings *settings_create(void);
void settings_release(struct settings *settings);
void settings_set_defaults(struct settings *settings);
char *settings_get_info(struct settings *settings, struct encoder *encoder, struct decoder *decoder);
int settings_set_app_settings(struct settings *settings, uint32_t settings_count,
		const struct plc_setting settings_src[]);

#endif /* SETTINGS_H */

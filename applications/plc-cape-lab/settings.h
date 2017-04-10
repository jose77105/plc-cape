/**
 * @file
 * @brief	Configuration of the PlcCape board
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include "controller.h"							// configuration_profile_enum
#include "rx.h"									// rx_mode_enum
#include "libraries/libplc-cape/api/afe.h"		// gain_XXX_enum
#include "libraries/libplc-cape/api/tx.h"		// spi_tx_mode_enum;
#include "plugins/encoder/api/encoder.h"			// stream_type_enum

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
	char *encoder_plugin;
	uint32_t freq_bps;
	uint16_t samples_delay_us;
	enum stream_type_enum stream_type;
	uint32_t preload_buffer_len;
	uint32_t tx_buffers_len;
	int samples_offset;
	int samples_range;
	int samples_freq;
	char *dac_filename;
	char *message_to_send;
	enum spi_tx_mode_enum tx_mode;
	enum afe_gain_tx_pga_enum gain_tx_pga;
};

struct settings_rx
{
	enum rx_mode_enum rx_mode;
	uint32_t samples_to_file;
	enum demodulation_mode_enum demod_mode;
	uint16_t demod_data_hi_threshold;
	sample_rx_t data_offset;
	sample_rx_t data_hi_threshold_detection;
	enum afe_gain_rx_pga1_enum gain_rx_pga1;
	enum afe_gain_rx_pga2_enum gain_rx_pga2;
};

struct settings
{
	char *ui_plugin_name;
	int std_driver;
	enum operating_mode_enum operating_mode;
	int cenelec_a;
	uint32_t data_bit_us;
	int autostart;
	uint32_t communication_timeout_ms;
	uint32_t communication_interval_ms;
	enum configuration_profile_enum configuration_profile;
	enum monitor_profile_enum monitor_profile;
	struct settings_tx tx;
	struct settings_rx rx;
};

const char *settings_get_usage_message(void);
struct settings *settings_create(void);
void settings_release(struct settings *settings);
struct settings *settings_clone(const struct settings *settings);
void settings_copy(struct settings *settings, const struct settings *settings_src);
char *settings_get_info(struct settings *settings);
void settings_parse_args(struct settings *settings, int argc, char *argv[], char **error_msg);
void settings_update(struct settings *settings);

#endif /* SETTINGS_H */

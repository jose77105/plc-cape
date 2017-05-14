/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#define _GNU_SOURCE				// Required for 'asprintf' declaration
#include "+common/api/+base.h"
#include "+common/api/bbb.h"	// ADC_MAX_CAPTURE_RATE_SPS
#include "common.h"
#include "encoder.h"
#include "decoder.h"
#include "libraries/libplc-cape/api/afe.h"
#include "settings.h"

#define DATA_BIT_US_DEFAULT 1000
#define SAMPLING_FREQ_SPS_DEFAULT 100000.0
#define SPI_DAC_DELAY_US_DEFAULT 0
#define TX_BUFFERS_LEN_DEFAULT 1024
#define RX_SAMPLES_FILENAME "adc.csv"
#define RX_DATA_FILENAME "adc_data.csv"

const char *operating_mode_enum_text[operating_mode_COUNT] = {
	"none", "tx_dac", "tx_dac_txpga_txfilter", "tx_dac_txpga_txfilter_pa",
	"tx_dac_txpga_txfilter_pa_rx", "calib_dac_txpga", "calib_dac_txpga_txfilter",
	"calib_dac_txpga_rxpga1_rxfilter_rxpag2", "rx", };

static void settings_release_resources(struct settings *settings)
{
	if (settings->configuration_profile)
	{
		free(settings->configuration_profile);
		settings->configuration_profile = NULL;
	}
	if (settings->rx.samples_filename)
	{
		free(settings->rx.samples_filename);
		settings->rx.samples_filename = NULL;
	}
	if (settings->rx.data_filename)
	{
		free(settings->rx.data_filename);
		settings->rx.data_filename = NULL;
	}
}

void settings_set_defaults(struct settings *settings)
{
	settings_release_resources(settings);
	memset(settings, 0, sizeof(*settings));
	settings->operating_mode = operating_mode_none;
	settings->bit_width_us = DATA_BIT_US_DEFAULT;
	settings->tx.sampling_rate_sps = SAMPLING_FREQ_SPS_DEFAULT;
	settings->tx.samples_delay_us = SPI_DAC_DELAY_US_DEFAULT;
	settings->tx.tx_buffers_len = TX_BUFFERS_LEN_DEFAULT;
	settings->tx.tx_mode = spi_tx_mode_none;
	settings->rx.rx_mode = rx_mode_none;
	settings->rx.sampling_rate_sps = ADC_MAX_CAPTURE_RATE_SPS;
	settings->rx.samples_filename = strdup(RX_SAMPLES_FILENAME);
	settings->rx.data_filename = strdup(RX_DATA_FILENAME);
	settings->monitor_profile = monitor_profile_buffers_processed;
}

struct settings *settings_create(void)
{
	struct settings *settings = (struct settings*) calloc(1, sizeof(struct settings));
	settings_set_defaults(settings);
	return settings;
}

void settings_release(struct settings *settings)
{
	settings_release_resources(settings);
	free(settings);
}

static char *get_settings_list_info(const struct setting_linked_list_item *settings)
{
	char *info = strdup("");
	const struct setting_linked_list_item *list_item = settings;
	while (list_item != NULL)
	{
		char *setting_value_text = plc_setting_linked_data_to_text(&list_item->setting);
		// TODO: Use the setting description instead of the identifier, when possible
		char *info_new;
		asprintf(&info_new, "%s %s:%s\n", info, list_item->setting.definition->identifier,
				setting_value_text);
		free(info);
		info = info_new;
		list_item = list_item->next;
	}
	return info;
}

char *settings_get_info(struct settings *settings, struct encoder *encoder, struct decoder *decoder)
{
	char *app_info = NULL;
	if ((settings->communication_timeout_ms != 0) || (settings->communication_interval_ms != 0)
			|| settings->std_driver)
	{
		asprintf(&app_info, "APP\n"
				"  Comm timeout: %u ms\n"
				"  Comm interval: %u ms\n"
				"  %s driver\n", settings->communication_timeout_ms,
				settings->communication_interval_ms, settings->std_driver ? "STD" : "CUSTOM");
	}
	char *afe_info;
	asprintf(&afe_info, "AFE\n"
			" %u:%s\n"
			" Gains tx:%u rx1:%u rx2:%u\n"
			" CENELEC %s\n", settings->operating_mode,
			operating_mode_enum_text[settings->operating_mode], settings->tx.gain_tx_pga,
			settings->rx.gain_rx_pga1, settings->rx.gain_rx_pga2,
			settings->cenelec_a ? "A" : "B/C/D");
	char *tx_info = NULL;
	if (settings->tx.tx_mode != spi_tx_mode_none)
	{
		char *encoder_info = NULL;
		if (encoder)
		{
			char *encoder_settings = get_settings_list_info(encoder_get_settings(encoder));
			asprintf(&encoder_info, "%s\n%s", encoder_get_name(encoder), encoder_settings);
			free(encoder_settings);
		}
		asprintf(&tx_info, "TX\n"
				" %u:%s\n"
				" Sampling at %u ksps\n"
				" Pregen: %u samples\n"
				" TX buffer: %u\n"
				"%s", settings->tx.tx_mode, spi_tx_mode_enum_text[settings->tx.tx_mode],
				(uint32_t) (settings->tx.sampling_rate_sps / 1000.0),
				settings->tx.preload_buffer_len, settings->tx.tx_buffers_len,
				encoder_info ? encoder_info : "");
		if (encoder_info)
			free(encoder_info);
	}
	char *rx_info = NULL;
	if (settings->rx.rx_mode != rx_mode_none)
	{
		char *decoder_info = NULL;
		if (decoder)
		{
			char *decoder_settings = get_settings_list_info(decoder_get_settings(decoder));
			asprintf(&decoder_info, "%s\n%s", decoder_get_name(decoder), decoder_settings);
			free(decoder_settings);
		}
		asprintf(&rx_info, "RX\n"
				" %u:%s\n"
				" %u:%s\n"
				" ToFile: %u samples\n"
				"%s", settings->rx.rx_mode, rx_mode_enum_text[settings->rx.rx_mode],
				settings->rx.demod_mode, demod_mode_enum_text[settings->rx.demod_mode],
				settings->rx.samples_to_file, decoder_info ? decoder_info : "");
		if (decoder_info)
			free(decoder_info);
	}
	char *info;
	asprintf(&info, "PROFILE: %s\n%s%s%s%s",
			settings->configuration_profile ? settings->configuration_profile : "",
			app_info ? app_info : "", afe_info ? afe_info : "", tx_info ? tx_info : "",
			rx_info ? rx_info : "");
	if (rx_info)
		free(rx_info);
	if (tx_info)
		free(tx_info);
	if (afe_info)
		free(afe_info);
	if (app_info)
		free(app_info);
	return info;
}

int settings_set_value(void *dst_data, enum plc_setting_type dst_type,
		const struct plc_setting *setting_src)
{
	// TODO: Improve for better efficiency instead of going back and forth through a string
	char *text = plc_setting_data_to_text(setting_src->type, &setting_src->data);
	plc_setting_text_to_data(text, dst_type, (union plc_setting_data *) dst_data);
	free(text);
	return 0;
}

// TODO: Revise why the use of 'const' (recommended) produces several warnings
//	typedef const struct plc_setting_extra_data plc_setting_extra_data_t;
typedef struct plc_setting_extra_data plc_setting_extra_data_t;

#define DECLARE_ENUM_CAPTIONS(enum_name) \
		static plc_setting_extra_data_t enum_name ## _captions = { \
			plc_setting_extra_data_enum_captions, { \
				.enum_captions.captions = enum_name ## _enum_text, .enum_captions.captions_count = \
				enum_name ## _COUNT } };

DECLARE_ENUM_CAPTIONS(spi_tx_mode)
DECLARE_ENUM_CAPTIONS(afe_gain_tx_pga)
DECLARE_ENUM_CAPTIONS(rx_mode)
DECLARE_ENUM_CAPTIONS(demod_mode)
DECLARE_ENUM_CAPTIONS(afe_gain_rx_pga1)
DECLARE_ENUM_CAPTIONS(afe_gain_rx_pga2)
DECLARE_ENUM_CAPTIONS(operating_mode)

#define OFFSET(member) offsetof(struct settings, member)

static const struct plc_setting_definition app_accepted_settings[] = {
	{
		"tx_sampling_rate_sps", plc_setting_float, "TX DAC rate [sps]", {
			.f = SAMPLING_FREQ_SPS_DEFAULT }, 0, NULL, OFFSET(tx.sampling_rate_sps) }, {
		"preload_buffer_len", plc_setting_u32, "Preload buffer len", {
			.u32 = 0 }, 0, NULL, OFFSET(tx.preload_buffer_len) }, {
		"tx_buffers_len", plc_setting_u32, "TX buffers len", {
			.u32 = 0 }, 0, NULL, OFFSET(tx.tx_buffers_len) }, {
		"tx_mode", plc_setting_enum, "TX mode", {
			.u32 = spi_tx_mode_none }, 1, &spi_tx_mode_captions, OFFSET(tx.tx_mode) }, {
		"gain_tx_pga", plc_setting_enum, "Gain TX PGA", {
			.u32 = afe_gain_tx_pga_025 }, 1, &afe_gain_tx_pga_captions, OFFSET(tx.gain_tx_pga) }, {
		"rx_sampling_rate_sps", plc_setting_float, "RX ADC rate [sps]", {
			.f = ADC_MAX_CAPTURE_RATE_SPS }, 0, NULL, OFFSET(rx.sampling_rate_sps) }, {
		"rx_mode", plc_setting_enum, "RX mode", {
			.u32 = rx_mode_none }, 1, &rx_mode_captions, OFFSET(rx.rx_mode) }, {
		"rx_samples_filename", plc_setting_string, "RX samples filename", {
			.s = RX_SAMPLES_FILENAME }, 0, NULL, OFFSET(rx.samples_filename) }, {
		"rx_data_filename", plc_setting_string, "RX data filename", {
			.s = RX_DATA_FILENAME }, 0, NULL, OFFSET(rx.data_filename) }, {
		"samples_to_file", plc_setting_u32, "Samples to file", {
			.u32 = 0 }, 0, NULL, OFFSET(rx.samples_to_file) }, {
		"demod_mode", plc_setting_enum, "Samples to file", {
			.u32 = demod_mode_none }, 1, &demod_mode_captions, OFFSET(rx.demod_mode) }, {
		"data_offset", plc_setting_u16, "Data offset", {
			.u16 = 500 }, 0, NULL, OFFSET(rx.data_offset) }, {
		"data_hi_threshold_detection", plc_setting_u16, "Data HI Threshold detection", {
			.u16 = 20 }, 0, NULL, OFFSET(rx.data_hi_threshold_detection) }, {
		"gain_rx_pga1", plc_setting_enum, "Gain RX PGA1", {
			.u32 = afe_gain_rx_pga1_025 }, 1, &afe_gain_rx_pga1_captions, OFFSET(rx.gain_rx_pga1) },
	{
		"gain_rx_pga2", plc_setting_enum, "Gain RX PGA2", {
			.u32 = afe_gain_rx_pga2_1 }, 1, &afe_gain_rx_pga2_captions, OFFSET(rx.gain_rx_pga2) }, {
		"operating_mode", plc_setting_enum, "Operating mode", {
			.u32 = operating_mode_none }, 1, &operating_mode_captions, OFFSET(operating_mode) }, {
		"cenelec_a", plc_setting_bool, "CENELEC A", {
			.u32 = 0 }, 0, NULL, OFFSET(cenelec_a) }, {
		"bit_width_us", plc_setting_u32, "Data Bit Width [us]", {
			.u32 = 0 }, 0, NULL, OFFSET(bit_width_us) }, {
		"autostart", plc_setting_bool, "Autostart", {
			.u32 = 0 }, 0, NULL, OFFSET(autostart) }, {
		"quietmode", plc_setting_bool, "Quiet mode", {
			.u32 = 0 }, 0, NULL, OFFSET(quietmode) }, {
		"communication_timeout_ms", plc_setting_u32, "Communication Timeout [ms]", {
			.u32 = 0 }, 0, NULL, OFFSET(communication_timeout_ms) }, {
		"communication_interval_ms", plc_setting_u32, "Communication Interval [ms]", {
			.u32 = 0 }, 0, NULL, OFFSET(communication_interval_ms) } };

#undef OFFSET

int settings_set_app_settings(struct settings *settings, uint32_t settings_count,
		const struct plc_setting settings_src[])
{
	uint32_t n;
	const struct plc_setting *setting = settings_src;
	for (n = settings_count; n > 0; n--, setting++)
	{
		const struct plc_setting_definition *setting_definition = plc_setting_find_definition(
				app_accepted_settings, ARRAY_SIZE(app_accepted_settings), setting->identifier);
		if (setting_definition != NULL)
		{
			union plc_setting_data *dst_data = (union plc_setting_data *) (((char *) settings)
					+ setting_definition->user_data);
			plc_setting_data_release(setting_definition->type, dst_data);
			plc_setting_copy_convert_data(dst_data, setting_definition, setting);
		}
		else
		{
			log_format("Unknown application setting '%s'\n", setting->identifier);
			assert(0);
			return -1;
		}
	}
	return 0;
}

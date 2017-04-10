/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#define _GNU_SOURCE		// Required for 'asprintf' declaration
#include <ctype.h>		// isprint
#include <getopt.h>
#include "+common/api/+base.h"
#include "common.h"
#include "encoder.h"
#include "libraries/libplc-cape/api/afe.h"
#include "settings.h"

#define UI_PLUGIN_NAME_DEFAULT "ui-ncurses"
#define DATA_BIT_US_DEFAULT 1000
#define SPI_DAC_FREQ_HZ_DEFAULT 1500000
#define SAMPLES_FREQ_HZ_DEFAULT 2000
#define SPI_DAC_DELAY_US_DEFAULT 0
#define TX_BUFFERS_LEN_DEFAULT 1024
#define SPI_FILE_SOURCE "spi.bin"
#define MESSAGE_TO_SEND "This is PlcCape. Hello!\n"
// #define MESSAGE_TO_SEND "Hello!\n"
// #define MESSAGE_TO_SEND "ABC..XYZ abc..xyz 0123456789 Hello!\n"

float freq_dac_sps;

// '--help' message
// NOTE: When modifying this section update 'notes.md'
static const char usage_message[] =
	"Usage: plc-cape-lab [OPTION]...\n"
	"\"Laboratory\" to experiment with the PlcCape board\n\n"
	"  -A:MODE       Select ADC receiving mode\n"
	"  -d            Forces the application to use the standard drivers\n"
	"  -D:INTERVAL   Max duration for the test [ms]\n"
	"  -F:BPS        SPI bit rate [bps]\n"
	"  -H:FREQ       Signal frequency [Hz]\n"
	"  -I:INTERVAL   Repetitive test interval [ms]\n"
	"  -L:DELAY      Samples delay [us]\n"
	"  -N:SIZE       Buffer size [samples]\n"
	"  -O:OFFSET     Signal offset [DAC value]\n"
	"  -P:PROFILE    Select a predefined profile\n"
	"  -R:RANGE      Signal range [DAC value]\n"
	"  -S:MODE       SPI transmitting mode\n"
	"  -T:MODE       Operating mode\n"
	"  -U:NAME       UI plugin name (without extension)\n"
	"  -W:SAMPLES    Received samples to be stored in a file\n"
	"  -x            Auto start\n"
	"  -Y:TYPE       Stream type\n"
	"     --help     display this help and exit\n\n"
	"For the arguments requiring an index from a list of options you can get more\n"
	"information specifiying the parameter followed by just a colon\n";

const char *operating_mode_enum_text[operating_mode_COUNT] =
{ "none", "tx_dac", "tx_dac_txpga_txfilter", "tx_dac_txpga_txfilter_pa",
		"tx_dac_txpga_txfilter_pa_rx", "calib_dac_txpga", "calib_dac_txpga_txfilter",
		"calib_dac_txpga_rxpga1_rxfilter_rxpag2", "rx", };

const char *settings_get_usage_message(void)
{
	return usage_message;
}

struct settings *settings_create(void)
{
	struct settings *settings = (struct settings*) calloc(1, sizeof(struct settings));
	settings->ui_plugin_name = strdup(UI_PLUGIN_NAME_DEFAULT);
	settings->operating_mode = operating_mode_none;
	settings->data_bit_us = DATA_BIT_US_DEFAULT;
	settings->tx.freq_bps = SPI_DAC_FREQ_HZ_DEFAULT;
	settings->tx.samples_delay_us = SPI_DAC_DELAY_US_DEFAULT;
	settings->tx.stream_type = stream_ramp;
	settings->tx.tx_buffers_len = TX_BUFFERS_LEN_DEFAULT;
	settings->tx.samples_offset = DAC_RANGE / 2;
	settings->tx.samples_range = DAC_RANGE / 4;
	settings->tx.samples_freq = SAMPLES_FREQ_HZ_DEFAULT;
	settings->tx.dac_filename = strdup(SPI_FILE_SOURCE);
	settings->tx.message_to_send = strdup(MESSAGE_TO_SEND);
	settings->tx.tx_mode = spi_tx_mode_none;
	settings->rx.rx_mode = rx_mode_none;
	settings->configuration_profile = cfg_none;
	settings->monitor_profile = monitor_profile_buffers_processed;
	return settings;
}

void settings_release_resources(struct settings *settings)
{
	if (settings->ui_plugin_name)
	{
		free(settings->ui_plugin_name);
		settings->ui_plugin_name = NULL;
	}
	if (settings->tx.dac_filename)
	{
		free(settings->tx.dac_filename);
		settings->tx.dac_filename = NULL;
	}
	if (settings->tx.message_to_send)
	{
		free(settings->tx.message_to_send);
		settings->tx.message_to_send = NULL;
	}
}

void settings_copy_in_initialized(struct settings *settings, const struct settings *settings_src)
{
	assert((settings->tx.dac_filename == NULL) && (settings->tx.message_to_send == NULL));
	*settings = *settings_src;
	settings->ui_plugin_name = strdup(settings_src->ui_plugin_name);
	settings->tx.dac_filename = strdup(settings_src->tx.dac_filename);
	settings->tx.message_to_send = strdup(settings_src->tx.message_to_send);
}

void settings_release(struct settings *settings)
{
	settings_release_resources(settings);
	free(settings);
}

// POST_CONDITION: Use 'settings_release' to release the object when no longer required
struct settings *settings_clone(const struct settings *settings)
{
	struct settings *settings_cloned = (struct settings*) calloc(1, sizeof(*settings_cloned));
	settings_copy_in_initialized(settings_cloned, settings);
	return settings_cloned;
}

void settings_copy(struct settings *settings, const struct settings *settings_src)
{
	settings_release_resources(settings);
	settings_copy_in_initialized(settings, settings_src);
	settings_update(settings);
}

int set_enum_value_with_checking(const char *arg, int *enum_value, const char *error_description,
		const char **enum_options_text, int enum_options_count, char **error_msg)
{
	static const char *undefined_text = "Undefined";
	if ((arg[0] == ':') && (arg[1] != '\0'))
	{
		*enum_value = atoi(optarg + 1);
		if ((*enum_value >= 0) && (*enum_value < enum_options_count))
			return 1;
	}
	const char *available_options = "Available options:\n";
	// First se calculate the required length for the whole string (+1 for '\n')
	int text_len = strlen(error_description) + 1 + strlen(available_options);
	int n, option_digits = 1, next_option_digit = 10;
	// Limited to 100 options just to simplify the calculation of digits
	for (n = 0; n < enum_options_count; n++)
	{
		if (n == next_option_digit)
		{
			option_digits++;
			next_option_digit *= 10;
		}
		// Each enum item has the following format: "  %d: \n"
		const char *text = enum_options_text[n];
		if (text == NULL)
			text = undefined_text;
		text_len += 2 + option_digits + 2 + strlen(text) + 1;
	}
	// '+1' for ending '\0'
	*error_msg = malloc(text_len + 1);
	char *text_cur = *error_msg;
	text_cur += sprintf(text_cur, "%s\n%s", error_description, available_options);
	for (n = 0; n < enum_options_count; n++)
	{
		const char *text = enum_options_text[n];
		if (text == NULL)
			text = undefined_text;
		text_cur += sprintf(text_cur, "  %d: %s\n", n, text);
	}
	return 0;
}

char *settings_get_info(struct settings *settings)
{
	char *info;
	asprintf(&info,
			"AFE configuration\n"
			" %u:%s\n"
			" Gains tx:%u rx1:%u rx2:%u\n"
			" CENELEC %s\n"
			"TX\n"
			" %u:%s\n"
			" DAC at %u bps\n"
			" DAC freq max %u kHz\n"
			" %u:%s\n"
			" Signal settings\n %u Hz, O:%u, R%u\n"
			" Pregen: %u samples\n"
			" TX buffer: %u\n"
			"RX\n"
			" %u:%s\n"
			" %u:%s\n"
			" ToFile: %u samples\n"
			"Comm timeout: %u ms\n"
			"Comm interval: %u ms\n"
			"%s driver\n",
			settings->operating_mode, operating_mode_enum_text[settings->operating_mode],
			settings->tx.gain_tx_pga, settings->rx.gain_rx_pga1, settings->rx.gain_rx_pga2,
			settings->cenelec_a ? "A" : "B/C/D",
			settings->tx.tx_mode, spi_tx_mode_enum_text[settings->tx.tx_mode],
			settings->tx.freq_bps,
			(uint32_t) (freq_dac_sps / 2000.0),
			settings->tx.stream_type, stream_type_enum_text[settings->tx.stream_type],
			settings->tx.samples_freq, settings->tx.samples_offset, settings->tx.samples_range,
			settings->tx.preload_buffer_len,
			settings->tx.tx_buffers_len,
			settings->rx.rx_mode, rx_mode_enum_text[settings->rx.rx_mode],
			settings->rx.demod_mode, demodulation_mode_enum_text[settings->rx.demod_mode],
			settings->rx.samples_to_file,
			settings->communication_timeout_ms,
			settings->communication_interval_ms,
			settings->std_driver?"STD":"CUSTOM");
	return info;
}

// POST_CONDITION: if (*error_msg != NULL) releate it after use with 'free'
// NOTE: When adding a new option update the '--help' message above
void settings_parse_args(struct settings *settings, int argc, char *argv[], char **error_msg)
{
	int c;
	opterr = 0;
	*error_msg = NULL;
	while ((c = getopt(argc, argv, "A:dD:F:H:I:L:N:O:P:R:S:T:U:W:xY:")) != -1)
		switch (c)
		{
		case 'A':
			if (!set_enum_value_with_checking(optarg, (int*) &settings->rx.rx_mode,
					"Invalid ADC mode!", rx_mode_enum_text, rx_mode_COUNT, error_msg))
				return;
			break;
		case 'd':
			settings->std_driver = 1;
			break;
		case 'D':
			settings->communication_timeout_ms = atoi(optarg + 1);
			break;
		case 'F':
			settings->tx.freq_bps = atoi(optarg + 1) * 1000;
			break;
		case 'H':
			settings->tx.samples_freq = atoi(optarg + 1);
			break;
		case 'I':
			settings->communication_interval_ms = atoi(optarg + 1);
			break;
		case 'L':
			settings->tx.samples_delay_us = atoi(optarg + 1);
			break;
		case 'N':
			settings->tx.tx_buffers_len = atoi(optarg + 1);
			break;
		case 'O':
			settings->tx.samples_offset = atoi(optarg + 1);
			break;
		case 'P':
			if (!set_enum_value_with_checking(optarg, (int*) &settings->configuration_profile,
					"Invalid configuration profile!", configuration_profile_enum_text, cfg_COUNT,
					error_msg))
				return;
			break;
		case 'R':
			settings->tx.samples_range = atoi(optarg + 1);
			break;
		case 'S':
			if (!set_enum_value_with_checking(optarg, (int*) &settings->tx.tx_mode,
					"Invalid SPI transmission mode!", spi_tx_mode_enum_text, spi_tx_mode_COUNT,
					error_msg))
				return;
			break;
		case 'T':
			if (!set_enum_value_with_checking(optarg, (int*) &settings->operating_mode,
					"Invalid operating mode!", operating_mode_enum_text, operating_mode_COUNT,
					error_msg))
				return;
			break;
		case 'U':
			settings->ui_plugin_name = strdup(optarg + 1);
			break;
		case 'W':
			settings->rx.samples_to_file = atoi(optarg + 1);
			break;
		case 'x':
			settings->autostart = 1;
			break;
		case 'Y':
			if ((optarg[0] != ':') || (optarg[1] == '\0'))
			{
				asprintf(error_msg, "Invalid cycle type (-Y)!\nAvailable options:\n"
						"0: Ramp\n1: Triangle\n2: Constant\n3: Freq max\n4: Freq max/2\n"
						"5: Freq Sweep\n6: File '%s'\n7: Bit padding per cycle\n8: AM modulation\n"
						"9: Freq sinus\n10: Freq sinus + OOK Pattern\n", settings->tx.dac_filename);
				return;
			}
			settings->tx.stream_type = (enum stream_type_enum) atoi(optarg + 1);
			break;
		case '?':
			if (optopt == 'c')
				asprintf(error_msg, "Option -%c requires an argument\n", optopt);
			else if (isprint(optopt))
				asprintf(error_msg, "Unknown option '-%c'\n", optopt);
			else
				asprintf(error_msg, "Unknown option character '\\x%x'\n", optopt);
			return;
		default:
			// The 'default' should never be reached
			assert(0);
		}
	// Example to indicate all the invalid commands
	//	int index;
	//	for (index = optind; index < argc; index++)
	//		printf("Non-option argument %s\n", argv[index]);
	// For simplicity just create an error message for the first invalid command found
	if (optind < argc)
	{
		asprintf(error_msg, "Non-option argument %s\n", argv[optind]);
		return;
	}
	settings_update(settings);
}

void settings_update(struct settings *settings)
{
	// SPI DAC valid frequencies = 187.5kHz, 375k, 750k, 1500k, 3M, 6M, 12M...
	uint32_t target_freq = settings->tx.freq_bps;
	for (settings->tx.freq_bps = 12000000; settings->tx.freq_bps > target_freq;
			settings->tx.freq_bps /= 2)
		;
	// 10-data-bits + 0.5 clock between words (TCS) + 165ns CS pulse =
	//	10.5 proportional factor + 165ns fixed time
	freq_dac_sps = 1.0 / (10.5 / (float) settings->tx.freq_bps + 165e-9);
}
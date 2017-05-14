/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#define _GNU_SOURCE				// 'asprintf'
#include <ctype.h>				// isprint
#include <getopt.h>
#include "+common/api/+base.h"
#include "common.h"
#include "libraries/libplc-cape/api/tx.h"
#include "libraries/libplc-tools/api/cmdline.h"
#include "settings.h"

// '--help' message
// NOTE: When modifying this section update 'notes.md'
static const char usage_message[] = "Usage: plc-cape-lab [OPTION]...\n"
		"\"Laboratory\" to experiment with the PlcCape board\n\n"
		"  -A:MODE       Select ADC receiving mode\n"
		"  -d            Forces the application to use the standard drivers\n"
		"  -D:id=value   Speficy a DECODER 'value' for a setting identified as 'id'\n"
		"  -E:id=value   Speficy an ENCODER 'value' for a setting identified as 'id'\n"
		"  -F:SPS        SPI sampling rate [sps]\n"
		"  -I:INTERVAL   Repetitive test interval [ms]\n"
		"  -J:INTERVAL   Max duration for the test [ms]\n"
		"  -L:DELAY      Samples delay [us]\n"
		"  -N:SIZE       Buffer size [samples]\n"
		"  -P:PROFILE    Select a predefined profile\n"
		"  -q            Quiet mode\n"
		"  -S:MODE       SPI transmitting mode\n"
		"  -T:MODE       Operating mode\n"
		"  -U:NAME       UI plugin name (without extension)\n"
		"  -W:SAMPLES    Received samples to be stored in a file\n"
		"  -x            Auto start\n"
		"  -Y:TYPE       Stream type\n"
		"     --help     display this help and exit\n\n"
		"For the arguments requiring an index from a list of options you can get more\n"
		"information specifiying the parameter followed by just a colon\n";

const char *cmdline_get_usage_message(void)
{
	return usage_message;
}

// POST_CONDITION: if (*error_msg != NULL) releate it after use with 'free'
// NOTE: When adding a new option update the '--help' message above
void cmdline_parse_args(int argc, char *argv[], char **error_msg, struct settings *settings,
		char **ui_plugin_name, char **initial_profile,
		struct setting_list_item **encoder_setting_list,
		struct setting_list_item **decoder_setting_list)
{
	int c;
	// Disable 'getopt' messages printed to stderr because we use here a custom handler
	opterr = 0;
	*error_msg = NULL;
	while ((c = getopt(argc, argv, "A:dD:E:F:I:J:L:N:P:qS:T:U:W:xY:")) != -1)
		switch (c)
		{
		case 'A':
			if ((*error_msg = plc_cmdline_set_enum_value_with_checking(optarg,
					(int*) &settings->rx.rx_mode, "ADC mode", rx_mode_enum_text,
					rx_mode_COUNT)) != NULL)
				return;
			break;
		case 'd':
			settings->std_driver = 1;
			break;
		case 'D':
		case 'E':
		{
			struct plc_setting setting;
			char *identifier_end = strchr(optarg + 1, '=');
			if (identifier_end == NULL)
			{
				asprintf(error_msg, "Invalid plugin setting '%s'. Expected format 'id=value'\n",
						optarg + 1);
				return;
			}
			setting.identifier = strndup(optarg + 1, identifier_end - (optarg + 1));
			setting.type = plc_setting_string;
			setting.data.s = identifier_end + 1;
			plc_setting_set_setting((c == 'E') ? encoder_setting_list : decoder_setting_list,
					&setting);
			free(setting.identifier);
			break;
		}
		case 'F':
			settings->tx.sampling_rate_sps = atoi(optarg + 1);
			break;
		case 'I':
			settings->communication_interval_ms = atoi(optarg + 1);
			break;
		case 'J':
			settings->communication_timeout_ms = atoi(optarg + 1);
			break;
		case 'L':
			settings->tx.samples_delay_us = atoi(optarg + 1);
			break;
		case 'N':
			settings->tx.tx_buffers_len = atoi(optarg + 1);
			break;
		case 'P':
			if (*initial_profile)
				free(*initial_profile);
			*initial_profile = strdup(optarg + 1);
			break;
		case 'q':
			settings->quietmode = 1;
			break;
		case 'S':
			if ((*error_msg = plc_cmdline_set_enum_value_with_checking(optarg,
					(int*) &settings->tx.tx_mode, "SPI transmission mode", spi_tx_mode_enum_text,
					spi_tx_mode_COUNT)) != NULL)
				return;
			break;
		case 'T':
			if ((*error_msg = plc_cmdline_set_enum_value_with_checking(optarg,
					(int*) &settings->operating_mode, "operating mode", operating_mode_enum_text,
					operating_mode_COUNT)) != NULL)
				return;
			break;
		case 'U':
			if (*ui_plugin_name)
				free(*ui_plugin_name);
			*ui_plugin_name = strdup(optarg + 1);
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
						"5: Freq Sweep\n6: File\n7: Bit padding per cycle\n8: AM modulation\n"
						"9: Freq sinus\n10: Freq sinus + OOK Pattern\n");
				return;
			}
			break;
		case '?':
			if (isprint(optopt))
				asprintf(error_msg, "Unknown option '-%c' or requires an argument\n", optopt);
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
}

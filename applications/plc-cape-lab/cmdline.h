/**
 * @file
 * @brief	Command line options processing
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef CMDLINE_H
#define CMDLINE_H

struct settings;
struct setting_list_item;

const char *cmdline_get_usage_message(void);
void cmdline_parse_args(int argc, char *argv[], char **error_msg, struct settings *settings,
		char **ui_plugin_name, char **initial_profile,
		struct setting_list_item **encoder_setting_list,
		struct setting_list_item **decoder_setting_list);

#endif /* CMDLINE_H */

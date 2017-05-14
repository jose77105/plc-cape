/**
 * @file
 * 
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include "+common/api/+base.h"
#include "api/cmdline.h"

ATTR_EXTERN char *plc_cmdline_set_enum_value_with_checking(const char *arg, int *enum_value,
		const char *enum_option_title, const char **enum_options_text, int enum_options_count)
{
	static const char *undefined_text = "Undefined";
	if ((arg[0] == ':') && (arg[1] != '\0'))
	{
		*enum_value = atoi(arg + 1);
		if ((*enum_value >= 0) && (*enum_value < enum_options_count))
			return NULL;
	}
	// First calculate the required length for the whole string (+9 ; +1 for '\n')
	static const char *error_msg_pre = "Invalid ";
	static const char *error_msg_post = "!";
	static const char *available_options = "Available options:\n";
	int text_len = strlen(error_msg_pre) + strlen(enum_option_title) + strlen(error_msg_post) + 1 +
		strlen(available_options);
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
	char *error_msg = malloc(text_len + 1);
	char *text_cur = error_msg;
	text_cur += sprintf(text_cur, "%s%s%s\n%s", error_msg_pre, enum_option_title, error_msg_post,
		available_options);
	for (n = 0; n < enum_options_count; n++)
	{
		const char *text = enum_options_text[n];
		if (text == NULL)
			text = undefined_text;
		text_cur += sprintf(text_cur, "  %d: %s\n", n, text);
	}
	assert(text_cur == error_msg + text_len);
	return error_msg;
}

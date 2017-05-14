/**
 * @file
 * @brief	Helper functions related to the command line management
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef LIBPLC_TOOLS_CMDLINE_H
#define LIBPLC_TOOLS_CMDLINE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief	Sets the value of an enumeration given the index, checking if within the accepted values
 * @param	arg					The value of the command line argument (e.g. ":filename.txt")
 * @param	enum_value			A pointer to the enumeration value to be updated
 * @param	enum_option_title	A string identifying the enum option
 * @param	enum_options_text	A pointer to an array with the available enum options
 * @param	enum_options_count	The amount of available enum options
 * @return	A pointer to NULL if 'enum_value' as properly set or a pointer to a string if error
 *			The pointer must be released with 'free'
 */
char *plc_cmdline_set_enum_value_with_checking(const char *arg, int *enum_value,
		const char *enum_option_title, const char **enum_options_text, int enum_options_count);

#ifdef __cplusplus
}
#endif

#endif /* LIBPLC_TOOLS_CMDLINE_H */

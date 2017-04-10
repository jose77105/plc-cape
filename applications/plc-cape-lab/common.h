/**
 * @file
 * @brief	Common functions
 * @details	Common functions to be used by any file in the application
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef COMMON_H
#define COMMON_H

#include "error.h"
#include "logger.h"

// Generic settings
#define DAC_RANGE 1024
#define ADC_MAX_RANGE 4096

extern struct logger *logger;

extern float freq_dac_sps;

/**
 * @brief	Logs a message through the current active logger
 * @details
 *			It adds a carriage return to the message provided
 * @param	text	The message to be logged (without '\\n') 
 */
void log_line(const char *text);
/**
 * @brief	Logs a message with format through the current active logger
 * @details
 *			It doesn't add a carriage return to the message provided
 *			The _format_ parameter must be 'printf'-compliant
 * @param	format	The format of the message to be logged
 *					Add a ending '\\n' if the logging message is completed
 * @param	...		Variables used on the _format_ parameter
 */
void log_format(const char *format, ...);
/**
 * @brief	Logs a single character through the current active logger
 * @param	c	The character to be logged
 */
void log_char(char c);
/**
 * @brief	Logs a notification message (usually an error condition) and exits the application
 * @param	text	The ending message
 */
void log_and_exit(const char *text);
/**
 * @brief	Logs the last error occurred with an additional _text_ message
 * @details	Is the _plc-cape_ version of the classical _perror(text)_ + _exit(EXIT_FAILURE)_
 * @param	text	The additional message before the last error
 */
void log_errno_and_exit(const char *text);

/**
 * @brief	Trace a message using the current active logger
 * @param	function_name	The name of the function that is issuing the tracing
 * @param	format			The message to log either in plain text or with _printf_ format
 * @param	...				Optional arguments if required by _format_
 */
void plc_trace(const char *function_name, const char *format, ...);

#endif /* COMMON_H */

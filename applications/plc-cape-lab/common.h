/**
 * @file
 * @brief	Common functions
 * @details	Common functions to be used by any file in the application
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef COMMON_H
#define COMMON_H

#include "error.h"
#include "logger.h"

extern struct logger *logger;

extern int in_emulation_mode;

/**
 * @brief	Exists the application in a safe-controlled way
 * @details
 *			It exists the application releasing the opened resources
 */
void safe_exit(void);
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
void log_line_and_exit(const char *text);
/**
 * @brief	Logs the last error occurred with an additional _text_ message
 * @details	It's the _plc-cape_ version of the classical _perror(text)_ + _exit(EXIT_FAILURE)_
 * @param	text	The additional message before the last error
 */
void log_errno_and_exit(const char *text);

#endif /* COMMON_H */

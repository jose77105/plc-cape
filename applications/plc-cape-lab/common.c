/**
 * @file
 * 
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include <errno.h>		// errno
#include <fcntl.h>
#include <stdarg.h>		// va_list
#include <unistd.h>
#include "+common/api/+base.h"
#include "common.h"

void safe_exit(void)
{
	log_line("Press a key to exit...");
	getchar();
	exit(EXIT_FAILURE);
	// NOTE: 'abort' doesn't call 'atexit'
	// abort();
}

void log_line(const char *text)
{
	// NOTE: on 'ncurses' 'printf' doesn't produce results because 'stdout' is hidden
	//	However they are properly printed if using 'stderr'
	if (logger)
		logger_log_line(logger, text);
	else
		fprintf(stderr, "%s\n", text);
}

void log_format_va(const char *format, va_list args)
{
	if (logger)
		logger_log_sequence_format_va(logger, format, args);
	else
		vfprintf(stderr, format, args);
}

void log_format(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	log_format_va(format, args);
	va_end(args);
}

void log_line_and_exit(const char *text)
{
	log_line(text);
	safe_exit();
}

void log_errno_and_exit(const char *text)
{
	log_format("%s: %s\n", text, strerror(errno));
	safe_exit();
}

#ifdef DEBUG
void plc_trace_log(const char *function_name, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	// Prefix the logging message with the function name
	log_format("%s: ", function_name);
	log_format_va(format, args);
	// Add a new line
	log_line("");
	va_end(args);
}

void (*plc_trace)(const char *function_name, const char *format, ...) = plc_trace_log;
#endif

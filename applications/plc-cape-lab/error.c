/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#define _GNU_SOURCE		// Required for 'vasprintf' declaration
#include <errno.h>		// errno
#include <stdarg.h>		// va_list
#include "+common/api/+base.h"
// Declaration of 'plc_error_api_h' to avoid hard-castings
// TODO: in this specific case (singleton) 'error_ctrl' not required -> Remove
typedef struct error_ctrl* plc_error_api_h;
#define PLC_ERROR_API_HANDLE_EXPLICIT_DEF
#include "+common/api/error.h"
#include "common.h"
#include "logger.h"

static struct plc_error_api plc_error_api;
static char *last_error_msg;

static void error_ctrl_set_error_msg_va(plc_error_api_h error, const char *format, va_list args)
{
	// TODO: Activate loging with an option for tracing
	// logger_log_sequence_format_va(logger, format, args);
	// TODO: Improve carriage return control
	// logger_log_line(logger, "");
	if (last_error_msg)
		free(last_error_msg);
	int chars = vasprintf(&last_error_msg, format, args);
	assert(chars >= 0);
}

static void error_ctrl_set_error_msg(plc_error_api_h error, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	error_ctrl_set_error_msg_va(error, format, args);
	va_end(args);
}

void error_ctrl_initialize(void)
{
	plc_error_api.set_error_msg = error_ctrl_set_error_msg;
	plc_error_api.set_error_msg_va = error_ctrl_set_error_msg_va;
}

void error_ctrl_terminate(void)
{
	if (last_error_msg)
		free(last_error_msg);
}

struct plc_error_api *error_ctrl_get_interface(void)
{
	return &plc_error_api;
}

const char* get_last_error(void)
{
	if (last_error_msg)
		return last_error_msg;
	else if (errno != 0)
		return strerror(errno);
	else
		return "Undefined error";
}

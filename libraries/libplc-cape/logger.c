/**
 * @file
 * 
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include "+common/api/+base.h"
#include "+common/api/logger.h"
#include "logger.h"

void log_line_default(const char *msg)
{
	// TODO: Revise strategy. 'warnx' could be also used. Think on a common methods
	//	(libplc-adc also uses logging)
	fputs(msg, stderr);
	fputs("\n", stderr);
	fputs("\n", stderr);
}

// NOTE! 'libplc_cape_set_logger_msg' must be declared ATTR_EXTERN to become internally public
//	Side-effect: it becomes externally public too
//	More information on 'error.c' because same problem
ATTR_EXTERN void (*libplc_cape_log_line)(const char *text) = log_line_default;

struct plc_logger_api *logger_api;
void *logger_handle;

void log_line_wrapper(const char *msg)
{
	logger_api->log_line(logger_handle, msg);
}

void libplc_cape_logger_initialize(struct plc_logger_api *api, void *handle)
{
	logger_api = api;
	logger_handle = handle;
	libplc_cape_log_line = log_line_wrapper;
}

/**
 * @file
 * 
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include <errno.h>		// errno
#include <stdarg.h>		// va_list
#include "+common/api/+base.h"
#include "+common/api/error.h"
#include "error.h"

void set_error_msg_default(const char *format, ...)
{
}

void set_error_msg_errno_default(const char *title)
{
}

// NOTE! 'libplc_cape_set_error_msg' must be declared ATTR_EXTERN
//	Otherwise it will be considered 'static' (if the CFLAG '-fwhole-program' is set) and then not
//	exported. In that case the occurences of 'libplc_cape_set_error_msg' behind an 'extern'
//	declaration would be marked as unresolved
// TODO: Check for refactoring
ATTR_EXTERN void (*libplc_cape_set_error_msg)(const char *format, ...) = set_error_msg_default;
ATTR_EXTERN void (*libplc_cape_set_error_msg_errno)(const char *title) =
		set_error_msg_errno_default;

// Error reporting function
static struct plc_error_api *plc_error_api;
static void *error_ctrl_handle;

void set_error_msg_wrapper(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	plc_error_api->set_error_msg_va(error_ctrl_handle, format, args);
	va_end(args);
}

void set_error_msg_errno_wrapper(const char *title)
{
	plc_error_api->set_error_msg(error_ctrl_handle, "%s: %s", title, strerror(errno));
}

void libplc_cape_error_initialize(struct plc_error_api *api, void *handle)
{
	plc_error_api = api;
	error_ctrl_handle = handle;
	libplc_cape_set_error_msg = set_error_msg_wrapper;
	libplc_cape_set_error_msg_errno = set_error_msg_errno_wrapper;
}

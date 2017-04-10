/**
 * @file
 * 
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include <stdarg.h>		// va_list
#include "+common/api/+base.h"
#include "api/trace.h"

// Default tracing function
// We define it even for RELEASE versions just to avoid a segmentation fault in case an application
// build in DEBUG uses a RELEASE version of the libplc-tools
ATTR_EXTERN void plc_trace_default(const char *function_name, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	// Prefix the logging message with the function name
	printf("%s: ", function_name);
	vprintf(format, args);
	// Add a new line
	puts("");
	va_end(args);
}

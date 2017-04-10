/**
 * @file
 * @brief	Tracing helper functions
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef LIBPLC_TOOLS_TRACE_H
#define LIBPLC_TOOLS_TRACE_H

/**
 * @brief	An implementation of a classical tracing method compatible with @ref plc_trace and
 *			related macros
 * @param	function_name	The name of the caller function to be logged
 * @param	format			The format of the message to be traced _printf_-compliant
 * @details	The implementation of the tracing relays on _printf_. So it can be only used if the
 *			traces can be sent to _stdout_
 */
void plc_trace_default(const char *function_name, const char *format, ...);

#endif /* LIBPLC_TOOLS_TRACE_H */

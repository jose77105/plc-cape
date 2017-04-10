/**
 * @file
 * @brief	Global interface for error management within the \c plc-cape project
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef COMMON_ERROR_H
#define COMMON_ERROR_H

#ifndef PLC_ERROR_API_HANDLE_EXPLICIT_DEF
typedef void* plc_error_api_h;
#endif

// PRECONDITION: error management should be thread-local
// NOTE: Info on std error managmenent available here:
//	http://www.gnu.org/software/libc/manual/html_mono/libc.html#Checking-for-Errors

/**
 * @brief	Common error interface identified as 'singleton_id_error'
 * @details
 *			Interface used by 'singleton' providers and consumers
 */
struct plc_error_api {
	void (*set_error_msg)(plc_error_api_h handle, const char *format, ...);
	void (*set_error_msg_va)(plc_error_api_h handle, const char *format, va_list args);
};

#endif /* COMMON_ERROR_H */
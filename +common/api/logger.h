/**
 * @file
 * @brief	Global interface for logging within the \c plc-cape project
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef COMMON_LOGGER_H
#define COMMON_LOGGER_H

#ifndef PLC_LOGGER_API_HANDLE_EXPLICIT_DEF
typedef void* plc_logger_api_h;
#endif

/**
 * @brief	Common logger interface identified as 'singleton_id_logger'
 * @details
 *			Interface used by 'singleton' providers and consumers
 */
struct plc_logger_api {
	void (*log_line)(plc_logger_api_h handle, const char *text);
	void (*log_sequence)(plc_logger_api_h handle, const char *text);
	void (*log_sequence_format)(plc_logger_api_h handle, const char *format, ...);
	void (*log_sequence_format_va)(plc_logger_api_h handle, const char *format, va_list args);
};

#endif /* COMMON_LOGGER_H */
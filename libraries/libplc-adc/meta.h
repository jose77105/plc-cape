/**
 * @file
 * @brief	Meta-library functions (internal)
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef META_H
#define META_H

// 'singleton' object for logging
extern struct plc_logger_api *plc_adc_logger_api;
extern void *plc_adc_logger_handle;

// Shortcut for most usual logging interface
void plc_libadc_log_line(const char *msg);

#endif /* META_H */

/**
 * @file
 * @brief	Internal interface for logging
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef LOGGER_H
#define LOGGER_H

struct plc_logger_api;

extern void (*libplc_cape_log_line)(const char *text);

ATTR_INTERN void libplc_cape_logger_initialize(struct plc_logger_api *logger_api, void *logger_handle);

#endif /* LOGGER_H */

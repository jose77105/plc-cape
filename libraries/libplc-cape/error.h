/**
 * @file
 * @brief	Internal interface for error management
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef ERROR_H
#define ERROR_H

struct plc_error_api;

extern void (*libplc_cape_set_error_msg)(const char *format, ...);
extern void (*libplc_cape_set_error_msg_errno)(const char *title);

ATTR_INTERN void libplc_cape_error_initialize(struct plc_error_api *plc_error_api,
		void *error_ctrl_handle);

#endif /* ERROR_H */

/**
 * @file
 * @brief	Functions for error control
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef ERROR_H
#define ERROR_H

struct plc_error_api;

void error_ctrl_initialize(void);
void error_ctrl_terminate(void);
struct plc_error_api *error_ctrl_get_interface(void);

// Shortcuts
const char* get_last_error(void);

#endif /* ERROR_H */

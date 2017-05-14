/**
 * @file
 * @brief	Common functions and declarations used in the plugin
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef COMMON_H
#define COMMON_H

#define KEY_CANCEL '\x1b'
#define KEY_OK '\n'

#define PLUGINS_API_HANDLE_EXPLICIT_DEF
typedef struct tui *ui_api_h;
#include "plugins/ui/api/ui.h"

#endif /* COMMON_H */

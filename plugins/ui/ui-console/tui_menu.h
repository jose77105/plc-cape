/**
 * @file
 * @brief	Menu management
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef TUI_MENU_H
#define TUI_MENU_H

#include "common.h"

struct ui_menu_item;
struct tui_panel;

struct tui_panel *tui_menu_create(struct tui_panel *parent,
		const struct ui_menu_item *menu_items, uint32_t menu_items_count, const char *title,
		void (*on_cancel)(ui_callbacks_h), ui_callbacks_h callbacks_handle);

#endif /* TUI_MENU_H */

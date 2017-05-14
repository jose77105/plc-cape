/**
 * @file
 * @brief	Popup menus
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef TUI_MENU_H
#define TUI_MENU_H

#include "+common/api/ui.h"

struct tui;
struct tui_menu;
struct ui_menu_item;
struct tui_panel;

struct tui_menu *tui_menu_create(struct tui_panel *parent,
		const struct ui_menu_item *menu_items, uint32_t menu_items_count, const char *title,
		void (*on_cancel)(ui_callbacks_h), ui_callbacks_h callbacks_handle, pthread_mutex_t *lock);
struct tui_panel *tui_menu_get_as_panel(struct tui_menu *tui_menu);
struct tui_menu *tui_menu_get_from_panel(struct tui_panel *tui_panel);
void tui_menu_release(struct tui_menu *tui_menu);
void tui_open_menu(struct tui *tui, const struct ui_menu_item *menu_items,
		uint32_t menu_items_count, const char *title, void (*on_cancel)(ui_callbacks_h));

#endif /* TUI_MENU_H */

/**
 * @file
 * @brief	Dialog management
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef TUI_DIALOG_H
#define TUI_DIALOG_H

#include "common.h"

struct ui_dialog_item;
struct tui_panel;

struct tui_panel *tui_dialog_create(struct tui_panel *parent,
		const struct ui_dialog_item *dialog_items, uint32_t dialog_items_count, const char *title,
		void (*on_cancel)(ui_callbacks_h), void (*on_ok)(ui_callbacks_h),
		ui_callbacks_h callbacks_handle);

#endif /* TUI_DIALOG_H */

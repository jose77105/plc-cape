/**
 * @file
 * @brief	Popup dialogs
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef TUI_DIALOG_H
#define TUI_DIALOG_H

#include "+common/api/ui.h"

struct tui;

void tui_open_dialog(struct tui *tui, const struct ui_dialog_item *dialog_items,
		uint32_t dialog_items_count, const char *title, void (*on_cancel)(ui_callbacks_h),
		void (*on_ok)(ui_callbacks_h));

#endif /* TUI_DIALOG_H */

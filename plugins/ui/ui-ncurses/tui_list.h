/**
 * @file
 * @brief	Popup list boxes
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef TUI_LIST_H
#define TUI_LIST_H

#include "+common/api/ui.h"

typedef void * list_callback_h;
struct tui;

void tui_open_list(struct tui *tui, const char **text_items, uint32_t text_items_count,
		const char *title, void (*on_cancel)(list_callback_h),
		void (*on_ok)(list_callback_h, int selected_index), list_callback_h handle);

#endif /* TUI_LIST_H */

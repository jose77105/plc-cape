/**
 * @file
 * @brief	Main UI management based on the _ncurses_ package
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef TUI_H
#define TUI_H

#include "+common/api/ui.h"
struct tui;

struct tui_panel
{
	void (*tui_panel_release)(struct tui_panel *tui_panel);
	void (*tui_panel_get_dimensions)(struct tui_panel *tui_panel, int *pos_y, int *pos_x, int *rows,
			int *cols);
	void (*tui_panel_proces_key)(struct tui_panel *tui_panel, int c);
	struct tui_panel *parent;
};

void tui_active_panel_close(struct tui *tui);
ui_callbacks_h tui_get_callbacks_handle(struct tui *tui);
struct tui_panel *tui_get_active_panel(struct tui *tui);
void tui_set_active_panel(struct tui *tui, struct tui_panel *panel);

#endif /* TUI_H */

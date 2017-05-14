/**
 * @file
 * @brief	Panel declarations
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef TUI_PANEL_H
#define TUI_PANEL_H

#include "common.h"

#ifndef TUI_PANEL_HANDLE_EXPLICIT_DEF
typedef struct tui_panel *tui_panel_h;
#endif

// PRECONDITION: For simplicity, the 'struct tui_panel' must always be the first element in any
//	derived struct
struct tui_panel
{
	char *title;
	struct tui_panel *parent;
	ui_callbacks_h callbacks_handle;
	void (*on_cancel)(ui_callbacks_h);
	void (*release)(tui_panel_h);
	void (*display)(tui_panel_h);
	void (*process_key)(tui_panel_h, int c);
};

#endif /* TUI_PANEL_H */

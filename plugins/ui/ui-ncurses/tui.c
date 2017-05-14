/**
 * @file
 * @brief	**Main** file
 *	
 * @see		@ref plugin-ui-ncurses
 *
 * @cond COPYRIGHT_NOTES
 *
 * ##LICENSE
 *
 *		This file is part of plc-cape project.
 *
 *		plc-cape project is free software: you can redistribute it and/or modify
 *		it under the terms of the GNU General Public License as published by
 *		the Free Software Foundation, either version 3 of the License, or
 *		(at your option) any later version.
 *
 *		plc-cape project is distributed in the hope that it will be useful,
 *		but WITHOUT ANY WARRANTY; without even the implied warranty of
 *		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *		GNU General Public License for more details.
 *
 *		You should have received a copy of the GNU General Public License
 *		along with plc-cape project.  If not, see <http://www.gnu.org/licenses/>. 
 *
 * @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega
 * 
 * @endcond
 */

#include <ncurses.h>
#include <panel.h>
#include <pthread.h>		// pthread_mutex_t
#include "+common/api/+base.h"
#include "+common/api/logger.h"
#include "common.h"
// Declare the custom type used as handle. Doing it like this avoids the 'void*' hard-casting
#define PLUGINS_API_HANDLE_EXPLICIT_DEF
typedef struct tui *ui_api_h;
#include "plugins/ui/api/ui.h"
#include "tui.h"
#include "tui_dialog.h"
#include "tui_menu.h"

#define COLOR_ID_RED 1
#define COLOR_ID_YELLOW 2
#define COLOR_ID_RED_INVERTED 3

#define LOG_MIN_COLS 20
#define SETTINGS_PREFERRED_COLS 30

enum tui_windows_enum
{
	tui_windows_log_border = 0,
	tui_windows_log,
	tui_windows_settings_border,
	tui_windows_settings,
	tui_windows_status_bar,
	tui_windows_flags,
	tui_windows_COUNT
};

struct tui_rect
{
	int top;
	int left;
	int height;
	int width;
};

// PRECONDITION: 'struct xui' must always be the first element in the struct to apply kind of 
//	"polymorfism"
struct tui
{
	// For simplicity, lock the access to multi-threaded calls
	pthread_mutex_t lock;
	ui_callbacks_h callbacks_handle;
	int quit;
	int main_menu_width;
	WINDOW *log_border_window;
	PANEL *log_border_panel;
	WINDOW *log_window;
	PANEL *log_panel;
	WINDOW *settings_border_window;
	PANEL *settings_border_panel;
	WINDOW *settings_window;
	PANEL *settings_panel;
	char *settings_last_content;
	WINDOW *status_bar_window;
	PANEL *status_bar_panel;
	WINDOW *flags_window;
	PANEL *flags_panel;
	struct tui_panel *active_panel;
};

// Connection with the 'singletons_provider'
singletons_provider_get_t singletons_provider_get = NULL;
singletons_provider_h singletons_provider_handle = NULL;

// Logger served by the 'singletons_provider'
struct plc_logger_api *logger_api;
void *logger_handle;

ATTR_INTERN void log_line(const char *msg)
{
	if (logger_api)
		logger_api->log_line(logger_handle, msg);
	else
		fprintf(stderr, "%s\n", msg);
}

int tui_init(int argc, char *argv[])
{
	return 1;
}

void tui_set_tui_rect(struct tui_rect *tui_rect, int height, int width, int top, int left)
{
	tui_rect->top = top;
	tui_rect->left = left;
	tui_rect->height = height;
	tui_rect->width = width;
}

void tui_calculate_windows_layout(int rows, int cols, int menu_cols,
		struct tui_rect layout[tui_windows_COUNT])
{
	int log_col = menu_cols;
	int right_cols = cols - log_col;
	// If space enough use 'SETTINGS_PREFERRED_COLS'. Otherwise use a 1/2 ratio for each window
	int log_cols;
	int log_rows = rows - 3;
	if (right_cols - SETTINGS_PREFERRED_COLS >= LOG_MIN_COLS)
		log_cols = right_cols - SETTINGS_PREFERRED_COLS;
	else
		log_cols = right_cols / 2;
	int settings_col = log_col + log_cols;
	int settings_cols = right_cols-log_cols;
	tui_set_tui_rect(&layout[tui_windows_log_border], log_rows, log_cols, 0, log_col);
	tui_set_tui_rect(&layout[tui_windows_log], log_rows - 2, log_cols - 2, 1, 1);
	tui_set_tui_rect(&layout[tui_windows_settings_border],
			log_rows, settings_cols, 0, settings_col);
	tui_set_tui_rect(&layout[tui_windows_settings], log_rows - 2, settings_cols - 2, 1, 1);
	tui_set_tui_rect(&layout[tui_windows_status_bar], 1, cols - 20, rows - 1, 0);
	tui_set_tui_rect(&layout[tui_windows_flags], 1, 20, rows - 1, cols - 20);
}

WINDOW *tui_create_window(struct tui_rect *tui_rect)
{
	return newwin(tui_rect->height, tui_rect->width, tui_rect->top, tui_rect->left);
}

WINDOW *tui_create_child_window(WINDOW *window, struct tui_rect *tui_rect)
{
	return derwin(window, tui_rect->height, tui_rect->width, tui_rect->top, tui_rect->left);
}

void tui_resize_window(WINDOW *window, struct tui_rect *tui_rect, int is_child_window)
{
	if (!is_child_window) mvwin(window, tui_rect->top, tui_rect->left);
	wresize(window, tui_rect->height, tui_rect->width);
}

void tui_resize_panel(PANEL *panel, WINDOW **window, struct tui_rect *tui_rect,
		WINDOW *parent_window)
{
	WINDOW *new_window;
	new_window = (parent_window != NULL) ?
		tui_create_child_window(parent_window, tui_rect) : tui_create_window(tui_rect);
	replace_panel(panel, new_window);
	delwin(*window);
	*window = new_window;
}


void tui_draw_borders(struct tui *tui)
{
	wborder(tui->log_border_window, '|', '|', '-', '-', '+', '+', '+', '+');
	tui_window_print_at_pos(tui->log_border_window, 0, 2, "Log", COLOR_PAIR(1));
	wborder(tui->settings_border_window, '|', '|', '-', '-', '+', '+', '+', '+');
	tui_window_print_centered_title(tui->settings_border_window, "Settings", COLOR_PAIR(1));
}

struct tui *tui_create(const char *title, const struct ui_menu_item *main_menu_items,
		uint32_t main_menu_items_count, ui_callbacks_h callbacks_handle)
{
	int ret;
	struct tui *tui = (struct tui*) calloc(1, sizeof(struct tui));
	pthread_mutexattr_t mutex_attr;
	ret = pthread_mutexattr_init(&mutex_attr);
	assert(ret == 0);
	// PTHREAD_MUTEX_RECURSIVE to deal with mutex_lock within the same thread without blocking
	ret = pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
	assert(ret == 0);
	ret = pthread_mutex_init(&tui->lock, &mutex_attr);
	assert(ret == 0);
	ret = pthread_mutexattr_destroy(&mutex_attr);
	// Start ncurses mode
	initscr();
	// hide_cursor
	curs_set(0);
	// Use 'cbreak' to hook CTRL+C
	// cbreak();
	noecho();
	// We get F1, F2 etc..
	keypad(stdscr, TRUE);
	// ncurses uses by default "ESCape sequences" to manage the combination ALT+<key>
	//	http://stackoverflow.com/questions/5977395/ncurses-and-esc-alt-keys
	// A simpler alternative is to use 'set_escdelay(0)' is the ALT+<key> is not used
	set_escdelay(0);
	if (has_colors())
	{
		start_color();
		init_pair(COLOR_ID_RED, COLOR_RED, COLOR_BLACK);
		init_pair(COLOR_ID_YELLOW, COLOR_YELLOW, COLOR_BLACK);
		init_pair(COLOR_ID_RED_INVERTED, COLOR_BLACK, COLOR_RED);
	}
	// Menu Window
	tui->callbacks_handle = callbacks_handle;
	struct tui_menu *tui_menu = tui_menu_create(
		NULL, main_menu_items, main_menu_items_count, title, NULL, callbacks_handle, &tui->lock);
	int menu_y, menu_x, menu_rows, menu_cols;
	tui_set_active_panel(tui, tui_menu_get_as_panel(tui_menu));
	tui->active_panel->tui_panel_get_dimensions(tui->active_panel,
			&menu_y, &menu_x, &menu_rows, &menu_cols);
	struct tui_rect layout[tui_windows_COUNT];
	tui->main_menu_width = menu_x + menu_cols - 1;
	tui_calculate_windows_layout(LINES, COLS, tui->main_menu_width, layout);
	// Log window
	// NOTE: It's possible to write to the graphic screen in 'ncurses' with 'printw' and equivalent
	//	but here it's more convenient to use the windowed functiones as 'wprintw' (prefixed with 
	//	'w') which implement for example the clipping
	tui->log_border_window = tui_create_window(&layout[tui_windows_log_border]);
	// Creating a 'panel' attached to a 'window' allows 'ncurses' to automatically control the
	// overlapping between windows
	// Warning: panels work with 'update_panels' + 'doupdate' instead of 'wrefresh'
	tui->log_border_panel = new_panel(tui->log_border_window);
	tui->log_window = tui_create_child_window(tui->log_border_window, &layout[tui_windows_log]);
	tui->log_panel = new_panel(tui->log_window);
	scrollok(tui->log_window, TRUE);
	// Settings window
	tui->settings_border_window = tui_create_window(&layout[tui_windows_settings_border]);
	tui->settings_border_panel = new_panel(tui->settings_border_window);
	// Note: we can draw isolated lines with 'mvvline'
	//	mvvline(1, log_col + log_cols + 1, '|', LINES - 5);
	tui->settings_window = tui_create_child_window(tui->settings_border_window,
		 &layout[tui_windows_settings]);
	tui->settings_panel = new_panel(tui->settings_window);
	// Status bar
	tui->status_bar_window = tui_create_window(&layout[tui_windows_status_bar]);
	tui->status_bar_panel = new_panel(tui->status_bar_window);
	// Flags area
	tui->flags_window = tui_create_window(&layout[tui_windows_flags]);
	tui->flags_panel = new_panel(tui->flags_window);
	tui_draw_borders(tui);
	return tui;
}

void tui_release(struct tui *tui)
{
	tui->active_panel->tui_panel_release(tui->active_panel);
	del_panel(tui->flags_panel);
	del_panel(tui->status_bar_panel);
	if (tui->settings_last_content) free(tui->settings_last_content);
	del_panel(tui->settings_panel);
	del_panel(tui->settings_border_panel);
	del_panel(tui->log_panel);
	del_panel(tui->log_border_panel);
	delwin(tui->settings_window);
	delwin(tui->flags_window);
	delwin(tui->status_bar_window);
	delwin(tui->log_window);
	delwin(tui->log_border_window);
	endwin();
	pthread_mutex_destroy(&tui->lock);
	free(tui);
}

void tui_log_text(struct tui *tui, const char *text)
{
	pthread_mutex_lock(&tui->lock);
	wprintw(tui->log_window, text);
	update_panels();
	doupdate();
	pthread_mutex_unlock(&tui->lock);
}

void tui_resize(struct tui *tui)
{
	struct tui_rect layout[tui_windows_COUNT];
	// Clear borders to proper minimim repainting
	//wclear(tui->log_border_window);
	//wclear(tui->settings_border_window);
	tui_calculate_windows_layout(LINES, COLS, tui->main_menu_width, layout);
	// Only main windows show be moved (not 'derwin' based ones which implicitly do it)

	// Resizing isolated windows is easy with 'wresize', and so for moving with 'mvwin'
	// BUT for panelized windows it must be done in a different way, with 'move_panel' and
	//	'replace_panel'
	//		http://tldp.org/HOWTO/NCURSES-Programming-HOWTO/panels.html
	//		http://linux.die.net/man/3/move_panel
	tui_resize_panel(tui->log_border_panel, &tui->log_border_window,
			&layout[tui_windows_log_border], NULL);
	tui_resize_panel(tui->log_panel, &tui->log_window, &layout[tui_windows_log],
			tui->log_border_window);
	scrollok(tui->log_window, TRUE);
	tui_resize_panel(tui->settings_border_panel, &tui->settings_border_window,
			&layout[tui_windows_settings_border], NULL);
	tui_resize_panel(tui->settings_panel, &tui->settings_window, &layout[tui_windows_settings],
			tui->settings_border_window);
	tui_resize_panel(tui->status_bar_panel, &tui->status_bar_window,
			&layout[tui_windows_status_bar], NULL);
	tui_resize_panel(tui->flags_panel, &tui->flags_window, &layout[tui_windows_flags], NULL);
	tui_draw_borders(tui);
	// On resizing, the 'Settings' information is lost because the associated window recreated
	// Print the last content again
	wprintw(tui->settings_window, tui->settings_last_content);
	// For panelized windows use 'update_panels' + 'doupdate' instead of 'wrefresh' or 'refresh'
	update_panels();
	doupdate();
}

// PRECONDITION: 'tui->active_panel->parent != NULL'
void tui_quit(struct tui *tui)
{
	tui->quit = 1;
}

ATTR_INTERN void tui_active_panel_close(struct tui *tui)
{
	struct tui_panel *tui_panel_parent = tui->active_panel->parent;
	assert(tui_panel_parent);
	tui->active_panel->tui_panel_release(tui->active_panel);
	tui->active_panel = tui_panel_parent;
	update_panels();
	doupdate();
}

void tui_refresh(struct tui *tui)
{
	pthread_mutex_lock(&tui->lock);
	// This forces the entire screen to be redrawn
	// It is required if we do something wrong with the UIç
	// For example, if we manage 'ncurses' from different threads it could become corrupted
	// In such a case it's likely the update_panels() + doupdate() to don't do nothing because
	// they think the screen is properly painted
	// For such cases, a 'wrefresh(curscr)' it's good. It redraws the entire screen from scratch
	wrefresh(curscr);
	pthread_mutex_unlock(&tui->lock);
}

void tui_do_menu_loop(struct tui *tui)
{
	int c;
	tui->quit = 0;
	while (!tui->quit)
	{
		// The 'getch' can be configured in timeout mode to, for example, have a chance to
		//	periodically refresh the UI: wtimeout(tui->active_panel->window, 1000)
		c = getch();
		pthread_mutex_lock(&tui->lock);
		switch(c)
		{
		case KEY_F(5):
			wrefresh(curscr);
			break;
		case KEY_RESIZE:
			tui_resize(tui);
			break;
		case ERR:
			break;
		default:
			// NOTE: Some panels (as 'tui_menu') can assume that the 'tui->lock' is locked
			//	and temporally unlock on callback execution to avoid deadlocks
			tui->active_panel->tui_panel_process_key(tui->active_panel, c);
		}
		pthread_mutex_unlock(&tui->lock);
	}
	tui_log_text(tui, "Quitting...\n");
}

void tui_set_event(struct tui *tui, uint32_t id, uint32_t data)
{
	pthread_mutex_lock(&tui->lock);
	// TODO: Refactor for a configurable amount of flags (TX, RX, INT...)
	int x;
	int attr;
	char *text = NULL;
	switch (id)
	{
	case event_id_tx_flag:
		x = 0;
		text = data ? "TX_FLAG" : "       ";
		attr = has_colors() ? COLOR_PAIR(COLOR_ID_YELLOW) : A_BOLD;
		break;
	case event_id_rx_flag:
		x = 8;
		text = data ? "RX_FLAG" : "       ";
		attr = has_colors() ? COLOR_PAIR(COLOR_ID_RED) : A_BOLD;
		break;
	case event_id_ok_flag:
		x = 16;
		if (data)
		{
			text = "   ";
			attr = COLOR_PAIR(COLOR_ID_RED);
		}
		else
		{
			text = "ERR";
			attr = has_colors() ? COLOR_PAIR(COLOR_ID_RED_INVERTED) : A_BOLD;
		}
		break;
	}
	if (text)
	{
		wattron(tui->flags_window, attr);
		// 'mvwprintw' = 'move' + 'wprintw'
		mvwprintw(tui->flags_window, 0, x, text);
		wattroff(tui->flags_window, attr);
		update_panels();
		doupdate();
	}
	pthread_mutex_unlock(&tui->lock);
}

ATTR_INTERN pthread_mutex_t *tui_get_lock(struct tui *tui)
{
	return &tui->lock;
}

ATTR_INTERN ui_callbacks_h tui_get_callbacks_handle(struct tui *tui)
{
	return tui->callbacks_handle;
}

ATTR_INTERN struct tui_panel *tui_get_active_panel(struct tui *tui)
{
	return tui->active_panel;
}

ATTR_INTERN void tui_set_active_panel(struct tui *tui, struct tui_panel *panel)
{
	tui->active_panel = panel;
}

void tui_set_info(struct tui *tui, enum ui_info_enum info_type, const void *info_data)
{
	pthread_mutex_lock(&tui->lock);
	switch (info_type)
	{
	case ui_info_settings_text:
		wclear(tui->settings_window);
		// Alternative way:
		//	waddstr(tui->settings_window, text);
		if (tui->settings_last_content) free(tui->settings_last_content);
		// Keep a copy of the last info set just to reprint it in resizing operations
		tui->settings_last_content = strdup(info_data);
		wprintw(tui->settings_window, info_data);
		update_panels();
		doupdate();
		break;
	case ui_info_status_bar_text:
		mvwprintw(tui->status_bar_window, 0, 0, info_data);
		// 'mvwhline' (or 'hline') may be used too to clear the line
		wclrtoeol(tui->status_bar_window);
		update_panels();
		doupdate();
		break;
	default:
		assert(0);
	}
	pthread_mutex_unlock(&tui->lock);
}

void PLUGIN_API_SET_SINGLETON_PROVIDER(singletons_provider_get_t callback,
		singletons_provider_h handle)
{
	singletons_provider_get = callback;
	singletons_provider_handle = handle;
	// Ask for the required callbacks
	uint32_t version;
	singletons_provider_get(singletons_provider_handle, singleton_id_logger, (void**) &logger_api,
			&logger_handle, &version);
	assert(!logger_api || (version >= 1));
	// TODO: Althought 'logger_api' is ok it crashes if called here
	//	if (logger_api) logger_api->log_line(logger_handle, "This is logging from TUI plugin");
}

ATTR_EXTERN void *PLUGIN_API_LOAD(uint32_t *plugin_api_version, uint32_t *plugin_api_size)
{
	CHECK_INTERFACE_MEMBERS_COUNT(ui_api, 12);
	*plugin_api_version = 1;
	*plugin_api_size = sizeof(struct ui_api);
	struct ui_api *ui_api = calloc(1, *plugin_api_size);
	ui_api->init = tui_init;
	ui_api->create = tui_create;
	ui_api->release = tui_release;
	ui_api->do_menu_loop = tui_do_menu_loop;
	ui_api->refresh = tui_refresh;
	ui_api->log_text = tui_log_text;
	ui_api->set_event = tui_set_event;
	ui_api->active_panel_close = tui_active_panel_close;
	ui_api->quit = tui_quit;
	ui_api->open_menu = tui_open_menu;
	ui_api->open_dialog = tui_open_dialog;
	ui_api->set_info = tui_set_info;
	return ui_api;
}

ATTR_EXTERN void PLUGIN_API_UNLOAD(void *ui_api)
{
	free(ui_api);
}

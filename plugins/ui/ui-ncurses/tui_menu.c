/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include <ctype.h>		// tolower
#include <menu.h>
#include <ncurses.h>
#include <panel.h>
#include <pthread.h>		// pthread_mutex_t
#include "+common/api/+base.h"
#include "common.h"

#include "tui.h"
#include "tui_menu.h"

#define PANEL_TO_MENU(tui_panel_ptr) \
	container_of(tui_panel_ptr, struct tui_menu, tui_panel)

struct tui_menu
{
	struct tui_panel tui_panel;
	pthread_mutex_t *lock;
	void (*on_cancel)(ui_callbacks_h);
	ui_callbacks_h callbacks_handle;
	struct ui_menu_item *ui_items;
	WINDOW *window;
	PANEL *panel;
	MENU *menu;
	ITEM **items;
	uint32_t items_count;
};

ATTR_INTERN struct tui_panel *tui_menu_get_as_panel(struct tui_menu *tui_menu)
{
	return &tui_menu->tui_panel;
}

ATTR_INTERN struct tui_menu *tui_menu_get_from_panel(struct tui_panel *tui_panel)
{
	return PANEL_TO_MENU(tui_panel);
}

static void tui_menu_panel_get_dimensions(struct tui_panel *tui_panel, int *pos_y, int *pos_x,
		int *rows, int *cols)
{
	struct tui_menu *tui_menu = PANEL_TO_MENU(tui_panel);
	tui_window_get_dimensions(tui_menu->window, pos_y, pos_x, rows, cols);
}

static void tui_menu_panel_process_key(struct tui_panel *tui_panel, int c)
{
	struct tui_menu *tui_menu = PANEL_TO_MENU(tui_panel);
	switch (c)
	{
	case KEY_DOWN:
		menu_driver(tui_menu->menu, REQ_DOWN_ITEM);
		update_panels();
		doupdate();
		break;
	case KEY_UP:
		menu_driver(tui_menu->menu, REQ_UP_ITEM);
		update_panels();
		doupdate();
		break;
	case '\n':
	{
		ITEM *cur_menu_item = current_item(tui_menu->menu);
		assert(cur_menu_item);
		struct ui_menu_item *ui_menu_item = &tui_menu->ui_items[item_index(cur_menu_item)];
		// TODO: Improve the locking strategy
		// Release the lock temporarily to avoid deadlocks on callbacks
		pthread_mutex_unlock(tui_menu->lock);
		ui_menu_item->action(tui_menu->callbacks_handle, ui_menu_item->data);
		pthread_mutex_lock(tui_menu->lock);
		// Don't call 'wrefresh(tui_menu->window)'. If so, when opening a 'tui_dialog' the cursor
		//	appears on 'tui_menu->window'
		break;
	}
	case '\x1b':
		// Allow quick closing on submenus
		// TODO: With 'getch' ESC is holded for 1 or 2 seconds I guess waiting for an ESCAPE
		//	sequence
		//	To capture ESC immediately maybe we should use 'kbhit' instead
		if (tui_menu->on_cancel)
			tui_menu->on_cancel(tui_menu->callbacks_handle);
		break;
	default:
	{
		// Look for a keyboard shortcut
		int i;
		for (i = 0;
				(i < tui_menu->items_count)
						&& (tolower(tui_menu->ui_items[i].shortcut) != tolower(c)); i++)
			;
		if (i < tui_menu->items_count)
		{
			struct ui_menu_item *ui_menu_item = &tui_menu->ui_items[i];
			pthread_mutex_unlock(tui_menu->lock);
			ui_menu_item->action(tui_menu->callbacks_handle, ui_menu_item->data);
			pthread_mutex_lock(tui_menu->lock);
		}
		break;
	}
	}
}

ATTR_INTERN void tui_menu_release(struct tui_menu *tui_menu)
{
	unpost_menu(tui_menu->menu);
	free_menu(tui_menu->menu);
	int i;
	for (i = 0; i < tui_menu->items_count; ++i)
		free_item(tui_menu->items[i]);
	free(tui_menu->items);
	del_panel(tui_menu->panel);
	delwin(tui_menu->window);
	free(tui_menu->ui_items);
	free(tui_menu);
	// 'update_panels' + 'doupdate' required in order the overlapped windows to be properly redrawn
	update_panels();
	doupdate();
}

void tui_menu_panel_release(struct tui_panel *tui_panel)
{
	tui_menu_release(PANEL_TO_MENU(tui_panel));
}

// PRECONDITION: 'title' can be NULL
ATTR_INTERN struct tui_menu *tui_menu_create(struct tui_panel *parent,
		const struct ui_menu_item *menu_items, uint32_t menu_items_count,
		const char *title, void (*on_cancel)(ui_callbacks_h), ui_callbacks_h callbacks_handle,
		pthread_mutex_t *lock)
{
	int ystart = 2;
	static const char menu_marker[] = " > ";
	int menu_marker_len = strlen(menu_marker);
	if (title)
		ystart += 2;
	struct tui_menu *tui_menu = (struct tui_menu *) calloc(1, sizeof(struct tui_menu));
	tui_menu->tui_panel.tui_panel_release = tui_menu_panel_release;
	tui_menu->tui_panel.tui_panel_get_dimensions = tui_menu_panel_get_dimensions;
	tui_menu->tui_panel.tui_panel_process_key = tui_menu_panel_process_key;
	tui_menu->tui_panel.parent = parent;
	tui_menu->lock = lock;
	// TODO: Improve cursos management
	// Sets the pos of new menu automatically based on the parent selected position
	int pos_x, pos_y;
	if (parent)
	{
		parent->tui_panel_get_dimensions(parent, &pos_y, &pos_x, NULL, NULL);
		// New menu shifted pos_x+5, pos_y-2 as a reasonable value
		pos_x += menu_marker_len + 5;
		pos_y -= 2;
	}
	else
	{
		pos_x = 0;
		pos_y = 0;
	}
	tui_menu->callbacks_handle = callbacks_handle;
	tui_menu->on_cancel = on_cancel;
	tui_menu->ui_items = (struct ui_menu_item*) calloc(menu_items_count,
			sizeof(struct ui_menu_item));
	memcpy(tui_menu->ui_items, menu_items, sizeof(struct ui_menu_item) * menu_items_count);
	tui_menu->items_count = menu_items_count;
	tui_menu->items = (ITEM **) calloc(tui_menu->items_count + 1, sizeof(ITEM *));
	uint32_t i;
	int max_len = 0;
	for (i = 0; i < menu_items_count; ++i)
	{
		tui_menu->items[i] = new_item(menu_items[i].caption, NULL);
		int len = strlen(menu_items[i].caption);
		if (len > max_len)
			max_len = len;
	}
	int box_width = max_len + menu_marker_len + 3;
	tui_menu->items[menu_items_count] = (ITEM *) NULL;
	tui_menu->menu = new_menu((ITEM **) tui_menu->items);
	tui_menu->window = newwin(menu_items_count + ystart + 2, box_width, pos_y, pos_x);
	// Add panels for proper management of popups when releasing them
	tui_menu->panel = new_panel(tui_menu->window);
	keypad(tui_menu->window, TRUE);
	/* Set main window and sub window */
	set_menu_win(tui_menu->menu, tui_menu->window);
	set_menu_sub(tui_menu->menu,
			derwin(tui_menu->window, menu_items_count, box_width - 2, ystart, 1));
	set_menu_mark(tui_menu->menu, menu_marker);
	/* Print a border around the main window and print a title */
	// 'box' = 'wborder' using 2nd param as the vertical char and 3rd param as the horizontal one
	//	box(tui_menu->window, '|', '-');
	wborder(tui_menu->window, '|', '|', '-', '-', 'o', 'o', 'o', 'o');
	if (title)
	{
		tui_window_print_in_middle(tui_menu->window, 1, 0, box_width, title, COLOR_PAIR(1));
		mvwaddch(tui_menu->window, 2, 0, '|');
		mvwhline(tui_menu->window, 2, 1, '-', box_width - 2);
		mvwaddch(tui_menu->window, 2, box_width - 1, '|');
	}
	post_menu(tui_menu->menu);
	// TODO: Use private counter
	// set_panel_userptr(tui_popup->panel, tui_popup);
	update_panels();
	doupdate();
	return tui_menu;
}

ATTR_INTERN void tui_open_menu(struct tui *tui, const struct ui_menu_item *menu_items,
		uint32_t menu_items_count, const char *title, void (*on_cancel)(ui_callbacks_h))
{
	struct tui_menu *tui_menu = tui_menu_create(tui_get_active_panel(tui), menu_items,
			menu_items_count, title, on_cancel, tui_get_callbacks_handle(tui), tui_get_lock(tui));
	tui_set_active_panel(tui, tui_menu_get_as_panel(tui_menu));
}

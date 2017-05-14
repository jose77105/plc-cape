/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include <ctype.h>		// isspace
#include <menu.h>
#include <ncurses.h>
#include <panel.h>
#include "+common/api/+base.h"
#include "common.h"

#include "tui.h"
#include "tui_list.h"

#define PANEL_TO_LIST(tui_panel_ptr) \
	container_of(tui_panel_ptr, struct tui_list, tui_panel)

#define DEFAULT_FIELD_LENGTH 10

struct tui_list
{
	struct tui_panel tui_panel;
	list_callback_h callbacks_handle;
	void (*on_cancel)(list_callback_h);
	void (*on_ok)(list_callback_h, int selected_index);
	MENU *menu;
	ITEM **items;
	uint32_t items_count;
	WINDOW *window;
	PANEL *panel;
};

struct tui_panel *tui_list_get_as_panel(struct tui_list *tui_list)
{
	return &tui_list->tui_panel;
}

struct tui_list *tui_list_get_from_panel(struct tui_panel *tui_panel)
{
	return PANEL_TO_LIST(tui_panel);
}

static void tui_list_panel_get_dimensions(struct tui_panel *tui_panel, int *pos_y, int *pos_x,
		int *rows, int *cols)
{
	struct tui_list *tui_list = PANEL_TO_LIST(tui_panel);
	tui_window_get_dimensions(tui_list->window, pos_y, pos_x, rows, cols);
}

static void tui_list_panel_process_key(struct tui_panel *tui_panel, int c)
{
	struct tui_list *tui_list = PANEL_TO_LIST(tui_panel);
	switch (c)
	{
	case KEY_DOWN:
		menu_driver(tui_list->menu, REQ_DOWN_ITEM);
		break;
	case KEY_UP:
		menu_driver(tui_list->menu, REQ_UP_ITEM);
		break;
	case '\x1b':
		if (tui_list->on_cancel)
		{
			tui_list->on_cancel(tui_list->callbacks_handle);
			return;
		}
		break;
	case '\t':
	case '\n':
		// TODO: Process item selected
		// tui_list_validate(tui_list);
		if (tui_list->on_ok)
		{
			ITEM *item = current_item(tui_list->menu);
			tui_list->on_ok(tui_list->callbacks_handle, item_index(item));
			return;
		}
		break;
	default:
		break;
	}
	update_panels();
	doupdate();
}

void tui_list_release(struct tui_list *tui_list)
{
	unpost_menu(tui_list->menu);
	free_menu(tui_list->menu);
	int i;
	for (i = 0; i < tui_list->items_count; ++i)
		free_item(tui_list->items[i]);
	free(tui_list->items);
	del_panel(tui_list->panel);
	delwin(tui_list->window);
	free(tui_list);
	update_panels();
	doupdate();
}

void tui_list_panel_release(struct tui_panel *tui_panel)
{
	tui_list_release(PANEL_TO_LIST(tui_panel));
}

struct tui_list *tui_list_create(struct tui_panel *parent, const char **text_items,
		uint32_t text_items_count, const char *title, void (*on_cancel)(list_callback_h),
		void (*on_ok)(list_callback_h, int selected_index), list_callback_h callbacks_handle)
{
	struct tui_list *tui_list = (struct tui_list*) calloc(1, sizeof(struct tui_list));
	tui_list->tui_panel.tui_panel_release = tui_list_panel_release;
	tui_list->tui_panel.tui_panel_get_dimensions = tui_list_panel_get_dimensions;
	tui_list->tui_panel.tui_panel_process_key = tui_list_panel_process_key;
	tui_list->tui_panel.parent = parent;
	tui_list->callbacks_handle = callbacks_handle;
	tui_list->on_cancel = on_cancel;
	tui_list->on_ok = on_ok;
	tui_list->items_count = text_items_count;
	tui_list->items = (ITEM **) calloc(text_items_count + 1, sizeof(ITEM *));
	uint32_t i;
	int max_len = 0;
	for (i = 0; i < text_items_count; ++i)
	{
		tui_list->items[i] = new_item(text_items[i], NULL);
		int len = strlen(text_items[i]);
		if (len > max_len)
			max_len = len;
	}
	tui_list->items[text_items_count] = (ITEM *) NULL;
	// Create the menu and post it
	tui_list->menu = new_menu((ITEM **) tui_list->items);
	// Calculate the area required for the menu
	int pos_y, pos_x;
	if (parent)
	{
		parent->tui_panel_get_dimensions(parent, &pos_y, &pos_x, NULL, NULL);
		// New menu shifted pos_x+5, pos_y-2 as a reasonable value
		pos_x += 5;
		pos_y -= 2;
	}
	else
	{
		pos_x = 0;
		pos_y = 0;
	}
	// Create the window to be associated with the menu
	tui_list->window = newwin(text_items_count + 4, max_len + 4, pos_y, pos_x);
	tui_list->panel = new_panel(tui_list->window);
	keypad(tui_list->window, TRUE);
	// Set main window and sub window
	set_menu_win(tui_list->menu, tui_list->window);
	set_menu_sub(tui_list->menu, derwin(tui_list->window, text_items_count, max_len, 2, 2));
	// Remove the default mark (a dash '-')
	set_menu_mark(tui_list->menu, NULL);
	// Print a border around the main window and print a title
	wborder(tui_list->window, '|', '|', '-', '-', 'o', 'o', 'o', 'o');
	tui_window_print_in_middle(tui_list->window, 0, 0, max_len + 3, title, COLOR_PAIR(1));
	post_menu(tui_list->menu);
	update_panels();
	doupdate();
	// TODO: Better control of cursor_visibility
	curs_set(0);
	return tui_list;
}

ATTR_INTERN void tui_open_list(struct tui *tui, const char **text_items, uint32_t text_items_count,
		const char *title, void (*on_cancel)(list_callback_h),
		void (*on_ok)(list_callback_h, int selected_index), list_callback_h handle)
{
	struct tui_list *tui_list = tui_list_create(tui_get_active_panel(tui), text_items,
			text_items_count, title, on_cancel, on_ok, handle);
	tui_set_active_panel(tui, tui_list_get_as_panel(tui_list));
}

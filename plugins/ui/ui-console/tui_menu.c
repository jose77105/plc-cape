/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include <ctype.h>		// tolower
#include "+common/api/+base.h"
#include "tui_menu.h"
#define TUI_PANEL_HANDLE_EXPLICIT_DEF
typedef struct tui_menu *tui_panel_h;
#include "tui_panel.h"

struct tui_menu
{
	struct tui_panel panel;
	struct ui_menu_item *menu_items;
	uint32_t menu_items_count;
};

void tui_menu_release(struct tui_menu *menu)
{
	uint32_t n;
	for (n = 0; n < menu->menu_items_count; n++)
		free(menu->menu_items[n].caption);
	free(menu->menu_items);
	if (menu->panel.title) free(menu->panel.title);
	free(menu);
}

void tui_menu_display(struct tui_menu *menu)
{
	static const char menu_separator[] = "*==================";
	uint32_t i;
	// 'puts' writes a trailing newline
	puts(menu_separator);
	if (menu->panel.title)
	{
		printf("** %s\n", menu->panel.title);
		puts(menu_separator);
	}
	for (i = 0; i < menu->menu_items_count; i++)
		printf("| %s\n", menu->menu_items[i].caption);
	puts(menu_separator);
}

void tui_menu_process_key(struct tui_menu *menu, int c)
{
	if (c == KEY_CANCEL)
	{
		if (menu->panel.on_cancel)
			menu->panel.on_cancel(menu->panel.callbacks_handle);
	}
	else
	{
		// Look for a keyboard shortcut
		int i;
		for (i = 0;
				(i < menu->menu_items_count)
						&& (tolower(menu->menu_items[i].shortcut) != tolower(c));
								i++)
			;
		if (i < menu->menu_items_count)
		{
			struct ui_menu_item *ui_menu_item = &menu->menu_items[i];
			ui_menu_item->action(menu->panel.callbacks_handle, ui_menu_item->data);
		}
		else
		{
			// An unknown menu selection refreshes the menu
			if (c != '\n')
				tui_menu_display(menu);
			// TODO: In order to comfortably debug 'ui-console' within the Eclipse console
			//	ignore the extra '\n' required for an input option to be accepted
		}
	}
}

ATTR_INTERN struct tui_panel *tui_menu_create(struct tui_panel *parent,
		const struct ui_menu_item *menu_items, uint32_t menu_items_count, const char *title,
		void (*on_cancel)(ui_callbacks_h), ui_callbacks_h callbacks_handle)
{
	struct tui_menu *menu = malloc(sizeof(*menu));
	menu->panel.parent = parent;
	menu->panel.title = (title) ? strdup(title) : NULL;
	menu->panel.callbacks_handle = callbacks_handle;
	menu->panel.on_cancel = on_cancel;
	menu->panel.release = tui_menu_release;
	menu->panel.display = tui_menu_display;
	menu->panel.process_key = tui_menu_process_key;
	menu->menu_items_count = menu_items_count;
	menu->menu_items = malloc(menu_items_count*sizeof(*menu->menu_items));
	uint32_t n;
	for (n = 0; n < menu_items_count; n++)
	{
		menu->menu_items[n] = menu_items[n];
		menu->menu_items[n].caption = strdup(menu_items[n].caption);
	}
	return &menu->panel;
}

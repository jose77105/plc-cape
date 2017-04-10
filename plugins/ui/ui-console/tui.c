/**
 * @file
 * @brief	**Main** file
 *	
 * @see		@ref plugin-ui-console
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
 *	Copyright (C) 2016 Jose Maria Ortega
 * 
 * @endcond
 */

#include <ctype.h>		// tolower
#include "+common/api/+base.h"
#include "+common/api/logger.h"
#include "libraries/libplc-tools/api/terminal_io.h"
// Declare the custom type used as handle. Doing it like this avoids the 'void*' hard-casting
#define PLUGINS_API_HANDLE_EXPLICIT_DEF
typedef struct tui *ui_api_h;
#include "plugins/ui/api/ui.h"

// PRECONDITION: 'struct xui' must always be the first element in the struct to apply kind of 
//	"polymorfism"
struct tui_menu
{
	char *title;
	void (*on_cancel)(ui_callbacks_h);
	struct ui_menu_item *menu_items;
	uint32_t menu_items_count;
	struct tui_menu *parent_menu;
};

struct tui
{
	struct plc_terminal_io *plc_terminal_io;
	ui_callbacks_h callbacks_handle;
	int quit;
	struct tui_menu *active_menu;
};

// Connection with the 'singletons_provider'
singletons_provider_get_t singletons_provider_get = NULL;
singletons_provider_h singletons_provider_handle = NULL;

// Logger served by the 'singletons_provider'
struct plc_logger_api *logger_api;
void *logger_handle;

void log_line(const char *msg)
{
	if (logger_api)
		logger_api->log_line(logger_handle, msg);
	else
		fprintf(stderr, msg);
}

int tui_init(int argc, char *argv[])
{
	return 1;
}

void tui_display_active_menu(struct tui *tui)
{
	static const char menu_separator[] = "*==================";
	uint32_t i;
	// 'puts' writes a trailing newline
	puts(menu_separator);
	if (tui->active_menu->title)
	{
		printf("** %s\n", tui->active_menu->title);
		puts(menu_separator);
	}
	for (i = 0; i < tui->active_menu->menu_items_count; i++)
		printf("| %s\n", tui->active_menu->menu_items[i].caption);
	puts(menu_separator);
}

void tui_open_menu(struct tui *tui, const struct ui_menu_item *menu_items,
		uint32_t menu_items_count, const char *title, void (*on_cancel)(ui_callbacks_h))
{
	struct tui_menu *parent_menu = tui->active_menu;
	tui->active_menu = malloc(sizeof(*tui->active_menu));
	tui->active_menu->parent_menu = parent_menu;
	tui->active_menu->on_cancel = on_cancel;
	tui->active_menu->title = (title) ? strdup(title) : NULL;
	tui->active_menu->menu_items_count = menu_items_count;
	tui->active_menu->menu_items = malloc(menu_items_count*sizeof(*tui->active_menu->menu_items));
	uint32_t n;
	for (n = 0; n < menu_items_count; n++)
	{
		tui->active_menu->menu_items[n] = menu_items[n];
		tui->active_menu->menu_items[n].caption = strdup(menu_items[n].caption);
	}
	tui_display_active_menu(tui);
}

void tui_open_dialog(struct tui *tui, const struct ui_dialog_item *dialog_items,
		uint32_t dialog_items_count, const char *title, void (*on_cancel)(ui_callbacks_h),
		void (*on_ok)(ui_callbacks_h))
{
}

struct tui *tui_create(const char *title, const struct ui_menu_item *main_menu_items,
		uint32_t main_menu_items_count, ui_callbacks_h callbacks_handle)
{
	struct tui *tui = (struct tui*) calloc(1, sizeof(struct tui));
	tui->plc_terminal_io = plc_terminal_io_create();;
	tui->callbacks_handle = callbacks_handle;
	tui_open_menu(tui, main_menu_items, main_menu_items_count,  "Main Menu", NULL);
	return tui;
}

void tui_release_menu(struct tui_menu *menu)
{
	uint32_t n;
	for (n = 0; n < menu->menu_items_count; n++)
		free(menu->menu_items[n].caption);
	free(menu->menu_items);
	if (menu->title) free(menu->title);
	free(menu);
}

void tui_release(struct tui *tui)
{
	tui_release_menu(tui->active_menu);
	plc_terminal_io_release(tui->plc_terminal_io);
	free(tui);
}

void tui_log_text(struct tui *tui, const char *text)
{
	// static uint32_t log_counter = 0;
	// fprintf(stdout, "  [%03d] %s\n", log_counter++, text);
	// '.' just as an indicator the subsequent text is logging text
	putchar('.');
	fputs(text, stdout);
}

// PRECONDITION: 'tui->active_panel->parent != NULL'
void tui_quit(struct tui *tui)
{
	tui->quit = 1;
}

void tui_do_menu_loop(struct tui *tui)
{
	tui->quit = 0;
	while (!tui->quit)
	{
		// TODO: Use libplc-tools/terminal
		// int c = getchar();
		int c = plc_terminal_io_getchar(tui->plc_terminal_io);
		switch (c)
		{
		case '\x1b':
			if (tui->active_menu->parent_menu == NULL)
				tui->quit = 1;
			else if (tui->active_menu->on_cancel)
				tui->active_menu->on_cancel(tui->callbacks_handle);
			break;
		default:
		{
			// Look for a keyboard shortcut
			int i;
			for (i = 0;
					(i < tui->active_menu->menu_items_count)
							&& (tolower(tui->active_menu->menu_items[i].shortcut) != tolower(c));
									i++)
				;
			if (i < tui->active_menu->menu_items_count)
			{
				struct ui_menu_item *ui_menu_item = &tui->active_menu->menu_items[i];
				ui_menu_item->action(tui->callbacks_handle, ui_menu_item->data);
			}
			break;
		}
		}
	}
	tui_log_text(tui, "Quitting...\n");
}

void tui_refresh(struct tui *tui)
{
	// TODO: Ignore it? It usually draw the last buffers received. Put in a callback
}

void tui_set_event(struct tui *tui, uint32_t id, uint32_t data)
{
	// TODO: Receives the event_id_tx_flag, event_id_rx_flag... Ignore or notify? How?
	switch (id)
	{
	case event_id_tx_flag:
		printf("> TX flag: %u\n", data);
		break;
	case event_id_rx_flag:
		printf("> RX flag: %u\n", data);
		break;
	case event_id_ok_flag:
		printf("> OK flag: %d\n", data);
		break;
	}
}

void tui_active_panel_close(struct tui *tui)
{
	struct tui_menu *parent_menu = tui->active_menu->parent_menu;
	tui_release_menu(tui->active_menu);
	tui->active_menu = parent_menu;
	tui_display_active_menu(tui);
}

void tui_set_info(struct tui *tui, enum ui_info_enum info_type, const void *info_data)
{
	// TODO: Change this system. UI should ask for the settings and maybe for the status bar
	//	(callback). Only receive a notification information has changed
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

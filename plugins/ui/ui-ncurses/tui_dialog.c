/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include <ctype.h>		// isspace
#include <form.h>
#include <ncurses.h>
#include <panel.h>
#include "+common/api/+base.h"
#include "common.h"
#include "tui.h"
#include "tui_dialog.h"
#include "tui_list.h"

#define PANEL_TO_DIALOG(tui_panel_ptr) \
	container_of(tui_panel_ptr, struct tui_dialog, tui_panel)

#define DEFAULT_FIELD_LENGTH 10

struct tui_dialog
{
	struct tui_panel tui_panel;
	struct tui *tui;
	ui_callbacks_h callbacks_handle;
	void (*on_cancel)(ui_callbacks_h);
	void (*on_ok)(ui_callbacks_h);
	struct ui_dialog_item *tui_items;
	uint32_t tui_items_count;
	FIELD **fields;
	uint32_t fields_count;
	FORM *form;
	WINDOW *window;
	PANEL *panel;
};

struct tui_panel *tui_dialog_get_as_panel(struct tui_dialog *tui_dialog)
{
	return &tui_dialog->tui_panel;
}

struct tui_dialog *tui_dialog_get_from_panel(struct tui_panel *tui_panel)
{
	return PANEL_TO_DIALOG(tui_panel);
}

#include "+common/api/logger.h"
			extern struct plc_logger_api *logger_api;
			extern void *logger_handle;

static void tui_dialog_validate(struct tui_dialog *tui_dialog)
{
	form_driver(tui_dialog->form, REQ_VALIDATION);
	int i;
	for (i = 0; i < tui_dialog->tui_items_count; ++i)
	{
		struct ui_dialog_item *item = &tui_dialog->tui_items[i];
		const char *dialog_text = field_buffer(tui_dialog->fields[2 * i + 1], 0);
		// TODO: Optimize trailing space management
		// Trim trailing space
		const char *dialog_text_end = dialog_text + strlen(dialog_text) - 1;
		while ((dialog_text_end > dialog_text) && isspace(*dialog_text_end))
			dialog_text_end--;
		char *dialog_text_trim = strndup(dialog_text, dialog_text_end + 1 - dialog_text);
		switch (item->data_type)
		{
		case data_type_i32:
			sscanf(dialog_text, "%i", item->data_ptr.i32);
			break;
		case data_type_u16:
			sscanf(dialog_text, "%hu", item->data_ptr.u16);
			break;
		case data_type_u32:
			sscanf(dialog_text, "%u", item->data_ptr.u32);
			break;
		case data_type_str:
		{
			if (*item->data_ptr.str)
				free(*item->data_ptr.str);
			*item->data_ptr.str = strdup(dialog_text_trim);
			break;
		}
		case data_type_list:
		{
			struct ui_dialog_list *list = &item->data_ptr.list;
			int n;
			for (n = 0; n < list->items_count; n++)
				if (strcmp(dialog_text_trim, list->items[n]) == 0)
				{
					*(uint32_t*) list->index = n;
					break;
				}
			break;
		}
		case data_type_callbacks:
		{
			struct callbacks *callbacks = &item->data_ptr.callbacks;
			callbacks->set_value(callbacks->data, dialog_text_trim);
			break;
		}
		}
		free(dialog_text_trim);
	}
}

struct ui_dialog_list_callback
{
	struct tui *tui;
	FIELD *field;
	struct ui_dialog_list *list;
};

static void tui_dialog_on_list_cancel(list_callback_h list_callback_handle)
{
	struct ui_dialog_list_callback *list_callback = list_callback_handle;
	tui_active_panel_close(list_callback->tui);
	free(list_callback);
	// The dialog list disables the cursor -> re-enable
	curs_set(1);
}

static void tui_dialog_on_list_ok(list_callback_h list_callback_handle, int selected_index)
{
	struct ui_dialog_list_callback *list_callback = list_callback_handle;
	set_field_buffer(list_callback->field, 0, list_callback->list->items[selected_index]);
	tui_active_panel_close(list_callback->tui);
	free(list_callback);
	// The dialog list disables the cursor -> re-enable
	curs_set(1);
}

static void tui_dialog_panel_process_key(struct tui_panel *tui_panel, int c)
{
	struct tui_dialog *tui_dialog = PANEL_TO_DIALOG(tui_panel);
	int field_type_list = 0;
	struct ui_dialog_item *ui_dialog_item = NULL;
	FIELD *cur_field = current_field(tui_dialog->form);
	if (cur_field != NULL)
	{
		ui_dialog_item = field_userptr(cur_field);
		if (ui_dialog_item != NULL)
			field_type_list = (ui_dialog_item->data_type == data_type_list);
	}
	switch (c)
	{
	case KEY_DOWN:
		form_driver(tui_dialog->form, REQ_NEXT_FIELD);
		form_driver(tui_dialog->form, REQ_END_LINE);
		break;
	case KEY_UP:
		form_driver(tui_dialog->form, REQ_PREV_FIELD);
		form_driver(tui_dialog->form, REQ_END_LINE);
		break;
	case KEY_LEFT:
		form_driver(tui_dialog->form, REQ_PREV_CHAR);
		break;
	case KEY_RIGHT:
		if (field_type_list)
		{
			struct ui_dialog_list_callback *list_callback = malloc(
					sizeof(struct ui_dialog_list_callback));
			list_callback->tui = tui_dialog->tui;
			list_callback->field = cur_field;
			list_callback->list = &ui_dialog_item->data_ptr.list;
			tui_open_list(tui_dialog->tui, ui_dialog_item->data_ptr.list.items,
					ui_dialog_item->data_ptr.list.items_count, ui_dialog_item->caption,
					tui_dialog_on_list_cancel, tui_dialog_on_list_ok, list_callback);
		}
		else
		{
			form_driver(tui_dialog->form, REQ_NEXT_CHAR);
		}
		break;
		// Delete the char before cursor
	case KEY_BACKSPACE:
	case 127:
		form_driver(tui_dialog->form, REQ_DEL_PREV);
		break;
		// Delete the char under the cursor
	case KEY_DC:
		form_driver(tui_dialog->form, REQ_DEL_CHAR);
		break;
	case '\x1b':
		if (tui_dialog->on_cancel)
			tui_dialog->on_cancel(tui_dialog->callbacks_handle);
		break;
	case '\t':
	case '\n':
		tui_dialog_validate(tui_dialog);
		if (tui_dialog->on_ok)
			tui_dialog->on_ok(tui_dialog->callbacks_handle);
		break;
	default:
		// If this is a normal character, it gets printed
		form_driver(tui_dialog->form, c);
		break;
	}
	update_panels();
	doupdate();
}

static void tui_dialog_panel_get_dimensions(struct tui_panel *tui_panel, int *pos_y, int *pos_x,
		int *rows, int *cols)
{
	struct tui_dialog *tui_dialog = PANEL_TO_DIALOG(tui_panel);
	tui_window_get_dimensions(tui_dialog->window, pos_y, pos_x, rows, cols);
}

void tui_dialog_release(struct tui_dialog *tui_dialog)
{
	unpost_form(tui_dialog->form);
	free_form(tui_dialog->form);
	int i;
	for (i = 0; i < tui_dialog->fields_count; ++i)
		free_field(tui_dialog->fields[i]);
	del_panel(tui_dialog->panel);
	delwin(tui_dialog->window);
	free(tui_dialog->tui_items);
	free(tui_dialog);
	update_panels();
	doupdate();
	curs_set(0);
}

void tui_dialog_panel_release(struct tui_panel *tui_panel)
{
	tui_dialog_release(PANEL_TO_DIALOG(tui_panel));
}

struct tui_dialog *tui_dialog_create(struct tui_panel *parent,
		const struct ui_dialog_item *dialog_items, uint32_t dialog_items_count, const char *title,
		void (*on_cancel)(ui_callbacks_h), void (*on_ok)(ui_callbacks_h),
		ui_callbacks_h callbacks_handle)
{
	struct tui_dialog *tui_dialog = (struct tui_dialog*) calloc(1, sizeof(struct tui_dialog));
	tui_dialog->tui_panel.tui_panel_release = tui_dialog_panel_release;
	tui_dialog->tui_panel.tui_panel_get_dimensions = tui_dialog_panel_get_dimensions;
	tui_dialog->tui_panel.tui_panel_proces_key = tui_dialog_panel_process_key;
	tui_dialog->tui_panel.parent = parent;
	tui_dialog->callbacks_handle = callbacks_handle;
	tui_dialog->on_cancel = on_cancel;
	tui_dialog->on_ok = on_ok;
	tui_dialog->tui_items = (struct ui_dialog_item*) calloc(dialog_items_count,
			sizeof(struct ui_dialog_item));
	tui_dialog->tui_items_count = dialog_items_count;
	memcpy(tui_dialog->tui_items, dialog_items, sizeof(struct ui_dialog_item) * dialog_items_count);
	tui_dialog->fields_count = dialog_items_count * 2;
	tui_dialog->fields = (FIELD **) calloc(tui_dialog->fields_count + 1, sizeof(FIELD *));
	uint32_t i;
	int max_len = 0;
	int startx = 2;
	int starty = 0;
	FIELD **cur_field = tui_dialog->fields;
	for (i = 0; i < dialog_items_count; ++i)
	{
		struct ui_dialog_item *item = &tui_dialog->tui_items[i];
		int len = strlen(dialog_items[i].caption);
		if (len > max_len)
			max_len = len;
		*cur_field = new_field(1, len, starty + i, startx, 0, 0);
		set_field_buffer(*cur_field, 0, dialog_items[i].caption);
		// Static label field
		field_opts_off(*cur_field, O_ACTIVE);
		cur_field++;
		// Calculate 'data_len'
		int data_len = DEFAULT_FIELD_LENGTH;
		switch (item->data_type)
		{
		case data_type_list:
		{
			data_len = 0;
			struct ui_dialog_list *list = &item->data_ptr.list;
			int n;
			for (n = 0; n < list->items_count; n++)
			{
				int len = strlen(list->items[n]);
				if (len > data_len)
					data_len = len;
			}
			break;
		}
		case data_type_str:
			if (*item->data_ptr.str)
			{
				data_len = strlen(*item->data_ptr.str);
				if (data_len < item->max_len)
					data_len = item->max_len;
			}
			break;
		default:
			if (item->max_len) data_len = item->max_len;
		}
		*cur_field = new_field(1, data_len, starty + i, startx + len + 2, 0, 0);
		// Print a line for the option
		set_field_back(*cur_field, A_UNDERLINE);
		// Don't go to next field
		field_opts_off(*cur_field, O_AUTOSKIP);
		set_field_userptr(*cur_field, item);
		// Fill data
		char field_text_int[11];
		int use_field_text_int = 1;
		switch (item->data_type)
		{
		case data_type_i32:
			// TODO: Restrict to valid input. Check because not all the options seems to work
			// set_field_type(*cur_field, TYPE_INTEGER, 1, 0, 2000);
			sprintf(field_text_int, "%i", *item->data_ptr.i32);
			break;
		case data_type_u16:
			sprintf(field_text_int, "%hu", *item->data_ptr.u16);
			break;
		case data_type_u32:
			sprintf(field_text_int, "%u", *item->data_ptr.u32);
			break;
		case data_type_str:
			if (*item->data_ptr.str)
				set_field_buffer(*cur_field, 0, *item->data_ptr.str);
			use_field_text_int = 0;
			break;
		case data_type_list:
		{
			struct ui_dialog_list *list = &item->data_ptr.list;
			set_field_buffer(*cur_field, 0, list->items[*list->index]);
			use_field_text_int = 0;
		}
			break;
		case data_type_callbacks:
		{
			struct callbacks *callbacks = &item->data_ptr.callbacks;
			set_field_buffer(*cur_field, 0, callbacks->get_value(callbacks->data));
			use_field_text_int = 0;
			break;
		}
		}
		if (use_field_text_int)
			set_field_buffer(*cur_field, 0, field_text_int);
		cur_field++;
	}
	*cur_field = NULL;
	// Create the form and post it
	tui_dialog->form = new_form(tui_dialog->fields);
	// Calculate the area required for the form
	int form_rows, form_cols;
	scale_form(tui_dialog->form, &form_rows, &form_cols);
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
	// Create the window to be associated with the form
	tui_dialog->window = newwin(form_rows + 4, form_cols + 4, pos_y, pos_x);
	tui_dialog->panel = new_panel(tui_dialog->window);
	keypad(tui_dialog->window, TRUE);
	// Set main window and sub window
	set_form_win(tui_dialog->form, tui_dialog->window);
	set_form_sub(tui_dialog->form, derwin(tui_dialog->window, form_rows, form_cols, 2, 2));
	// Print a border around the main window and print a title
	// box(tui_dialog->window, 0, 0);
	wborder(tui_dialog->window, '|', '|', '-', '-', 'o', 'o', 'o', 'o');
	tui_window_print_in_middle(tui_dialog->window, 0, 0, form_cols + 4, title, COLOR_PAIR(1));
	post_form(tui_dialog->form);
	update_panels();
	doupdate();
	curs_set(1);
	return tui_dialog;
}

ATTR_INTERN void tui_open_dialog(struct tui *tui, const struct ui_dialog_item *dialog_items,
		uint32_t dialog_items_count, const char *title, void (*on_cancel)(ui_callbacks_h),
		void (*on_ok)(ui_callbacks_h))
{
	struct tui_dialog *tui_dialog = tui_dialog_create(tui_get_active_panel(tui), dialog_items,
			dialog_items_count, title, on_cancel, on_ok, tui_get_callbacks_handle(tui));
	// TODO: Refactor. 'tui' shouldn't be required to be passed to all the objects
	tui_dialog->tui = tui;
	tui_set_active_panel(tui, tui_dialog_get_as_panel(tui_dialog));
}

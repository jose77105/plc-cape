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
#include "tui_dialog.h"
#define TUI_PANEL_HANDLE_EXPLICIT_DEF
typedef struct tui_dialog *tui_panel_h;
#include "tui_panel.h"

struct tui_dialog
{
	struct tui_panel panel;
	void (*on_ok)(ui_callbacks_h);
	struct ui_dialog_item *dialog_items;
	uint32_t dialog_items_count;
};

void tui_dialog_release(struct tui_dialog *dialog)
{
	uint32_t n;
	for (n = 0; n < dialog->dialog_items_count; n++)
		free(dialog->dialog_items[n].caption);
	free(dialog->dialog_items);
	if (dialog->panel.title) free(dialog->panel.title);
	free(dialog);
}

void tui_dialog_display_item(struct ui_dialog_item *item)
{
	printf("%s ", item->caption);
	switch (item->data_type)
	{
	case data_type_i32:
		printf("%i", *item->data_ptr.i32);
		break;
	case data_type_u16:
		printf("%hu", *item->data_ptr.u16);
		break;
	case data_type_u32:
		printf("%u", *item->data_ptr.u32);
		break;
	case data_type_float:
		printf("%.2f", *item->data_ptr.f);
		break;
	case data_type_str:
		if (*item->data_ptr.str)
			fputs(*item->data_ptr.str, stdout);
		break;
	case data_type_list:
	{
		struct ui_dialog_list *list = &item->data_ptr.list;
		if (*list->index < list->items_count)
			fputs(list->items[*list->index], stdout);
		else
			printf("%u", *list->index);
		break;
	}
	case data_type_callbacks:
	{
		struct callbacks *callbacks = &item->data_ptr.callbacks;
		fputs(callbacks->get_value(callbacks->data), stdout);
		break;
	}
	}
}

void tui_dialog_change_item(struct ui_dialog_item *item)
{
	tui_dialog_display_item(item);
	printf(" -> ");
	if (item->data_type == data_type_list)
	{
		int i;
		puts("");
		for (i = 0; i < item->data_ptr.list.items_count; i++)
			printf("%d. %s\n", i, item->data_ptr.list.items[i]);
	}
	switch (item->data_type)
	{
	case data_type_i32:
		scanf("%i", item->data_ptr.i32);
		// 'getchar' required to process the '\n'
		getchar();
		break;
	case data_type_u16:
		scanf("%hu", item->data_ptr.u16);
		getchar();
		break;
	case data_type_u32:
		scanf("%u", item->data_ptr.u32);
		getchar();
		break;
	case data_type_float:
		scanf("%f", item->data_ptr.f);
		getchar();
		break;
	case data_type_str:
		if (*item->data_ptr.str)
		{
			uint16_t data_len = strlen(*item->data_ptr.str);
			if (data_len < item->max_len)
				data_len = item->max_len;
			char *input = malloc(data_len);
			scanf("%s", input);
			getchar();
			free(*item->data_ptr.str);
			*item->data_ptr.str = strdup(input);
		}
		break;
	case data_type_list:
		scanf("%u", item->data_ptr.list.index);
		getchar();
		break;
	case data_type_callbacks:
		// TODO: To be done
		break;
	}
}

void tui_dialog_display(struct tui_dialog *dialog)
{
	static const char dialog_separator[] = "*==================";
	uint32_t i;
	// 'puts' writes a trailing newline
	puts(dialog_separator);
	if (dialog->panel.title)
	{
		printf("** %s\n", dialog->panel.title);
		puts(dialog_separator);
	}
	for (i = 0; i < dialog->dialog_items_count; i++)
	{
		printf("| %c.", i+'A');
		tui_dialog_display_item(&dialog->dialog_items[i]);
		puts("");
	}
	puts(dialog_separator);
}

void tui_dialog_process_key(struct tui_dialog *dialog, int c)
{
	// TODO: Implement a proper behavior for KEY_CANCEL instead of doing the same as in KEY_OK
	if ((c == KEY_OK) || (c == KEY_CANCEL))
	{
		if (dialog->on_ok)
			dialog->on_ok(dialog->panel.callbacks_handle);
	}
	else
	{
		// Look for a keyboard shortcut
		int i;
		for (i = 0; (i < dialog->dialog_items_count) && (('a'+i) != tolower(c)); i++);
		if (i < dialog->dialog_items_count)
			tui_dialog_change_item(&dialog->dialog_items[i]);
		tui_dialog_display(dialog);
	}
}

ATTR_INTERN struct tui_panel *tui_dialog_create(struct tui_panel *parent,
		const struct ui_dialog_item *dialog_items, uint32_t dialog_items_count, const char *title,
		void (*on_cancel)(ui_callbacks_h), void (*on_ok)(ui_callbacks_h),
		ui_callbacks_h callbacks_handle)
{
	struct tui_dialog *dialog = malloc(sizeof(*dialog));
	dialog->panel.parent = parent;
	dialog->panel.title = (title) ? strdup(title) : NULL;
	dialog->panel.callbacks_handle = callbacks_handle;
	dialog->panel.on_cancel = on_cancel;
	dialog->on_ok = on_ok;
	dialog->panel.release = tui_dialog_release;
	dialog->panel.display = tui_dialog_display;
	dialog->panel.process_key = tui_dialog_process_key;
	dialog->dialog_items_count = dialog_items_count;
	dialog->dialog_items = malloc(dialog_items_count*sizeof(*dialog->dialog_items));
	uint32_t n;
	for (n = 0; n < dialog_items_count; n++)
	{
		dialog->dialog_items[n] = dialog_items[n];
		dialog->dialog_items[n].caption = strdup(dialog_items[n].caption);
	}
	return &dialog->panel;
}

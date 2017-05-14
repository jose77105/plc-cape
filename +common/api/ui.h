/**
 * @file
 * @brief	UI public symbols within the \c plc-cape project
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef COMMON_UI_H
#define COMMON_UI_H

#ifdef __cplusplus
extern "C" {
#endif

// TODO: Add Doxygen info

enum event_id
{
	event_id_tx_flag = 1, event_id_rx_flag, event_id_ok_flag,
};

enum buffer_category_enum
{
	buffer_category_tx = 1, buffer_category_rx,
};

enum ui_data_type
{
	data_type_i32 = 0,
	data_type_u16,
	data_type_u32,
	data_type_float,
	data_type_str,
	data_type_list,
	data_type_callbacks,
};

union ui_data_ptr
{
	int *i32;
	uint16_t *u16;
	uint32_t *u32;
	float *f;
	char **str;
	struct ui_dialog_list
	{
		uint32_t *index;
		const char **items;
		uint32_t items_count;
	} list;
	struct callbacks
	{
		void *data;
		const char *(*get_value)(void *data);
		void (*set_value)(void *data, const char *new_value);
	} callbacks;
};

struct settings;
struct ui;
// typedef void *ui_callbacks_h;
typedef struct ui *ui_callbacks_h;

struct ui_menu_item
{
	char *caption;
	char shortcut;
	void (*action)(ui_callbacks_h, uint32_t data);
	uint32_t data;
};

struct ui_dialog_item
{
	char *caption;
	enum ui_data_type data_type;
	union ui_data_ptr data_ptr;
	uint16_t max_len;
};

#ifdef __cplusplus
}
#endif

#endif /* COMMON_UI_H */

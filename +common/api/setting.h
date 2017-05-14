/**
 * @file
 * @brief	Common management of \c settings within the \c plc-cape project
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef COMMON_SETTING_H
#define COMMON_SETTING_H

#ifdef __cplusplus
extern "C" {
#endif

// TODO: Add Doxygen info

enum plc_setting_type
{
	plc_setting_empty = 0,
	plc_setting_pointer,
	plc_setting_bool,		// 'u32' = 0 means 'false'; 'true' otherwise
	plc_setting_u8,
	plc_setting_i8,
	plc_setting_u16,
	plc_setting_i16,
	plc_setting_u32,
	plc_setting_i32,
	plc_setting_u64,
	plc_setting_i64,
	plc_setting_float,
	plc_setting_double,
	plc_setting_string,
	plc_setting_enum,		// index stored in 'u32'
	plc_setting_callback,	// data stored in 'p'
};

union plc_setting_data
{
	void *p;
	uint8_t b;
	uint8_t u8;
	int8_t i8;
	uint16_t u16;
	int16_t i16;
	uint32_t u32;
	int32_t i32;
	uint64_t u64;
	int64_t i64;
	float f;
	double d;
	char *s;
};

enum plc_setting_extra_data_type
{
	plc_setting_extra_data_none = 0,
	plc_setting_extra_data_numeric_range,
	plc_setting_extra_data_enum_captions,
};

struct plc_setting_numeric_range
{
	const char *min;
	const char *max;
};

struct plc_setting_enum_captions
{
	uint32_t captions_count;
	const char **captions;
};

union plc_setting_extra_data_content
{
	struct plc_setting_numeric_range numeric_range;
	struct plc_setting_enum_captions enum_captions;
};

struct plc_setting_extra_data
{
	enum plc_setting_extra_data_type type;
	union plc_setting_extra_data_content content;
};

// IMPORTANT: This structure is widely used on plugins in hard-coded vector definitions
//	For backwards compatibility don't modify it
struct plc_setting_definition
{
	char *identifier;
	enum plc_setting_type type;
	char *caption;
	union plc_setting_data default_value;
	uint32_t extra_data_count;
	struct plc_setting_extra_data *extra_data;
	uint32_t user_data;
};

// TODO: Move 'plc_setting' and 'plc_setting_ref' locally to 'plc-cape-lab' project?
struct plc_setting
{
	char *identifier;
	enum plc_setting_type type;
	union plc_setting_data data;
};

struct plc_setting_linked
{
	const struct plc_setting_definition *definition;
	union plc_setting_data data;
};

/*
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
struct enum_type {
	uint32_t index;
	const char *(*index_to_text)(uint32_t index);
	uint32_t (*text_to_index)(const char *text);
} e;
*/

#ifdef __cplusplus
}
#endif

#endif /* COMMON_SETTING_H */

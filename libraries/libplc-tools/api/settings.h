/**
 * @file
 * @brief	Generic settings management
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef LIBPLC_TOOLS_SETTINGS_H
#define LIBPLC_TOOLS_SETTINGS_H

// TODO: Add Doxygen info
#include "+common/api/setting.h"

#ifdef __cplusplus
extern "C" {
#endif

struct setting_list_item
{
	struct setting_list_item *next;
	struct plc_setting setting;
};

struct setting_linked_list_item
{
	struct setting_linked_list_item *next;
	struct plc_setting_linked setting;
};

// TODO: Refactor & encapsulate. Make struct members private
struct plc_setting_named_list
{
	uint32_t definitions_count;
	const struct plc_setting_definition *definitions;
	struct setting_linked_list_item *linked_list;
};

void plc_setting_data_release(enum plc_setting_type type, union plc_setting_data *data);
void plc_setting_clear(struct plc_setting *setting);
char *plc_setting_data_to_text(enum plc_setting_type type, const union plc_setting_data *data);
char *plc_setting_linked_data_to_text(const struct plc_setting_linked *setting);
int plc_setting_text_to_data(const char *text, enum plc_setting_type type,
		union plc_setting_data *data);
void plc_setting_copy(struct plc_setting *dst, const struct plc_setting *src);
void plc_setting_convert(struct plc_setting *setting, enum plc_setting_type dst_type);
void plc_setting_copy_convert(struct plc_setting *dst, const struct plc_setting *src,
		enum plc_setting_type dst_type);
void plc_setting_copy_convert_data(union plc_setting_data *dst_data,
		const struct plc_setting_definition *dst_definition, const struct plc_setting *src);
void plc_setting_copy_convert_linked(struct plc_setting_linked *dst, const struct plc_setting *src);
const struct plc_setting_definition *plc_setting_find_definition(
		const struct plc_setting_definition *setting_definitions,
		uint32_t setting_definitions_count, const char *identifier);
void plc_setting_get_enum_captions(const struct plc_setting_definition *setting_definition,
		const char ***captions, uint32_t *captions_count);
uint32_t plc_setting_get_settings_linked_count(const struct setting_linked_list_item *setting_linked_list);
void plc_setting_clear_settings_linked(struct plc_setting_named_list *settings);
void plc_setting_normalize(const struct setting_list_item *setting_list,
		struct plc_setting_named_list *plugin_settings);
void plc_setting_set_setting(struct setting_list_item **item_dst,
		const struct plc_setting *setting_src);
void plc_setting_set_settings(uint32_t settings_count, const struct plc_setting settings_src[],
		struct setting_list_item **item_dst);
void plc_setting_add_settings(uint32_t settings_count, const struct plc_setting settings_src[],
		struct setting_list_item **item_dst);
void plc_setting_clear_settings(struct setting_list_item **setting_list);
void plc_setting_load_default_settings(const struct plc_setting_named_list *settings,
		struct setting_list_item **item_dst);
void plc_setting_load_all_default_settings(struct plc_setting_named_list *settings,
		struct setting_list_item **setting_list);

#ifdef __cplusplus
}
#endif

#endif /* LIBPLC_TOOLS_SETTINGS_H */

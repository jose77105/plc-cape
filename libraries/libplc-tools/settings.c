/**
 * @file
 * 
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#define _GNU_SOURCE				// 'asprintf'
#include "+common/api/+base.h"
#include "api/settings.h"

ATTR_EXTERN void plc_setting_data_release(enum plc_setting_type type, union plc_setting_data *data)
{
	switch (type)
	{
	case plc_setting_pointer:
	case plc_setting_callback:
		free(data->p);
		break;
	case plc_setting_string:
		free(data->s);
		break;
	case plc_setting_bool:
	case plc_setting_u8:
	case plc_setting_i8:
	case plc_setting_u16:
	case plc_setting_i16:
	case plc_setting_u32:
	case plc_setting_i32:
	case plc_setting_u64:
	case plc_setting_i64:
	case plc_setting_float:
	case plc_setting_double:
	case plc_setting_enum:
		break;
	default:
		assert(0);
	}
}

ATTR_EXTERN void plc_setting_clear(struct plc_setting *setting)
{
	plc_setting_data_release(setting->type, &setting->data);
	setting->type = plc_setting_empty;
	if (setting->identifier)
	{
		free(setting->identifier);
		setting->identifier = NULL;
	}
}

// PRECONDITION: 'setting_definition' corresponding to a 'plc_setting_enum'
ATTR_EXTERN void plc_setting_get_enum_captions(
		const struct plc_setting_definition *setting_definition, const char ***captions,
		uint32_t *captions_count)
{
	uint32_t n;
	assert(setting_definition->type == plc_setting_enum);
	struct plc_setting_extra_data *extra_data = setting_definition->extra_data;
	for (n = setting_definition->extra_data_count; n > 0; n--, extra_data++)
	{
		if (extra_data->type == plc_setting_extra_data_enum_captions)
		{
			*captions = extra_data->content.enum_captions.captions;
			*captions_count = extra_data->content.enum_captions.captions_count;
			break;
		}
	}
	assert(n > 0);
}

// POSTCONDITION: Release the resulting pointer with 'free' when no longer required
ATTR_EXTERN char *plc_setting_data_to_text(enum plc_setting_type type,
		const union plc_setting_data *data)
{
	char *text;
	// For 'printf' format specifiers see:
	//	https://en.wikipedia.org/wiki/Printf_format_string
	switch (type)
	{
	case plc_setting_bool:
		asprintf(&text, "%s", (data->u32 == 0) ? "0" : "1");
		break;
	case plc_setting_u8:
		asprintf(&text, "%hhu", data->u8);
		break;
	case plc_setting_i8:
		asprintf(&text, "%hhd", data->i8);
		break;
	case plc_setting_u16:
		asprintf(&text, "%hu", data->u16);
		break;
	case plc_setting_i16:
		asprintf(&text, "%hd", data->i16);
		break;
	case plc_setting_u32:
		asprintf(&text, "%u", data->u32);
		break;
	case plc_setting_i32:
		asprintf(&text, "%d", data->i32);
		break;
	case plc_setting_u64:
		asprintf(&text, "%llu", data->u64);
		break;
	case plc_setting_i64:
		asprintf(&text, "%lld", data->i64);
		break;
	case plc_setting_float:
		asprintf(&text, "%f", data->f);
		break;
	case plc_setting_double:
		// TODO: Revise float vs double
		asprintf(&text, "%lf", data->d);
		break;
	case plc_setting_enum:
		// TODO: Display the enum text
		asprintf(&text, "%u", data->u32);
		break;
	case plc_setting_string:
		text = strdup(data->s);
		break;
	case plc_setting_pointer:
	case plc_setting_callback:
	default:
		text = strdup("N/A");
		assert(0);
	}
	return text;
}

ATTR_EXTERN char *plc_setting_linked_data_to_text(const struct plc_setting_linked *setting)
{
	if (setting->definition->type == plc_setting_enum)
	{
		uint32_t captions_count;
		const char **captions;
		plc_setting_get_enum_captions(setting->definition, &captions, &captions_count);
		assert(setting->data.u32 < captions_count);
		return strdup(captions[setting->data.u32]);
	}
	else
	{
		return plc_setting_data_to_text(setting->definition->type, &setting->data);
	}
}

ATTR_EXTERN int plc_setting_text_to_data(const char *text, enum plc_setting_type type,
		union plc_setting_data *data)
{
	// For 'printf' format specifiers see:
	//	https://en.wikipedia.org/wiki/Printf_format_string
	switch (type)
	{
	case plc_setting_bool:
		data->u32 = (strcmp(text, "0") == 0) ? 0 : 1;
		break;
	case plc_setting_u8:
		sscanf(text, "%hhu", &data->u8);
		break;
	case plc_setting_i8:
		sscanf(text, "%hhd", &data->i8);
		break;
	case plc_setting_u16:
		sscanf(text, "%hu", &data->u16);
		break;
	case plc_setting_i16:
		sscanf(text, "%hd", &data->i16);
		break;
	case plc_setting_u32:
		sscanf(text, "%u", &data->u32);
		break;
	case plc_setting_i32:
		sscanf(text, "%d", &data->i32);
		break;
	case plc_setting_u64:
		sscanf(text, "%llu", &data->u64);
		break;
	case plc_setting_i64:
		sscanf(text, "%lld", &data->i64);
		break;
	case plc_setting_float:
		sscanf(text, "%f", &data->f);
		break;
	case plc_setting_double:
		// TODO: Revise float vs double
		sscanf(text, "%lf", &data->d);
		break;
	case plc_setting_enum:
		// TODO: Display the enum text
		sscanf(text, "%u", &data->u32);
		break;
	case plc_setting_string:
		data->s = strdup(text);
		break;
	case plc_setting_pointer:
	case plc_setting_callback:
	default:
		assert(0);
		return -1;
	}
	return 0;
}

ATTR_EXTERN int plc_setting_linked_text_to_data(const char *text,
		const struct plc_setting_definition *definition, union plc_setting_data *data)
{
	if (definition->type == plc_setting_enum)
	{
		uint32_t n, captions_count;
		const char **captions;
		plc_setting_get_enum_captions(definition, &captions, &captions_count);
		for (n = 0; n < captions_count; n++)
			if (strcmp(text, captions[n]) == 0)
			{
				data->u32 = n;
				return 0;
			}
		TRACE(0, "Setting error: '%s' is not an accepted enum value for '%s'\n", text,
				definition->identifier);
		assert(0);
		return -1;
	}
	else
	{
		return plc_setting_text_to_data(text, definition->type, data);
	}
}

// POSTCONDITION: Only the strictly involved bytes of the union are modified.
//	For example, if 'type == plc_setting_u8' -> *(uint_8t*)dst = value
ATTR_EXTERN void plc_setting_copy_data(union plc_setting_data *dst,
		const union plc_setting_data *src, enum plc_setting_type type)
{
	switch (type)
	{
	case plc_setting_bool:
	case plc_setting_u32:
	case plc_setting_i32:
	case plc_setting_enum:
		dst->u32 = src->u32;
		break;
	case plc_setting_u8:
	case plc_setting_i8:
		dst->u8 = src->u8;
		break;
	case plc_setting_u16:
	case plc_setting_i16:
		dst->u16 = src->u16;
		break;
	case plc_setting_u64:
	case plc_setting_i64:
		dst->u64 = src->u64;
		break;
	case plc_setting_float:
		dst->f = src->f;
		break;
	case plc_setting_double:
		dst->d = src->d;
		break;
	case plc_setting_string:
		dst->s = strdup(src->s);
		break;
	case plc_setting_pointer:
	case plc_setting_callback:
	default:
		assert(0);
	}
}

ATTR_EXTERN void plc_setting_copy(struct plc_setting *dst, const struct plc_setting *src)
{
	dst->identifier = src->identifier ? strdup(src->identifier) : NULL;
	dst->type = src->type;
	plc_setting_copy_data(&dst->data, &src->data, src->type);
}

ATTR_EXTERN void plc_setting_convert(struct plc_setting *setting, enum plc_setting_type dst_type)
{
	if (setting->type != dst_type)
	{
		char *text;
		if (setting->type == plc_setting_string)
		{
			text = setting->data.s;
		}
		else
		{
			text = plc_setting_data_to_text(setting->type, &setting->data);
			plc_setting_data_release(setting->type, &setting->data);
		}
		plc_setting_text_to_data(text, dst_type, &setting->data);
		free(text);
		setting->type = dst_type;
	}
}

ATTR_EXTERN void plc_setting_copy_convert(struct plc_setting *dst, const struct plc_setting *src,
		enum plc_setting_type dst_type)
{
	if (src->type == dst_type)
	{
		plc_setting_copy(dst, src);
	}
	else
	{
		dst->identifier = src->identifier ? strdup(src->identifier) : NULL;
		dst->type = dst_type;
		if (src->type == plc_setting_string)
		{
			plc_setting_text_to_data(src->data.s, dst_type, &dst->data);
		}
		else
		{
			char *text = plc_setting_data_to_text(src->type, &src->data);
			plc_setting_text_to_data(text, dst_type, &dst->data);
			free(text);
		}
	}
}

ATTR_EXTERN void plc_setting_copy_convert_data(union plc_setting_data *dst_data,
		const struct plc_setting_definition *dst_definition, const struct plc_setting *src)
{
	enum plc_setting_type dst_type = dst_definition->type;
	if (dst_type == src->type)
	{
		plc_setting_copy_data(dst_data, &src->data, dst_type);
	}
	else
	{
		if (src->type == plc_setting_string)
		{
			plc_setting_linked_text_to_data(src->data.s, dst_definition, dst_data);
		}
		else
		{
			char *text = plc_setting_data_to_text(src->type, &src->data);
			plc_setting_linked_text_to_data(text, dst_definition, dst_data);
			free(text);
		}
	}
}

// PRECONDITION: 'dst->setting.definition' initialized
ATTR_EXTERN void plc_setting_copy_convert_linked(struct plc_setting_linked *dst,
		const struct plc_setting *src)
{
	plc_setting_copy_convert_data(&dst->data, dst->definition, src);
}

ATTR_EXTERN const struct plc_setting_definition *plc_setting_find_definition(
		const struct plc_setting_definition *setting_definitions,
		uint32_t setting_definitions_count, const char *identifier)
{
	uint32_t i;
	const struct plc_setting_definition *setting_definition = setting_definitions;
	for (i = setting_definitions_count; i > 0; i--, setting_definition++)
		if (strcmp(identifier, setting_definition->identifier) == 0)
			break;
	return (i > 0) ? setting_definition : NULL;
}

// TODO: Keep the item counter within the list
ATTR_EXTERN uint32_t plc_setting_get_settings_linked_count(
		const struct setting_linked_list_item *setting_linked_list)
{
	uint32_t count = 0;
	const struct setting_linked_list_item *item = setting_linked_list;
	for (; item != NULL; item = item->next)
		count++;
	return count;
}

ATTR_EXTERN void plc_setting_clear_settings_linked(struct plc_setting_named_list *settings)
{
	struct setting_linked_list_item *linked_item = settings->linked_list;
	while (linked_item != NULL)
	{
		plc_setting_data_release(linked_item->setting.definition->type, &linked_item->setting.data);
		struct setting_linked_list_item *item_prev = linked_item;
		linked_item = linked_item->next;
		free(item_prev);
	}
	settings->linked_list = NULL;
}

ATTR_EXTERN void plc_setting_normalize(const struct setting_list_item *setting_list,
		struct plc_setting_named_list *plugin_settings)
{
	plc_setting_clear_settings_linked(plugin_settings);
	struct setting_linked_list_item **setting_linked_item = &plugin_settings->linked_list;
	while (setting_list != NULL)
	{
		*setting_linked_item = malloc(sizeof(struct setting_linked_list_item));
		(*setting_linked_item)->setting.definition = plc_setting_find_definition(
				plugin_settings->definitions, plugin_settings->definitions_count,
				setting_list->setting.identifier);
		if ((*setting_linked_item)->setting.definition != NULL)
		{
			plc_setting_copy_convert_linked(&(*setting_linked_item)->setting,
					&setting_list->setting);
		}
		else
		{
			TRACE(0, "Setting error: the setting '%s' is not accepted by the corresponding plugin\n",
					setting_list->setting.identifier);
			assert(0);
		}
		setting_linked_item = &(*setting_linked_item)->next;
		setting_list = setting_list->next;
	}
	*setting_linked_item = NULL;
}

ATTR_EXTERN void plc_setting_set_setting(struct setting_list_item **item_dst,
		const struct plc_setting *setting_src)
{
	while ((*item_dst) != NULL)
	{
		if (strcmp(setting_src->identifier, (*item_dst)->setting.identifier) == 0)
			break;
		item_dst = &(*item_dst)->next;
	}
	if (*item_dst != NULL)
	{
		plc_setting_clear(&(*item_dst)->setting);
	}
	else
	{
		*item_dst = malloc(sizeof(struct setting_list_item));
		(*item_dst)->next = NULL;
	}
	plc_setting_copy(&(*item_dst)->setting, setting_src);
}

// This function checks for any setting if it already exists a setting with same identifier
ATTR_EXTERN void plc_setting_set_settings(uint32_t settings_count,
		const struct plc_setting settings_src[], struct setting_list_item **item_dst)
{
	const struct plc_setting *setting_src = settings_src;
	uint32_t n;
	// TODO: Encapsulate all the process to improve it using hashes or dictionaries
	for (n = settings_count; n > 0; n--, setting_src++)
		plc_setting_set_setting(item_dst, setting_src);
}

// This functions adds settings to the end (accepting duplicated identifiers)
ATTR_EXTERN void plc_setting_add_settings(uint32_t settings_count,
		const struct plc_setting settings_src[], struct setting_list_item **item_dst)
{
	const struct plc_setting *setting_src = settings_src;
	// Add items to the end
	// TODO: For better efficiency keep a pointer to the end of the list
	while ((*item_dst) != NULL)
		item_dst = &(*item_dst)->next;
	uint32_t n;
	for (n = settings_count; n > 0; n--, setting_src++)
	{
		*item_dst = malloc(sizeof(struct setting_list_item));
		plc_setting_copy(&(*item_dst)->setting, setting_src);
		item_dst = &(*item_dst)->next;
	}
	*item_dst = NULL;
}

ATTR_EXTERN void plc_setting_clear_settings(struct setting_list_item **setting_list)
{
	struct setting_list_item *item = *setting_list;
	while (item != NULL)
	{
		plc_setting_clear(&item->setting);
		struct setting_list_item *item_prev = item;
		item = item->next;
		free(item_prev);
	}
	*setting_list = NULL;
}

ATTR_EXTERN void plc_setting_load_default_settings(const struct plc_setting_named_list *settings,
		struct setting_list_item **item_dst)
{
	uint32_t n;
	const struct plc_setting_definition *setting_definition = settings->definitions;
	for (n = settings->definitions_count; n > 0; n--, setting_definition++)
	{
		*item_dst = malloc(sizeof(struct setting_list_item));
		struct plc_setting setting;
		setting.identifier = setting_definition->identifier;
		setting.type = setting_definition->type;
		setting.data = setting_definition->default_value;
		plc_setting_copy(&(*item_dst)->setting, &setting);
		item_dst = &(*item_dst)->next;
	}
	*item_dst = NULL;
}

ATTR_EXTERN void plc_setting_load_all_default_settings(
		struct plc_setting_named_list *settings, struct setting_list_item **setting_list)
{
	plc_setting_clear_settings(setting_list);
	plc_setting_load_default_settings(settings, setting_list);
	plc_setting_normalize(*setting_list, settings);
}

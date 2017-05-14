/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include "+common/api/+base.h"
#include "+common/api/setting.h"
#include "common.h"
#include "libraries/libplc-tools/api/settings.h"	// plc_setting_named_list
#include "plugins.h"
#include "encoder.h"

struct encoder
{
	struct plugin *plugin;
	struct encoder_api *api;
	encoder_api_h api_handle;
	char *name;
	struct plc_setting_named_list settings;
	int invalid_configuration;
	// Data source copy mode
	uint16_t *data_source;
	uint16_t *data_source_cur;
	uint16_t *data_source_end;
	uint32_t data_source_len;
	int continuous_mode;
};

struct encoder *encoder_create(const char *path)
{
	struct encoder *encoder = calloc(1, sizeof(struct encoder));
	uint32_t api_version, api_size;
	encoder->plugin = load_plugin(path, (void**) &encoder->api, &api_version, &api_size);
	assert((api_version >= 1) && (api_size >= sizeof(struct encoder_api)));
	encoder->api_handle = encoder->api->create();
	encoder->name = strdup(strrchr(path, '/') + 1);
	encoder->settings.definitions = encoder->api->get_accepted_settings(encoder->api_handle,
			&encoder->settings.definitions_count);
	return encoder;
}

void encoder_release(struct encoder *encoder)
{
	plc_setting_clear_settings_linked(&encoder->settings);
	free(encoder->name);
	encoder->api->release(encoder->api_handle);
	unload_plugin(encoder->plugin);
	free(encoder);
}

void encoder_set_configuration(struct encoder *encoder, const struct setting_list_item *setting_list)
{
	plc_setting_normalize(setting_list, &encoder->settings);
}

void encoder_set_default_configuration(struct encoder *encoder)
{
	struct setting_list_item *setting_list = NULL;
	// TODO: Improve or remove artificial use of intermediate 'setting_list'
	plc_setting_load_all_default_settings(&encoder->settings, &setting_list);
	plc_setting_clear_settings(&setting_list);
}

void encoder_apply_configuration(struct encoder *encoder, float sampling_rate_sps,
		uint32_t bit_width_us)
{
	uint32_t n;
	int ret = encoder->api->begin_settings(encoder->api_handle);
	if (ret == 0)
	{
		// First check for default global settings
		const struct plc_setting_definition *setting_definition = encoder->settings.definitions;
		for (n = encoder->settings.definitions_count; (n > 0) && (ret == 0); n--, setting_definition++)
		{
			union plc_setting_data setting_data;
			if (strcmp(setting_definition->identifier, "sampling_rate_sps") == 0)
			{
				assert(setting_definition->type == plc_setting_float);
				setting_data.f = sampling_rate_sps;
			}
			else if (strcmp(setting_definition->identifier, "bit_width_us") == 0)
			{
				assert(setting_definition->type == plc_setting_u32);
				setting_data.u32 = bit_width_us;
			}
			else
			{
				// Continue with the loop without specific processing
				continue;
			}
			ret = encoder->api->set_setting(encoder->api_handle, setting_definition->identifier,
					setting_data);
		}
		if (ret == 0)
		{
			// Set the custom settings
			const struct setting_linked_list_item *setting_item = encoder->settings.linked_list;
			while (setting_item != NULL)
			{
				int ret = encoder->api->set_setting(encoder->api_handle,
						setting_item->setting.definition->identifier,
						setting_item->setting.data);
				if (ret != 0)
					break;
				setting_item = setting_item->next;
			}
		}
		// TODO: Improve error control
		if (ret != 0)
			log_line(get_last_error());
		int ret_end = encoder->api->end_settings(encoder->api_handle);
		if (ret == 0)
			ret = ret_end;
	}
	if (ret != 0)
		log_line(get_last_error());
	encoder->invalid_configuration = (ret != 0);
}

void encoder_reset(struct encoder *encoder)
{
	encoder->api->reset(encoder->api_handle);
}

void encoder_prepare_next_samples(struct encoder *encoder, uint16_t *buffer, uint32_t buffer_count)
{
	if (encoder->data_source_cur)
	{
		if (encoder->data_source_cur + buffer_count <= encoder->data_source_end)
		{
			memcpy(buffer, encoder->data_source_cur, buffer_count * sizeof(*buffer));
			encoder->data_source_cur += buffer_count;
		}
		else
		{
			uint32_t items_chunk_1 = encoder->data_source_end - encoder->data_source_cur;
			uint32_t items_chunk_2 = buffer_count - items_chunk_1;
			memcpy(buffer, encoder->data_source_cur, items_chunk_1 * sizeof(*buffer));
			if (encoder->continuous_mode)
			{
				assert(encoder->data_source + items_chunk_2 <= encoder->data_source_end);
				memcpy(buffer + items_chunk_1, encoder->data_source,
						items_chunk_2 * sizeof(*buffer));
				encoder->data_source_cur = encoder->data_source + items_chunk_2;
			}
			else
			{
				memset(buffer + items_chunk_1, 0, items_chunk_2 * sizeof(*buffer));
				encoder->data_source_cur = encoder->data_source_end;
			}
		}
	}
	else
	{
		encoder->api->prepare_next_samples(encoder->api_handle, buffer, buffer_count);
	}
}

// Remark: 'data_source == NULL' stops the copy mode
void encoder_set_copy_mode(struct encoder *encoder, uint16_t *data_source, uint32_t data_source_len,
		int continuous_mode)
{
	encoder->data_source = data_source;
	encoder->data_source_cur = data_source;
	encoder->data_source_end = data_source + data_source_len;
	encoder->data_source_len = data_source_len;
	encoder->continuous_mode = continuous_mode;
}

int encoder_is_ready(struct encoder *encoder)
{
	return !encoder->invalid_configuration;
}

struct setting_linked_list_item *encoder_get_settings(struct encoder *encoder)
{
	return encoder->settings.linked_list;
}

const char *encoder_get_name(struct encoder *encoder)
{
	return encoder->name;
}

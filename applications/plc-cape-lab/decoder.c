/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include "+common/api/+base.h"
#include "common.h"
#include "decoder.h"
#include "libraries/libplc-tools/api/settings.h"	// plc_setting_named_list
#include "plugins.h"
#include "plugins/decoder/api/decoder.h"

struct decoder
{
	struct plugin *plugin;
	struct decoder_api *api;
	decoder_api_h api_handle;
	char *name;
	struct plc_setting_named_list settings;
	int invalid_configuration;
};

struct decoder *decoder_create(const char *path)
{
	struct decoder *decoder = calloc(1, sizeof(struct decoder));
	uint32_t api_version, api_size;
	decoder->plugin = load_plugin(path, (void**) &decoder->api, &api_version, &api_size);
	assert((api_version >= 1) && (api_size >= sizeof(struct decoder_api)));
	decoder->api_handle = decoder->api->create();
	decoder->name = strdup(strrchr(path, '/') + 1);
	decoder->settings.definitions = decoder->api->get_accepted_settings(decoder->api_handle,
			&decoder->settings.definitions_count);
	return decoder;
}

void decoder_release(struct decoder *decoder)
{
	plc_setting_clear_settings_linked(&decoder->settings);
	free(decoder->name);
	decoder->api->release(decoder->api_handle);
	unload_plugin(decoder->plugin);
	free(decoder);
}

void decoder_set_configuration(struct decoder *decoder, const struct setting_list_item *setting_list)
{
	plc_setting_normalize(setting_list, &decoder->settings);
}

void decoder_set_default_configuration(struct decoder *decoder)
{
	struct setting_list_item *setting_list = NULL;
	// TODO: Improve or remove artificial use of intermediate 'setting_list'
	plc_setting_load_all_default_settings(&decoder->settings, &setting_list);
	plc_setting_clear_settings(&setting_list);
}

// TODO: Better strategy for data_offset and data_hi_threshold_detection
void decoder_apply_configuration(struct decoder *decoder,
		sample_rx_t data_offset, sample_rx_t data_hi_threshold, float capturing_rate_sps,
		uint32_t bit_width_us, uint32_t samples_to_file)
{
	uint32_t n;
	int ret = decoder->api->begin_settings(decoder->api_handle);
	if (ret == 0)
	{
		// First check for default global settings
		const struct plc_setting_definition *setting_definition = decoder->settings.definitions;
		for (n = decoder->settings.definitions_count; (n > 0) && (ret == 0); n--, setting_definition++)
		{
			union plc_setting_data setting_data;
			if (strcmp(setting_definition->identifier, "sampling_rate_sps") == 0)
			{
				assert(setting_definition->type == plc_setting_float);
				setting_data.f = capturing_rate_sps;
			}
			else if (strcmp(setting_definition->identifier, "data_offset") == 0)
			{
				assert(setting_definition->type == plc_setting_u16);
				setting_data.u16 = data_offset;
			}
			else if (strcmp(setting_definition->identifier, "data_hi_threshold") == 0)
			{
				assert(setting_definition->type == plc_setting_u16);
				setting_data.u16 = data_hi_threshold;
			}
			else if (strcmp(setting_definition->identifier, "bit_width_us") == 0)
			{
				assert(setting_definition->type == plc_setting_u32);
				setting_data.u32 = bit_width_us;
			}
			else if (strcmp(setting_definition->identifier, "samples_to_file") == 0)
			{
				assert(setting_definition->type == plc_setting_u32);
				setting_data.u32 = samples_to_file;
			}
			else
			{
				// Continue with the loop without specific processing
				continue;
			}
			ret = decoder->api->set_setting(decoder->api_handle, setting_definition->identifier,
					setting_data);
		}
		if (ret == 0)
		{
			// Set the custom settings
			const struct setting_linked_list_item *setting_item = decoder->settings.linked_list;
			while (setting_item != NULL)
			{
				int ret = decoder->api->set_setting(decoder->api_handle,
						setting_item->setting.definition->identifier, setting_item->setting.data);
				if (ret != 0)
					break;
				setting_item = setting_item->next;
			}
		}
		// TODO: Improve error control
		if (ret != 0)
			log_line(get_last_error());
		int ret_end = decoder->api->end_settings(decoder->api_handle);
		if (ret == 0)
			ret = ret_end;
	}
	if (ret != 0)
		log_line(get_last_error());
	decoder->invalid_configuration = (ret != 0);
}

inline void decoder_initialize_demodulator(struct decoder *decoder, uint32_t chunk_samples)
{
	decoder->api->initialize(decoder->api_handle, chunk_samples);
}

inline void decoder_terminate_demodulator(struct decoder *decoder)
{
	decoder->api->terminate(decoder->api_handle);
}

inline uint32_t decoder_parse_next_samples(struct decoder *decoder, const sample_rx_t *buffer_in,
		uint8_t *buffer_data_out, uint32_t buffer_data_out_count)
{
	return decoder->api->parse_next_samples(decoder->api_handle, buffer_in, buffer_data_out,
			buffer_data_out_count);
}

int decoder_is_ready(struct decoder *decoder)
{
	return !decoder->invalid_configuration;
}

struct setting_linked_list_item *decoder_get_settings(struct decoder *decoder)
{
	return decoder->settings.linked_list;
}

const char *decoder_get_name(struct decoder *decoder)
{
	return decoder->name;
}

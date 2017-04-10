/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include "+common/api/+base.h"
#include "common.h"
#include "plugins.h"
#include "encoder.h"

const char *stream_type_enum_text[stream_COUNT] =
{ "ramp", "triangular", "constant", "freq_max", "freq_max_div2", "freq_sweep", "freq_sweep_preload",
		"file", "bit_padding_per_cycle", "am_modulation", "freq_sinus", "ook_pattern", "ook_hi", };

struct encoder
{
	struct plugin *plugin;
	struct encoder_api *api;
	encoder_api_h api_handle;
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
	return encoder;
}

void encoder_release(struct encoder *encoder)
{
	encoder->api->release(encoder->api_handle);
	unload_plugin(encoder->plugin);
	free(encoder);
}

static void set_setting(const struct encoder *encoder, enum encoder_setting_enum setting,
		const void *data)
{
	int ret = encoder->api->set_setting(encoder->api_handle, setting, data);
	if (ret != 0)
		log_and_exit(get_last_error());
}

void encoder_configure_WAVE(struct encoder *encoder, enum stream_type_enum stream_type,
		int samples_offset, int samples_range, float samples_freq, const char *filename,
		uint32_t bit_width_us)
{
	int ret = encoder->api->begin_settings(encoder->api_handle);
	if (ret < 0)
		log_and_exit(get_last_error());
	set_setting(encoder, encoder_setting_freq_dac_sps, &freq_dac_sps);
	set_setting(encoder, encoder_setting_stream_type, (void*) stream_type);
	switch (stream_type)
	{
	case stream_file:
		set_setting(encoder, encoder_setting_filename, filename);
		break;
	default:
		set_setting(encoder, encoder_setting_offset, (void*) samples_offset);
		set_setting(encoder, encoder_setting_range, (void*) samples_range);
		set_setting(encoder, encoder_setting_freq, &samples_freq);
		if ((stream_type == stream_ook_pattern) || (stream_type == stream_ook_hi))
			set_setting(encoder, encoder_setting_bit_width_us, (void*) bit_width_us);
		break;
	}
	ret = encoder->api->end_settings(encoder->api_handle);
	if (ret < 0)
		log_and_exit(get_last_error());
}

void encoder_configure_OOK(struct encoder *encoder, int samples_offset, int samples_range,
		float samples_freq, uint32_t bit_width_us, const char *message_to_send)
{
	int ret = encoder->api->begin_settings(encoder->api_handle);
	if (ret < 0)
		log_and_exit(get_last_error());
	set_setting(encoder, encoder_setting_freq_dac_sps, &freq_dac_sps);
	set_setting(encoder, encoder_setting_offset, (void*) samples_offset);
	set_setting(encoder, encoder_setting_range, (void*) samples_range);
	set_setting(encoder, encoder_setting_freq, &samples_freq);
	set_setting(encoder, encoder_setting_bit_width_us, (void*) bit_width_us);
	set_setting(encoder, encoder_setting_message, message_to_send);
	ret = encoder->api->end_settings(encoder->api_handle);
	if (ret < 0)
		log_and_exit(get_last_error());
}

void encoder_configure_PWM(struct encoder *encoder, int samples_offset, int samples_range,
		float samples_freq, uint32_t bit_width_us, const char *message_to_send)
{
	int ret = encoder->api->begin_settings(encoder->api_handle);
	if (ret < 0)
		log_and_exit(get_last_error());
	set_setting(encoder, encoder_setting_freq_dac_sps, &freq_dac_sps);
	set_setting(encoder, encoder_setting_offset, (void*) samples_offset);
	set_setting(encoder, encoder_setting_range, (void*) samples_range);
	set_setting(encoder, encoder_setting_freq, &samples_freq);
	set_setting(encoder, encoder_setting_bit_width_us, (void*) bit_width_us);
	set_setting(encoder, encoder_setting_message, message_to_send);
	ret = encoder->api->end_settings(encoder->api_handle);
	if (ret < 0)
		log_and_exit(get_last_error());
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

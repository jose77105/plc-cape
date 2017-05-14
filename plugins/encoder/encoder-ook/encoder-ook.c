/**
 * @file
 * @brief	**Main** file
 *
 * @see		@ref plugin-encoder-ook
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
 *	Copyright (C) 2016-2017 Jose Maria Ortega
 * 
 * @endcond
 */

#include <err.h>			// warnx
#include <math.h>			// sin
#include "+common/api/+base.h"
#include "+common/api/error.h"
#include "+common/api/logger.h"
#include "+common/api/setting.h"
#define PLUGINS_API_HANDLE_EXPLICIT_DEF
typedef struct encoder *encoder_api_h;
#include "plugins/encoder/api/encoder.h"

// Bits in idle status between character transmission to allow proper synchronization
#define GUARD_BITS 32

struct encoder_settings
{
	uint32_t offset;
	uint32_t range;
	float freq;
	uint32_t bit_width_us;
	char *message;
};

struct encoder
{
	float sampling_rate_sps;
	uint32_t samples_per_bit;
	float amp;
	float freq;
	struct encoder_settings settings;
	uint32_t counter;
	uint32_t counter_bit;
	uint16_t message_length;
	uint16_t message_index;
	uint8_t bit_index;
	uint16_t guard_bits;
};

// Connection with the 'singletons_provider'
static singletons_provider_get_t singletons_provider_get = NULL;
static singletons_provider_h singletons_provider_handle = NULL;

// Error reporting function
static struct plc_error_api *plc_error_api;
static void *error_ctrl_handle;

// Logger served by the 'singletons_provider'
static struct plc_logger_api *logger_api;
static void *logger_handle;

// Error function shortcut
int set_error_msg(const char *msg)
{
	if (plc_error_api)
		plc_error_api->set_error_msg(error_ctrl_handle, msg);
	else
		warnx("%s", msg);
	return -1;
}

static void encoder_set_defaults(struct encoder *encoder)
{
	memset(encoder, 0, sizeof(*encoder));
	encoder->settings.offset = 500;
	encoder->settings.range = 400;
	encoder->settings.freq = 2000.0;
	encoder->settings.bit_width_us = 1000;
	encoder->settings.message = strdup("This is PlcCape. Hello!\n");
}

struct encoder *encoder_create(void)
{
	struct encoder *encoder = malloc(sizeof(struct encoder));
	encoder_set_defaults(encoder);
	return encoder;
}

static void encoder_release_resources(struct encoder *encoder)
{
	assert(encoder->settings.message);
	free(encoder->settings.message);
	encoder->settings.message = NULL;
}

void encoder_release(struct encoder *encoder)
{
	encoder_release_resources(encoder);
	free(encoder);
}

const struct plc_setting_definition accepted_settings[] = {
	{
		"sampling_rate_sps", plc_setting_float, "Sampling rate [sps]", {
			.f = 100000.0f }, 0 }, {
		"offset", plc_setting_u16, "Offset", {
			.u16 = 500 }, 0 }, {
		"range", plc_setting_u16, "Range", {
			.u16 = 400 }, 0 }, {
		"freq", plc_setting_float, "Frequency", {
			.f = 2000.0f }, 0 }, {
		"bit_width_us", plc_setting_u32, "Bit Width [us]", {
			.u32 = 1000 }, 0 }, {
		"message", plc_setting_string, "Message", {
			.s = "This is PlcCape. Hello!\n" }, 0 } };

const struct plc_setting_definition *encoder_get_accepted_settings(struct encoder *encoder,
		uint32_t *accepted_settings_count)
{
	*accepted_settings_count = ARRAY_SIZE(accepted_settings);
	return accepted_settings;
}

int encoder_begin_settings(struct encoder *encoder)
{
	encoder_release_resources(encoder);
	encoder_set_defaults(encoder);
	return 0;
}

int encoder_set_setting(struct encoder *encoder, const char *identifier,
		union plc_setting_data data)
{
	if (strcmp(identifier, "sampling_rate_sps") == 0)
	{
		encoder->sampling_rate_sps = data.f;
	}
	else if (strcmp(identifier, "offset") == 0)
	{
		encoder->settings.offset = data.u16;
	}
	else if (strcmp(identifier, "range") == 0)
	{
		if (data.u16 % 2 != 0)
			return set_error_msg("Range must be an even value");
		encoder->settings.range = data.u16;
	}
	else if (strcmp(identifier, "freq") == 0)
	{
		encoder->settings.freq = data.f;
	}
	else if (strcmp(identifier, "bit_width_us") == 0)
	{
		encoder->settings.bit_width_us = data.u32;
	}
	else if (strcmp(identifier, "message") == 0)
	{
		assert(encoder->settings.message);
		free(encoder->settings.message);
		encoder->settings.message = strdup(data.s);
		encoder->message_length = strlen(encoder->settings.message);
	}
	else
	{
		return set_error_msg("Unknown setting");
	}
	return 0;
}

int encoder_end_settings(struct encoder *encoder)
{
	encoder->samples_per_bit = round(
			encoder->sampling_rate_sps * encoder->settings.bit_width_us / 1000000.0f);
	if (encoder->samples_per_bit == 0)
		return set_error_msg("Bit width must be greater than 1 us");
	encoder->amp = (encoder->settings.range - 1) / 2;
	encoder->freq = 2.0 * M_PI * encoder->settings.freq / encoder->sampling_rate_sps;
	return 0;
}

void encoder_reset(struct encoder *encoder)
{
	encoder->counter = 0;
	encoder->counter_bit = 0;
	encoder->message_index = 0;
	encoder->bit_index = 0xFF;
	encoder->guard_bits = 0;
}

void encoder_prepare_next_samples(struct encoder *encoder, sample_tx_t *buffer,
		uint32_t buffer_count)
{
	int i;
	for (i = 0; i < buffer_count; i++)
	{
		if ((encoder->guard_bits == 0)
				&& ((encoder->bit_index == 0xFF)
						|| ((encoder->settings.message[encoder->message_index] << encoder->bit_index)
								& 0x80)))
		{
			buffer[i] = (sample_tx_t) round(
					encoder->settings.offset
							+ encoder->amp
									* sin(
											encoder->freq
													* (encoder->counter - encoder->counter_bit)));
		}
		else
		{
			buffer[i] = (sample_tx_t) round(encoder->settings.offset);
		}
		encoder->counter++;
		if ((encoder->counter % encoder->samples_per_bit) == 0)
		{
			if (encoder->guard_bits > 0)
			{
				encoder->guard_bits--;
			}
			else
			{
				if (++encoder->bit_index == 8)
				{
					encoder->guard_bits = GUARD_BITS;
					encoder->bit_index = 0xFF;
					// -1 because the '\0' ending
					if (++encoder->message_index == encoder->message_length)
						encoder->message_index = 0;
				}
				encoder->counter_bit = encoder->counter;
			}
		}
	}
}

ATTR_EXTERN void PLUGIN_API_SET_SINGLETON_PROVIDER(singletons_provider_get_t callback,
		singletons_provider_h handle)
{
	singletons_provider_get = callback;
	singletons_provider_handle = handle;
	// Ask for the required callbacks
	uint32_t version;
	singletons_provider_get(singletons_provider_handle, singleton_id_error, (void**) &plc_error_api,
			&error_ctrl_handle, &version);
	assert(!plc_error_api || (version >= 1));
	singletons_provider_get(singletons_provider_handle, singleton_id_logger, (void**) &logger_api,
			&logger_handle, &version);
	assert(!logger_api || (version >= 1));
}

ATTR_EXTERN void *PLUGIN_API_LOAD(uint32_t *plugin_api_version, uint32_t *plugin_api_size)
{
	CHECK_INTERFACE_MEMBERS_COUNT(encoder_api, 8);
	*plugin_api_version = 1;
	*plugin_api_size = sizeof(struct encoder_api);
	struct encoder_api *encoder_api = calloc(1, *plugin_api_size);
	encoder_api->create = encoder_create;
	encoder_api->release = encoder_release;
	encoder_api->get_accepted_settings = encoder_get_accepted_settings;
	encoder_api->begin_settings = encoder_begin_settings;
	encoder_api->set_setting = encoder_set_setting;
	encoder_api->end_settings = encoder_end_settings;
	encoder_api->reset = encoder_reset;
	encoder_api->prepare_next_samples = encoder_prepare_next_samples;
	return encoder_api;
}

ATTR_EXTERN void PLUGIN_API_UNLOAD(void *encoder_api)
{
	free(encoder_api);
}

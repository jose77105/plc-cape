/**
 * @file
 * @brief	**Main** file
 *
 * @see		@ref plugin-encoder-pwm
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
 *	Copyright (C) 2016 Jose Maria Ortega
 * 
 * @endcond
 */

#include <err.h>			// warnx
#include <math.h>			// sin
#include "+common/api/+base.h"
#include "+common/api/error.h"
#include "+common/api/logger.h"
#define PLUGINS_API_HANDLE_EXPLICIT_DEF
typedef struct encoder *encoder_api_h;
#include "plugins/encoder/api/encoder.h"

// Bits in idle status between character transmission to allow proper synchronization
#define GUARD_SAMPLES 1000

struct encoder_settings
{
	uint32_t offset;
	uint32_t range;
	float freq;
	uint32_t bit_width_us;
	char *message;
	int loop_message;
};

struct encoder
{
	float freq_dac_sps;
	float samples_per_bit;
	float amp;
	float freq;
	struct encoder_settings settings;
	uint32_t pwm_next;
	uint32_t pwm_counter;
	uint16_t message_length;
	uint16_t message_index;
	uint16_t guard_samples;
	int message_end_reached;
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
}

struct encoder *encoder_create(void)
{
	struct encoder *encoder = malloc(sizeof(struct encoder));
	encoder_set_defaults(encoder);
	return encoder;
}

static void encoder_release_resources(struct encoder *encoder)
{
	if (encoder->settings.message)
	{
		free(encoder->settings.message);
		encoder->settings.message = NULL;
	}
}

void encoder_release(struct encoder *encoder)
{
	encoder_release_resources(encoder);
	free(encoder);
}

// TODO: Instead of returning 'int' return an object to be passed to 'set_setting'
//	This ensures proper locking
int encoder_begin_settings(struct encoder *encoder)
{
	encoder_release_resources(encoder);
	encoder_set_defaults(encoder);
	return 1;
}

int encoder_set_setting(struct encoder *encoder, enum encoder_setting_enum setting,
		const void *data)
{
	switch (setting)
	{
	case encoder_setting_freq_dac_sps:
		encoder->freq_dac_sps = *(float*) data;
		break;
	case encoder_setting_offset:
		encoder->settings.offset = (uint32_t) data;
		break;
	case encoder_setting_range:
		if ((uint32_t) data % 2 != 0)
			return set_error_msg("Range must be an even value");
		encoder->settings.range = (uint32_t) data;
		break;
	case encoder_setting_freq:
		encoder->settings.freq = *(float*) data;
		break;
	case encoder_setting_bit_width_us:
		encoder->settings.bit_width_us = (uint32_t) data;
		break;
	case encoder_setting_message:
		if (encoder->settings.message)
			free(encoder->settings.message);
		encoder->settings.message = strdup((const char*) data);
		encoder->message_length = strlen(encoder->settings.message);
		break;
	default:
		return set_error_msg("Unknown setting");
	}
	return 0;
}

int encoder_end_settings(struct encoder *encoder)
{
	encoder->samples_per_bit = encoder->freq_dac_sps * (float) encoder->settings.bit_width_us
			/ 1000000.0;
	// TODO: Just for testing
	// encoder->samples_per_bit = 8;
	encoder->amp = (encoder->settings.range - 1) / 2;
	encoder->freq = 2.0 * M_PI * encoder->settings.freq / encoder->freq_dac_sps;
#ifdef DEBUG
	/*
	if (logger_api)
	{
		char szMsg[64];
		sprintf(szMsg, "samples_per_bit=%.1f", encoder->samples_per_bit);
		logger_api->log_line(logger_handle, szMsg);
	}
	*/
#endif
	return 0;
}

void encoder_reset(struct encoder *encoder)
{
	encoder->pwm_counter = 0;
	encoder->pwm_next = 0;
	encoder->guard_samples = GUARD_SAMPLES;
	encoder->message_end_reached = 0;
	encoder->message_index = 0;
}

void encoder_prepare_next_samples(struct encoder *encoder, sample_tx_t *buffer,
		uint32_t buffer_count)
{
	int i;
	for (i = 0; i < buffer_count; i++)
	{
		if (encoder->guard_samples == 0)
		{
			buffer[i] = (sample_tx_t) round(
					encoder->settings.offset
							+ encoder->amp * sin(encoder->freq * (encoder->pwm_counter++)));
			if (encoder->pwm_counter == encoder->pwm_next)
				encoder->guard_samples = GUARD_SAMPLES;
		}
		else
		{
			buffer[i] = (sample_tx_t) round(encoder->settings.offset);
			if (!encoder->message_end_reached)
			{
				encoder->guard_samples--;
				if (encoder->guard_samples == 0)
				{
					encoder->pwm_counter = 0;
					if (encoder->message_index == encoder->message_length)
					{
						encoder->message_index = 0;
						if (!encoder->settings.loop_message)
						{
							encoder->message_end_reached = 1;
							encoder->guard_samples = (uint16_t) -1;
						}
					}
					// TODO: Instead of rounding accumulation?
					encoder->pwm_next = round(
							encoder->samples_per_bit
									* encoder->settings.message[encoder->message_index++]);
				}
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
	singletons_provider_get(singletons_provider_handle,
			singleton_id_error, (void**) &plc_error_api, &error_ctrl_handle, &version);
	assert(!plc_error_api || (version >= 1));
	singletons_provider_get(singletons_provider_handle,
			singleton_id_logger, (void**) &logger_api, &logger_handle, &version);
	assert(!logger_api || (version >= 1));
}

ATTR_EXTERN void *PLUGIN_API_LOAD(uint32_t *plugin_api_version, uint32_t *plugin_api_size)
{
	CHECK_INTERFACE_MEMBERS_COUNT(encoder_api, 7);
	*plugin_api_version = 1;
	*plugin_api_size = sizeof(struct encoder_api);
	struct encoder_api *encoder_api = calloc(1, *plugin_api_size);
	encoder_api->create = encoder_create;
	encoder_api->release = encoder_release;
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

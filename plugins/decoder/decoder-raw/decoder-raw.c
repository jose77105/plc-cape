/**
 * @file
 * @brief	**Main** file
 *
 * @see		@ref plugin-decoder-raw
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
 *	Copyright (C) 2017 Jose Maria Ortega
 * 
 * @endcond
 */

#include <err.h>			// warnx
#include <math.h>
#include "+common/api/+base.h"
#include "+common/api/error.h"
#include "+common/api/logger.h"
#include "+common/api/setting.h"
// Declare the custom type used as handle. Doing it like this avoids the 'void*' hard-casting
#define PLUGINS_API_HANDLE_EXPLICIT_DEF
typedef struct decoder *decoder_api_h;
#include "plugins/decoder/api/decoder.h"

struct decoder
{
	float sampling_rate_sps;
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

static void decoder_set_defaults(struct decoder *decoder)
{
	memset(decoder, 0, sizeof(*decoder));
	decoder->sampling_rate_sps = 100000.0f;
}

struct decoder *decoder_create(void)
{
	struct decoder *decoder = malloc(sizeof(struct decoder));
	decoder_set_defaults(decoder);
	return decoder;
}

static void decoder_release_resources(struct decoder *decoder)
{
}

void decoder_release(struct decoder *decoder)
{
	decoder_release_resources(decoder);
	free(decoder);
}

const struct plc_setting_definition accepted_settings[] = {
	{
		"sampling_rate_sps", plc_setting_float, "Freq Capture [sps]", {
			.f = 100000.0f }, 0 }};

const struct plc_setting_definition *decoder_get_accepted_settings(struct decoder *decoder,
		uint32_t *accepted_settings_count)
{
	*accepted_settings_count = ARRAY_SIZE(accepted_settings);
	return accepted_settings;
}

int decoder_begin_settings(struct decoder *decoder)
{
	decoder_release_resources(decoder);
	decoder_set_defaults(decoder);
	return 0;
}

int decoder_set_setting(struct decoder *decoder, const char *identifier,
		union plc_setting_data data)
{
	if (strcmp(identifier, "sampling_rate_sps") == 0)
	{
		decoder->sampling_rate_sps = data.f;
	}
	else
	{
		return set_error_msg("Unknown setting");
	}
	return 0;
}

int decoder_end_settings(struct decoder *decoder)
{
	return 0;
}

void decoder_initialize(struct decoder *decoder, uint32_t chunk_samples)
{
#ifdef DEBUG
	if (logger_api)
		logger_api->log_line(logger_handle, "decoder_initialize");
#endif
}

void decoder_terminate(struct decoder *decoder)
{
#ifdef DEBUG
	if (logger_api)
		logger_api->log_line(logger_handle, "decoder_terminate");
#endif
}

uint32_t decoder_parse_next_samples(struct decoder *decoder, const sample_rx_t *buffer_in,
		uint8_t *buffer_data_out, uint32_t buffer_data_out_count)
{
	return 0;
}

ATTR_EXTERN void PLUGIN_API_SET_SINGLETON_PROVIDER(singletons_provider_get_t callback,
		singletons_provider_h handle)
{
	singletons_provider_get = callback;
	singletons_provider_handle = handle;
	// Ask for the required callbacks
	uint32_t version;
	singletons_provider_get(singletons_provider_handle, singleton_id_logger, (void**) &logger_api,
			&logger_handle, &version);
	assert(!logger_api || (version >= 1));
}

ATTR_EXTERN void *PLUGIN_API_LOAD(uint32_t *plugin_api_version, uint32_t *plugin_api_size)
{
	CHECK_INTERFACE_MEMBERS_COUNT(decoder_api, 9);
	*plugin_api_version = 1;
	*plugin_api_size = sizeof(struct decoder_api);
	struct decoder_api *decoder_api = calloc(1, *plugin_api_size);
	decoder_api->create = decoder_create;
	decoder_api->release = decoder_release;
	decoder_api->get_accepted_settings = decoder_get_accepted_settings;
	decoder_api->begin_settings = decoder_begin_settings;
	decoder_api->set_setting = decoder_set_setting;
	decoder_api->end_settings = decoder_end_settings;
	decoder_api->initialize = decoder_initialize;
	decoder_api->terminate = decoder_terminate;
	decoder_api->parse_next_samples = decoder_parse_next_samples;
	return decoder_api;
}

ATTR_EXTERN void PLUGIN_API_UNLOAD(void *decoder_api)
{
	free(decoder_api);
}

/**
 * @file
 * @brief	**Main** file
 *
 * @see		@ref plugin-encoder-wav
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
#include <fcntl.h>			// open
#include <unistd.h>			// read
#include "+common/api/+base.h"
#include "+common/api/error.h"
#include "+common/api/logger.h"
#include "+common/api/setting.h"
#define PLUGINS_API_HANDLE_EXPLICIT_DEF
typedef struct encoder *encoder_api_h;
#include "plugins/encoder/api/encoder.h"

struct encoder_settings
{
	char *filename;
	int fd;
};

// Info on WAV format:
//	http://www.topherlee.com/software/pcm-tut-wavformat.html
//	http://soundfile.sapp.org/doc/WaveFormat/
//	http://www-mmsp.ece.mcgill.ca/documents/audioformats/wave/wave.html

// WAV examples:
//	http://www.downloadfreesound.com/8-bit-sound-effects/
//	
struct wav_header
{
	char chunk_id[4];			// "RIFF"
	int chunk_size;				// chunk_size = file_size-8
	char format[4];				// "WAVE"
	char subchunk1_id[4];		// "fmt "
	int subchunk1_size;			// [16|18|40]
	short int audio_format;		// 1:PCM
	short int num_channels;		// 1:Mono, 2:Stereo
	int samples_per_second;		// Typ: 8000|11025|22050|44100
	int bytes_per_second;		// samples_per_second*bits_per_sample*num_channels/8
	short int block_align;		// Typ: 1|2|4
	short int bits_per_sample;	// Typ: 8|16
	char subchunk2_id[4];		// "data"
	int subchunk2_size;			// size of the data section
};

struct encoder
{
	struct encoder_settings settings;
	struct wav_header wav_header;
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

static void encoder_set_defaults(struct encoder *encoder)
{
	memset(encoder, 0, sizeof(*encoder));
	encoder->settings.fd = -1;
}

struct encoder *encoder_create(void)
{
	struct encoder *encoder = malloc(sizeof(struct encoder));
	encoder_set_defaults(encoder);
	return encoder;
}

static void encoder_release_resources(struct encoder *encoder)
{
	if (encoder->settings.filename)
		free(encoder->settings.filename);
	if (encoder->settings.fd != -1)
		close(encoder->settings.fd);
}

void encoder_release(struct encoder *encoder)
{
	encoder_release_resources(encoder);
	free(encoder);
}

static const struct plc_setting_definition accepted_settings[] = {
	{
		"filename", plc_setting_string, "Filename", {
			.s = "spi.wav" }, 0 }, };

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
	if (strcmp(identifier, "filename") == 0)
	{
		encoder->settings.filename = strdup(data.s);
	}
	else
	{
		return set_error_msg("Unknown setting");
	}
	return 0;
}

static void parse_wav_header(struct encoder *encoder)
{
	size_t bytes_read = read(encoder->settings.fd, &encoder->wav_header,
			sizeof(encoder->wav_header));
	assert(bytes_read == sizeof(encoder->wav_header));
	int ok = (strncmp(encoder->wav_header.chunk_id, "RIFF", 4) == 0)
			&& (strncmp(encoder->wav_header.format, "WAVE", 4) == 0)
			&& (strncmp(encoder->wav_header.subchunk1_id, "fmt ", 4) == 0)
			&& (encoder->wav_header.audio_format == 1) && (encoder->wav_header.num_channels == 1)
			&& ((encoder->wav_header.bits_per_sample == 8)
					|| (encoder->wav_header.bits_per_sample == 16))
			&& (strncmp(encoder->wav_header.subchunk2_id, "data", 4) == 0);
	if (logger_api)
		logger_api->log_line(logger_handle, ok ? "WAV supported\n" : "WAV NOT supported!");
	if (!ok)
		set_error_msg("Unsupported WAV");
}

int encoder_end_settings(struct encoder *encoder)
{
	return 0;
}

void encoder_reset(struct encoder *encoder)
{
	if (encoder->settings.filename == NULL)
	{
		set_error_msg("No filename provided");
		return;
	}
	if (encoder->settings.fd != -1)
		close(encoder->settings.fd);
	encoder->settings.fd = open(encoder->settings.filename, O_RDONLY);
	if (encoder->settings.fd < 0)
	{
		set_error_msg("Can't open filename");
		return;
	}
	parse_wav_header(encoder);
}

void encoder_prepare_next_samples(struct encoder *encoder, sample_tx_t *buffer,
		uint32_t buffer_count)
{
	size_t bytes_read;
	sample_tx_t *samples_buffer_cur = buffer;
	if (encoder->wav_header.bits_per_sample == 8)
	{
		uint8_t *wav_buffer = malloc(buffer_count);
		uint8_t *wav_buffer_cur = wav_buffer;
		// Limitation: Only 8-bits WAV supported
		size_t remaining_bytes = buffer_count;
		do
		{
			bytes_read = read(encoder->settings.fd, wav_buffer_cur, remaining_bytes);
			assert(bytes_read >= 0);
			if (bytes_read == 0)
			{
				off_t offset = lseek(encoder->settings.fd, sizeof(struct wav_header), SEEK_SET);
				assert(offset == sizeof(struct wav_header));
			}
			else
			{
				remaining_bytes -= bytes_read;
				// Conversion from 8-bytes RAW WAV to 16-bits samples centered at high level
				for (; bytes_read > 0; bytes_read--)
					*samples_buffer_cur++ = 0x400 - 0x200 + 2 * (*wav_buffer_cur++);
			}
		} while (remaining_bytes > 0);
		free(wav_buffer);
	}
	else
	{
		int16_t *wav_buffer = (int16_t*) malloc(buffer_count * sizeof(int16_t));
		int16_t *wav_buffer_cur = wav_buffer;
		// Limitation: Only 8-bits WAV supported
		size_t remaining_bytes = buffer_count * sizeof(int16_t);
		do
		{
			bytes_read = read(encoder->settings.fd, wav_buffer_cur, remaining_bytes);
			assert(bytes_read >= 0);
			if (bytes_read == 0)
			{
				off_t offset = lseek(encoder->settings.fd, sizeof(struct wav_header), SEEK_SET);
				assert(offset == sizeof(struct wav_header));
			}
			else
			{
				remaining_bytes -= bytes_read;
				// Conversion from 8-bytes RAW WAV to 16-bits samples centered at high level
				for (; bytes_read > 0; bytes_read -= sizeof(int16_t))
					*samples_buffer_cur++ = 0x200 + (*wav_buffer_cur++) / 64;
			}
		} while (remaining_bytes > 0);
		free(wav_buffer);
	}
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

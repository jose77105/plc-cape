/**
 * @file
 * @brief	**Main** file
 *
 * @see		@ref plugin-encoder-wave
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
#include <fcntl.h>			// open
#include <math.h>			// sin
#include <unistd.h>			// read
#include "+common/api/+base.h"
#include "+common/api/error.h"
#include "+common/api/logger.h"
#include "+common/api/setting.h"
#define PLUGINS_API_HANDLE_EXPLICIT_DEF
typedef struct encoder *encoder_api_h;
#include "plugins/encoder/api/encoder.h"

enum stream_type_enum
{
	stream_ramp = 0,
	stream_triangular,
	stream_constant,
	stream_freq_max,
	stream_freq_max_div2,
	stream_freq_sweep,
	stream_freq_sweep_preload,
	stream_file,
	stream_bit_padding_per_cycle,
	stream_am_modulation,
	stream_freq_sinus,
	stream_ook_pattern,
	stream_ook_hi,
	stream_square,
	stream_COUNT
};
static const char *stream_type_enum_text[stream_COUNT] = {
	"ramp", "triangular", "constant", "freq_max", "freq_max_div2", "freq_sweep",
	"freq_sweep_preload", "file", "bit_padding_per_cycle", "am_modulation", "freq_sinus",
	"ook_pattern", "ook_hi", "square" };

struct encoder_settings
{
	enum stream_type_enum stream_type;
	sample_tx_t offset;
	sample_tx_t range;
	float freq;
	char *filename;
	uint32_t bit_width_us;
};

struct encoder_stream_ramp
{
	float value_last;
	float value_max;
	float value_delta;
};

struct encoder_stream_triangular
{
	float value_last;
	float value_min;
	float value_max;
	float value_delta;
};

struct encoder_stream_file
{
	int fd;
};

struct encoder_stream_am
{
	float sample;
};

struct encoder_stream_bit_padding
{
	sample_tx_t value_last;
};

struct encoder_stream_sweep
{
	float freq_ini;
	float freq_delta;
	float amp;
	float offset;
	float freq;
	uint32_t iteration;
	uint32_t iterations;
	uint32_t iteration_sample;
	uint32_t iteration_samples;
	// TODO: Only used on sweep_preload. Refactor
	struct iteration_cycle
	{
		uint32_t length;
		float *data;
	}*iterations_cycle;
	uint32_t iteration_cycle_sample;
};

struct encoder_stream_pattern
{
	int symbol_index;
	uint32_t counter_bit;
	uint32_t samples_per_bit;
};

struct encoder_stream_ook_hi
{
	uint32_t counter_bit;
	uint16_t message_index;
	uint8_t bit_index;
};

struct encoder_stream_sinus
{
	float amp;
	float freq;
};

struct encoder_stream_square
{
	sample_tx_t lo_level;
	sample_tx_t hi_level;
	float ratio_cycle_delta;
	float ratio_cycle;
};

struct encoder
{
	struct encoder_settings settings;
	float sampling_rate_sps;
	uint32_t counter;
	union
	{
		struct encoder_stream_ramp stream_ramp;
		struct encoder_stream_triangular stream_triangular;
		struct encoder_stream_sweep stream_sweep;
		struct encoder_stream_file stream_file;
		struct encoder_stream_bit_padding stream_bit_padding;
		struct encoder_stream_am stream_am;
		struct encoder_stream_pattern stream_ook_pattern;
		struct encoder_stream_ook_hi stream_ook_hi;
		struct encoder_stream_sinus stream_sinus;
		struct encoder_stream_square stream_square;
	};
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

static void encoder_set_defaults(struct encoder *encoder);
static void encoder_release_resources(struct encoder *encoder);

// Error function shortcut
int set_error_msg(const char *msg)
{
	if (plc_error_api)
		plc_error_api->set_error_msg(error_ctrl_handle, msg);
	else
		warnx("%s", msg);
	return -1;
}

struct encoder *encoder_create(void)
{
	struct encoder *encoder = malloc(sizeof(struct encoder));
	encoder_set_defaults(encoder);
	return encoder;
}

void encoder_release(struct encoder *encoder)
{
	encoder_release_resources(encoder);
	free(encoder);
}

static void encoder_set_defaults(struct encoder *encoder)
{
	memset(encoder, 0, sizeof(*encoder));
	encoder->settings.stream_type = stream_freq_sinus;
	encoder->settings.offset = 512;
	encoder->settings.range = 256;
	encoder->settings.freq = 2000;
	encoder->sampling_rate_sps = 100000;
	encoder->settings.bit_width_us = 1000;
}

static void encoder_release_resources(struct encoder *encoder)
{
	switch (encoder->settings.stream_type)
	{
	case stream_file:
		if (encoder->settings.filename)
			free(encoder->settings.filename);
		close(encoder->stream_file.fd);
		break;
	case stream_freq_sweep:
	case stream_freq_sweep_preload:
		if (encoder->stream_sweep.iterations_cycle != NULL)
		{
			uint32_t iteration;
			for (iteration = 0; iteration < encoder->stream_sweep.iterations; iteration++)
				free(encoder->stream_sweep.iterations_cycle[iteration].data);
			free(encoder->stream_sweep.iterations_cycle);
		}
		break;
	default:
		break;
	}
}

struct plc_setting_extra_data stream_type_captions = {
	plc_setting_extra_data_enum_captions, {
		.enum_captions.captions = stream_type_enum_text, .enum_captions.captions_count =
				stream_COUNT } };

const struct plc_setting_definition accepted_settings[] = {
	{
		"sampling_rate_sps", plc_setting_float, "Sampling rate [sps]", {
			.f = 100000.0f }, 0 }, {
		"stream_type", plc_setting_enum, "Stream type", {
			.u32 = stream_freq_sinus }, 1, &stream_type_captions }, {
		"offset", plc_setting_u16, "Offset", {
			.u16 = 500 }, 0 }, {
		"range", plc_setting_u16, "Range", {
			.u16 = 400 }, 0 }, {
		"freq", plc_setting_float, "Frequency", {
			.f = 2000.0f }, 0 }, {
		"filename", plc_setting_string, "Filename", {
			.s = "spi.bin" }, 0 }, {
		"bit_width_us", plc_setting_u32, "Bit Width [us]", {
			.u32 = 1000 }, 0 } };

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
	else if (strcmp(identifier, "stream_type") == 0)
	{
		encoder->settings.stream_type = data.u32;
	}
	else if (strcmp(identifier, "offset") == 0)
	{
		encoder->settings.offset = data.u16;
	}
	else if (strcmp(identifier, "range") == 0)
	{
		encoder->settings.range = data.u16;
	}
	else if (strcmp(identifier, "freq") == 0)
	{
		encoder->settings.freq = data.f;
	}
	else if (strcmp(identifier, "filename") == 0)
	{
		if (encoder->settings.filename)
			free(encoder->settings.filename);
		encoder->settings.filename = strdup(data.s);
	}
	else if (strcmp(identifier, "bit_width_us") == 0)
	{
		encoder->settings.bit_width_us = data.u32;
	}
	else
	{
		return set_error_msg("Unknown setting");
	}
	return 0;
}

void encoder_reset(struct encoder *encoder)
{
	encoder->counter = 0;
	switch (encoder->settings.stream_type)
	{
	case stream_ramp:
		encoder->stream_ramp.value_last = encoder->settings.offset - encoder->settings.range / 2;
		break;
	case stream_triangular:
		encoder->stream_triangular.value_last = encoder->settings.offset
				- encoder->settings.range / 2;
		break;
	case stream_freq_sweep:
	case stream_freq_sweep_preload:
		encoder->stream_sweep.iteration_cycle_sample = 0;
		encoder->stream_sweep.iteration_sample = 0;
		encoder->stream_sweep.iteration = 0;
		encoder->stream_sweep.freq = encoder->stream_sweep.freq_ini;
		break;
	case stream_file:
		lseek(encoder->stream_file.fd, 0, SEEK_SET);
		break;
	case stream_bit_padding_per_cycle:
		encoder->stream_bit_padding.value_last = 0;
		break;
	case stream_am_modulation:
		encoder->stream_am.sample = 1.0;
		break;
	case stream_ook_pattern:
		encoder->stream_ook_pattern.symbol_index = 0;
		encoder->stream_ook_pattern.counter_bit = 0;
		break;
	case stream_ook_hi:
		encoder->stream_ook_hi.counter_bit = 0;
		encoder->stream_ook_hi.message_index = 0;
		encoder->stream_ook_hi.bit_index = 0xFF;
		break;
	case stream_freq_sinus:
		// TODO: To be sure that resulting sample is within the limits, 'encoder->settings.range-1'
		//	is used. Improvement: use the whole range
		encoder->stream_sinus.amp = (encoder->settings.range - 1) / 2;
		encoder->stream_sinus.freq = 2.0 * M_PI * encoder->settings.freq
				/ encoder->sampling_rate_sps;
		break;
	case stream_square:
		encoder->stream_square.lo_level = encoder->settings.offset - encoder->settings.range / 2;
		encoder->stream_square.hi_level = encoder->stream_square.lo_level + encoder->settings.range;
		encoder->stream_square.ratio_cycle_delta = encoder->settings.freq
				/ encoder->sampling_rate_sps;
		encoder->stream_square.ratio_cycle = 0;
		break;
	default:
		break;
	}
}

int encoder_end_settings(struct encoder *encoder)
{
	switch (encoder->settings.stream_type)
	{
	case stream_file:
		if (encoder->settings.filename == NULL)
			return set_error_msg("No filename provided");
		encoder->stream_file.fd = open(encoder->settings.filename, O_RDONLY);
		if (encoder->stream_file.fd < 0)
			return set_error_msg("Can't open filename");
		break;
	case stream_ramp:
		encoder->stream_ramp.value_max = encoder->settings.offset + encoder->settings.range / 2 - 1;
		encoder->stream_ramp.value_delta = encoder->settings.freq / encoder->sampling_rate_sps
				* ((float) (encoder->settings.range - 1));
		break;
	case stream_triangular:
		encoder->stream_triangular.value_min = encoder->settings.offset
				- encoder->settings.range / 2;
		encoder->stream_triangular.value_max = encoder->settings.offset
				+ encoder->settings.range / 2 - 1;
		encoder->stream_triangular.value_delta = encoder->settings.freq / encoder->sampling_rate_sps
				* ((float) (encoder->settings.range - 1)) * 2;
		break;
	case stream_freq_sweep:
	case stream_freq_sweep_preload:
	{
		static const uint32_t iterations = 20;
		// TODO: Make 'freq_ini' and 'freq_end' configurable
		static const float iteration_duration_seconds = 0.5;
		// For sweep based on center frequency
		//	float freq_mid = 2.0*M_PI*encoder->settings.freq/encoder->sampling_rate_sps;
		//	encoder->stream_sweep.freq_ini = freq_mid*0.5;
		//	float freq_end = freq_mid*1.5;
		// Sweep using 'current freq' as 'initial freq' and 'ending freq' based on maximum
		encoder->stream_sweep.freq_ini = 2.0 * M_PI * encoder->settings.freq
				/ encoder->sampling_rate_sps;
		float freq_end = 0.6 * M_PI;
		encoder->stream_sweep.freq_delta = (freq_end - encoder->stream_sweep.freq_ini)
				/ (float) iterations;
		encoder->stream_sweep.offset = encoder->settings.offset;
		encoder->stream_sweep.amp = round((encoder->settings.range - 1) / 2.0);
		encoder->stream_sweep.iterations = iterations;
		encoder->stream_sweep.iteration_samples = round(
				encoder->sampling_rate_sps * iteration_duration_seconds);
		if (encoder->settings.stream_type == stream_freq_sweep_preload)
		{
			encoder->stream_sweep.iterations_cycle = malloc(
					iterations * sizeof(*encoder->stream_sweep.iterations_cycle));
			uint32_t iteration;
			float freq = encoder->stream_sweep.freq_ini;
			for (iteration = 0; iteration < iterations;
					iteration++, freq += encoder->stream_sweep.freq_delta)
			{
				// TODO: Calculate a suitable buffer length to accomodate several periods
				//	('cycle_length') in order to minimize the level gap between iterations (as done
				//	in 'plc-cape-freq-response')
				//	Here, for simplicity, use just one 'cycle_length'
				uint32_t cycle_length = round(2.0 * M_PI / freq);
				assert(cycle_length > 0);
				encoder->stream_sweep.iterations_cycle[iteration].length = cycle_length;
				uint32_t i;
				float *data = malloc(cycle_length * sizeof(float));
				for (i = 0; i < cycle_length; i++)
					data[i] = encoder->stream_sweep.offset
							+ encoder->stream_sweep.amp * sin(freq * i);
				encoder->stream_sweep.iterations_cycle[iteration].data = data;
			}
		}
		break;
	}
	case stream_ook_pattern:
		encoder->stream_ook_pattern.samples_per_bit = round(
				encoder->sampling_rate_sps * (float) encoder->settings.bit_width_us / 1000000.0);
		if (encoder->stream_ook_pattern.samples_per_bit == 0)
			return set_error_msg("Bit width must be greater than 1 us");
		break;
	default:
		break;
	}
	encoder_reset(encoder);
	return 0;
}

void encoder_prepare_next_samples(struct encoder *encoder, sample_tx_t *buffer,
		uint32_t buffer_count)
{
	int i;
	switch (encoder->settings.stream_type)
	{
	case stream_constant:
		for (i = 0; i < buffer_count; i++)
			buffer[i] = encoder->settings.offset;
		break;
	case stream_freq_max:
	{
		int nMin = encoder->settings.offset - encoder->settings.range / 2 + 1;
		int nMax = encoder->settings.offset + encoder->settings.range / 2 - 1;
		for (i = 0; i < buffer_count; i++)
			buffer[i] = (i % 2) ? nMin : nMax;
		break;
	}
	case stream_freq_max_div2:
	{
		int nMin = encoder->settings.offset - encoder->settings.range / 2 + 1;
		int nMed = encoder->settings.offset;
		int nMax = encoder->settings.offset + encoder->settings.range / 2 - 1;
		for (i = 0; i < buffer_count; i++)
		{
			if ((i % 4) == 0)
				buffer[i] = nMin;
			else if ((i % 4) == 2)
				buffer[i] = nMax;
			else
				buffer[i] = nMed;
		}
		break;
	}
	case stream_freq_sinus:
	{
		for (i = 0; i < buffer_count; i++, encoder->counter++)
			buffer[i] = (sample_tx_t) round(
					(float) encoder->settings.offset
							+ encoder->stream_sinus.amp
									* sin(encoder->stream_sinus.freq * encoder->counter));
		break;
	}
	case stream_ramp:
		for (i = 0; i < buffer_count;
				i++, encoder->stream_ramp.value_last += encoder->stream_ramp.value_delta)
		{
			if (encoder->stream_ramp.value_last >= encoder->stream_ramp.value_max)
				encoder->stream_ramp.value_last -= encoder->settings.range;
			sample_tx_t n = (sample_tx_t) round(encoder->stream_ramp.value_last);
			buffer[i] = n;
		}
		break;
	case stream_triangular:
		for (i = 0; i < buffer_count;
				i++, encoder->stream_triangular.value_last += encoder->stream_triangular.value_delta)
		{
			if (((encoder->stream_triangular.value_delta > 0.0)
					&& (encoder->stream_triangular.value_last
							>= encoder->stream_triangular.value_max))
					|| ((encoder->stream_triangular.value_delta < 0.0)
							&& (encoder->stream_triangular.value_last
									< encoder->stream_triangular.value_min)))
			{
				// TODO: Calculate the exact value on delta toggling using linear interpolation
				encoder->stream_triangular.value_last -= encoder->stream_triangular.value_delta;
				encoder->stream_triangular.value_delta = -encoder->stream_triangular.value_delta;
			}
			buffer[i] = (sample_tx_t) round(encoder->stream_triangular.value_last);
		}
		break;
	case stream_file:
	{
		size_t bytes_read;
		uint8_t *buffer_pos = (uint8_t*) buffer;
		size_t remaining_bytes = buffer_count * sizeof(sample_tx_t);
		do
		{
			bytes_read = read(encoder->stream_file.fd, buffer_pos, remaining_bytes);
			assert(bytes_read >= 0);
			if (bytes_read == 0)
			{
				off_t offset = lseek(encoder->stream_file.fd, 0, SEEK_SET);
				assert(offset == 0);
			}
			else
			{
				remaining_bytes -= bytes_read;
				buffer_pos += bytes_read;
			}
		} while (remaining_bytes > 0);
		break;
	}
	case stream_bit_padding_per_cycle:
	{
		// Used to check that ping-pong OK with a SPI trame that fills 1-bit more on each iteration 
		//	and restarts on 10-bits completion
		encoder->stream_bit_padding.value_last = (encoder->stream_bit_padding.value_last << 1)
				| 0x1;
		if (encoder->stream_bit_padding.value_last == 0x3FF)
			encoder->stream_bit_padding.value_last = 0x1;
		for (i = buffer_count; i > 0; i--)
			*buffer++ = encoder->stream_bit_padding.value_last;
		break;
	}
	case stream_freq_sweep:
	case stream_freq_sweep_preload:
	{
		// Alias to simplify the reading: 'p' means 'parameters'
		struct encoder_stream_sweep *p = &encoder->stream_sweep;
		if (encoder->settings.stream_type == stream_freq_sweep_preload)
		{
			for (i = 0; i < buffer_count; i++)
			{
				buffer[i] = round(
						p->iterations_cycle[p->iteration].data[p->iteration_cycle_sample]);
				if (++p->iteration_cycle_sample == p->iterations_cycle[p->iteration].length)
					p->iteration_cycle_sample = 0;
				if (++p->iteration_sample == p->iteration_samples)
				{
					p->iteration_sample = 0;
					p->iteration_cycle_sample = 0;
					if (++p->iteration == p->iterations)
						p->iteration = 0;
				}
			}
		}
		else
		{
			for (i = 0; i < buffer_count; i++)
			{
				buffer[i] = (sample_tx_t) round(
						p->offset + p->amp * sin(p->freq * p->iteration_sample));
				if (++p->iteration_sample == p->iteration_samples)
				{
					p->iteration_sample = 0;
					if (++p->iteration == p->iterations)
					{
						p->iteration = 0;
						p->freq = p->freq_ini;
					}
					else
					{
						p->freq += p->freq_delta;
					}
				}
			}
		}
		break;
	}
	case stream_am_modulation:
	{
		float amp = encoder->stream_am.sample * (encoder->settings.range - 1) / 2;
		float freq = 2.0 * M_PI * encoder->settings.freq / encoder->sampling_rate_sps;
		for (i = 0; i < buffer_count; i++, encoder->counter++)
			buffer[i] = (sample_tx_t) floor(
					encoder->settings.offset + amp * cos(freq * encoder->counter) + .5);
		encoder->stream_am.sample -= 0.002;
		if (encoder->stream_am.sample < 0.0)
			encoder->stream_am.sample = 1.0;
		break;
	}
	case stream_ook_pattern:
	{
		static const uint8_t pattern[] = {
			1, 0, 1, 0, 1, 1, 1, 0 };
		// TODO: To be sure that resulting sample is within the limits
		//	'encoder->settings.range-1' is used. Improvement: use the whole range
		float amp = (encoder->settings.range - 1) / 2;
		float freq = 2.0 * M_PI * encoder->settings.freq / encoder->sampling_rate_sps;
		for (i = 0; i < buffer_count; i++)
		{
			if (pattern[encoder->stream_ook_pattern.symbol_index])
				buffer[i] =
						(sample_tx_t) round(
								encoder->settings.offset
										+ amp
												* sin(
														freq
																* (encoder->counter
																		- encoder->stream_ook_pattern.counter_bit)));
			else
				buffer[i] = (sample_tx_t) round(encoder->settings.offset);
			encoder->counter++;
			if ((encoder->counter % encoder->stream_ook_pattern.samples_per_bit) == 0)
			{
				encoder->stream_ook_pattern.symbol_index++;
				encoder->stream_ook_pattern.counter_bit = encoder->counter;
				if (encoder->stream_ook_pattern.symbol_index >= ARRAY_SIZE(pattern))
					encoder->stream_ook_pattern.symbol_index = 0;
			}
		}
		break;
	}
	case stream_ook_hi:
	{
		// Pattern: 1 start-bit + 'Hi' + 10 inactive bits
		static const char message[] = "H\0\0\0i\0\0\0\0\0\0\0\0\0\0";
		uint32_t samples_per_bit = round(
				encoder->sampling_rate_sps * (float) encoder->settings.bit_width_us / 1000000.0);
		float amp = (encoder->settings.range - 1) / 2;
		float freq = 2.0 * M_PI * encoder->settings.freq / encoder->sampling_rate_sps;
		for (i = 0; i < buffer_count; i++)
		{
			if ((message[encoder->stream_ook_hi.message_index] != 0)
					&& ((encoder->stream_ook_hi.bit_index == 0xFF)
							|| ((message[encoder->stream_ook_hi.message_index]
									<< encoder->stream_ook_hi.bit_index) & 0x80)))
			{
				buffer[i] =
						(sample_tx_t) round(
								encoder->settings.offset
										+ amp
												* sin(
														freq
																* (encoder->counter
																		- encoder->stream_ook_hi.counter_bit)));
			}
			else
			{
				buffer[i] = (sample_tx_t) round(encoder->settings.offset);
			}
			encoder->counter++;
			if ((encoder->counter % samples_per_bit) == 0)
			{
				if (++encoder->stream_ook_hi.bit_index == 8)
				{
					encoder->stream_ook_hi.bit_index = 0xFF;
					// -1 because the '\0' ending
					if (++encoder->stream_ook_hi.message_index == ARRAY_SIZE(message) - 1)
						encoder->stream_ook_hi.message_index = 0;
				}
				encoder->stream_ook_hi.counter_bit = encoder->counter;
			}
		}
		break;
	}
	case stream_square:
		for (i = 0; i < buffer_count;
				i++, encoder->stream_square.ratio_cycle += encoder->stream_square.ratio_cycle_delta)
		{
			if (encoder->stream_square.ratio_cycle >= 1.0)
				encoder->stream_square.ratio_cycle -= 1.0;
			buffer[i] =
					(encoder->stream_square.ratio_cycle >= .5) ?
							encoder->stream_square.hi_level : encoder->stream_square.lo_level;
		}
		break;
	default:
		assert(0);
		break;
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

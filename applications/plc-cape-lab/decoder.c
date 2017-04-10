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
#include "decoder.h"
#include "plugins.h"
#include "plugins/decoder/api/decoder.h"

struct decoder
{
	struct plugin *plugin;
	struct decoder_api *api;
	decoder_api_h api_handle;
};

struct decoder *decoder_create(const char *path)
{
	struct decoder *decoder = calloc(1, sizeof(struct decoder));
	uint32_t api_version, api_size;
	decoder->plugin = load_plugin(path, (void**) &decoder->api, &api_version, &api_size);
	assert((api_version >= 1) && (api_size >= sizeof(struct decoder_api)));
	decoder->api_handle = decoder->api->create();
	return decoder;
}

void decoder_release(struct decoder *decoder)
{
	decoder->api->release(decoder->api_handle);
	unload_plugin(decoder->plugin);
	free(decoder);
}

inline void decoder_initialize_demodulator(struct decoder *decoder, uint32_t chunk_samples,
		sample_rx_t demod_data_hi_threshold, sample_rx_t data_offset,
		float samples_per_bit, uint32_t samples_to_file)
{
	decoder->api->initialize_demodulator(decoder->api_handle, chunk_samples,
			demod_data_hi_threshold, data_offset, samples_per_bit, samples_to_file);
}

inline void decoder_terminate_demodulator(struct decoder *decoder)
{
	decoder->api->terminate_demodulator(decoder->api_handle);
}

inline uint32_t decoder_parse_next_samples(struct decoder *decoder, const sample_rx_t *buffer_in,
		uint8_t *buffer_data_out, uint32_t buffer_data_out_count)
{
	return decoder->api->parse_next_samples(decoder->api_handle, buffer_in, buffer_data_out,
			buffer_data_out_count);
}

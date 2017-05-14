/**
 * @file
 * 
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include <alsa/asoundlib.h>
#include <pthread.h>
#include "+common/api/+base.h"
#include "api/afe.h"
#include "error.h"
#include "logger.h"
#define PLC_TX_SCHED_HANDLE_EXPLICIT_DEF
typedef struct tx_sched_alsa *plc_tx_sched_h;
#include "tx_sched.h"

#define VERBOSE

#define _(text) text

static const char *adc_tx_device = "default";

struct tx_sched_alsa
{
	tx_fill_cycle_callback_t tx_fill_cycle_callback;
	tx_fill_cycle_callback_h tx_fill_cycle_callback_handle;
	uint16_t *frames_buffer;
	uint32_t frames_buffer_len;
	snd_pcm_t *snd_pcm_handle;
	float freq_sampling_sps;
	snd_pcm_hw_params_t *hwparams;
	snd_pcm_sw_params_t *swparams;
};

static int set_hwparams(struct tx_sched_alsa *tx_sched, snd_pcm_uframes_t period_len)
{
	int err;
	// Choose all parameters
	// NOTES:
	//	* Although the 'SND_PCM_FORMAT_U16' is an available format it is not accepted on some PCs
	//	* A 'freq_sampling_sps' == 600 kbps is not accepted by my sound card
	err = snd_pcm_hw_params_any(tx_sched->snd_pcm_handle, tx_sched->hwparams);
	if (err >= 0)
		err = snd_pcm_hw_params_set_access(tx_sched->snd_pcm_handle, tx_sched->hwparams,
				SND_PCM_ACCESS_RW_INTERLEAVED);
	if (err >= 0)
		err = snd_pcm_hw_params_set_format(tx_sched->snd_pcm_handle, tx_sched->hwparams,
				SND_PCM_FORMAT_S16);
	if (err >= 0)
		err = snd_pcm_hw_params_set_channels(tx_sched->snd_pcm_handle, tx_sched->hwparams, 1);
	if (err >= 0)
		err = snd_pcm_hw_params_set_rate(tx_sched->snd_pcm_handle, tx_sched->hwparams,
				tx_sched->freq_sampling_sps, 0);
	if (err >= 0)
	{
		// Default periods = 4 as a reasonable value
		unsigned int nperiods = 4;
		snd_pcm_uframes_t buffer_len = period_len * nperiods;
		// Buffers
		snd_pcm_uframes_t period_len_min;
		snd_pcm_uframes_t period_len_max;
		snd_pcm_uframes_t buffer_len_min;
		snd_pcm_uframes_t buffer_len_max;
		err = snd_pcm_hw_params_get_buffer_size_min(tx_sched->hwparams, &buffer_len_min);
		assert(err >= 0);
		err = snd_pcm_hw_params_get_buffer_size_max(tx_sched->hwparams, &buffer_len_max);
		assert(err >= 0);
		err = snd_pcm_hw_params_get_period_size_min(tx_sched->hwparams, &period_len_min, NULL);
		assert(err >= 0);
		err = snd_pcm_hw_params_get_period_size_max(tx_sched->hwparams, &period_len_max, NULL);
		assert(err >= 0);
		err = snd_pcm_hw_params_set_periods_near(tx_sched->snd_pcm_handle, tx_sched->hwparams,
				&nperiods, NULL);
		if (err >= 0)
		{
			err = snd_pcm_hw_params_set_buffer_size_near(tx_sched->snd_pcm_handle,
					tx_sched->hwparams, &buffer_len);
			// Update the parameters
			if (err >= 0)
			{
				err = snd_pcm_hw_params(tx_sched->snd_pcm_handle, tx_sched->hwparams);
				if (err >= 0)
				{
					err = snd_pcm_hw_params_get_buffer_size(tx_sched->hwparams, &buffer_len);
					assert(err >= 0);
					err = snd_pcm_hw_params_get_period_size(tx_sched->hwparams, &period_len, NULL);
					assert(
							(err >= 0) && (period_len >= period_len_min)
									&& (period_len <= period_len_max)
									&& (buffer_len >= buffer_len_min)
									&& (buffer_len <= buffer_len_max));
					tx_sched->frames_buffer_len = period_len;
				}
			}
		}
	}
	return err;
}

static int set_swparams(struct tx_sched_alsa *tx_sched)
{
	int err;
	// Get the current swparams
	err = snd_pcm_sw_params_current(tx_sched->snd_pcm_handle, tx_sched->swparams);
	assert(err >= 0);
	// start the transfer when a period is full
	err = snd_pcm_sw_params_set_start_threshold(tx_sched->snd_pcm_handle, tx_sched->swparams,
			tx_sched->frames_buffer_len);
	if (err >= 0)
	{
		// Allow the transfer when at least period_size frames can be processed
		err = snd_pcm_sw_params_set_avail_min(tx_sched->snd_pcm_handle, tx_sched->swparams,
				tx_sched->frames_buffer_len);
		// Update the parameters
		if (err >= 0)
			err = snd_pcm_sw_params(tx_sched->snd_pcm_handle, tx_sched->swparams);
	}
	return err;
}

void tx_sched_alsa_release(struct tx_sched_alsa *tx_sched)
{
	if (tx_sched->frames_buffer)
		free(tx_sched->frames_buffer);
	snd_pcm_sw_params_free(tx_sched->swparams);
	snd_pcm_hw_params_free(tx_sched->hwparams);
	free(tx_sched);
}

float tx_sched_alsa_get_closest_sampling_rate(struct tx_sched_alsa *tx_sched,
		float requested_sampling_rate)
{
	unsigned int sampling_rate_min;
	unsigned int sampling_rate_max;
	assert(tx_sched->snd_pcm_handle == NULL);
	int ret = snd_pcm_open(&tx_sched->snd_pcm_handle, adc_tx_device, SND_PCM_STREAM_PLAYBACK, 0);
	if (ret >= 0)
	{
		ret = snd_pcm_hw_params_any(tx_sched->snd_pcm_handle, tx_sched->hwparams);
		if (ret >= 0)
		{
			int dir;
			ret = snd_pcm_hw_params_get_rate_min(tx_sched->hwparams, &sampling_rate_min, &dir);
			if (ret >= 0)
			{
				ret = snd_pcm_hw_params_get_rate_max(tx_sched->hwparams, &sampling_rate_max, &dir);
				if (ret >= 0)
				{
					if (requested_sampling_rate < sampling_rate_min)
						requested_sampling_rate = sampling_rate_min;
					else if (requested_sampling_rate > sampling_rate_max)
						requested_sampling_rate = sampling_rate_max;
				}
			}
		}
		snd_pcm_close(tx_sched->snd_pcm_handle);
		tx_sched->snd_pcm_handle = NULL;
	}
	if (ret >= 0)
		return requested_sampling_rate;
	else
		return 0.0;
}

float tx_sched_alsa_get_effective_sampling_rate(struct tx_sched_alsa *tx_sched)
{
	return tx_sched->freq_sampling_sps;
}

int tx_sched_alsa_start(struct tx_sched_alsa *tx_sched)
{
	assert(tx_sched->snd_pcm_handle == NULL);
	int ret = snd_pcm_open(&tx_sched->snd_pcm_handle, adc_tx_device, SND_PCM_STREAM_PLAYBACK, 0);
	if (ret < 0)
		return ret;
	ret = set_hwparams(tx_sched, tx_sched->frames_buffer_len);
	if (ret < 0)
	{
		snd_pcm_close(tx_sched->snd_pcm_handle);
		tx_sched->snd_pcm_handle = NULL;
		libplc_cape_set_error_msg("Error when configuring ALSA player 'hwparams': %s",
				strerror(-ret));
		return ret;
	}
	ret = set_swparams(tx_sched);
	if (ret < 0)
	{
		snd_pcm_close(tx_sched->snd_pcm_handle);
		tx_sched->snd_pcm_handle = NULL;
		libplc_cape_set_error_msg("Error when configuring ALSA player 'swparams': %s",
				strerror(-ret));
		return ret;
	}
	uint32_t period_bytes = snd_pcm_frames_to_bytes(tx_sched->snd_pcm_handle,
			tx_sched->frames_buffer_len);
	tx_sched->frames_buffer = (uint16_t*) malloc(period_bytes);
	assert(tx_sched->frames_buffer != NULL);
#ifdef VERBOSE
	// Print log
	char *log_text;
	size_t log_text_len;
	FILE *log_file = open_memstream(&log_text, &log_text_len);
	assert(log_file != NULL);
	snd_output_t *log;
	ret = snd_output_stdio_attach(&log, log_file, 0);
	if (ret >= 0)
	{
		snd_pcm_dump(tx_sched->snd_pcm_handle, log);
		snd_output_close(log);
	}
	fclose(log_file);
	libplc_cape_log_line(log_text);
	free(log_text);
#endif
	return 0;
}

void tx_sched_alsa_stop(struct tx_sched_alsa *tx_sched)
{
	int ret = snd_pcm_close(tx_sched->snd_pcm_handle);
	assert(ret >= 0);
	tx_sched->snd_pcm_handle = NULL;
	if (tx_sched->frames_buffer)
	{
		free(tx_sched->frames_buffer);
		tx_sched->frames_buffer = NULL;
	}
}

void tx_sched_alsa_fill_next_buffer(struct tx_sched_alsa *tx_sched)
{
	tx_sched->tx_fill_cycle_callback(tx_sched->tx_fill_cycle_callback_handle,
			tx_sched->frames_buffer, tx_sched->frames_buffer_len);
}

uint16_t *tx_sched_alsa_get_address_buffer_in_tx(struct tx_sched_alsa *tx_sched)
{
	return tx_sched->frames_buffer;
}

static int write_buffer(snd_pcm_t *handle, sample_tx_t *buffer, uint32_t buffer_len)
{
	int ret;
	while (buffer_len > 0)
	{
		ret = snd_pcm_writei(handle, buffer, buffer_len);
		if (ret == -EAGAIN)
			continue;
		assert(ret >= 0);
		// To convert frames sent to bytes we can use:
		//	snd_pcm_frames_to_bytes(handle, err)
		buffer += ret;
		buffer_len -= ret;
	}
	return 0;
}

#if AFE_DAC_BITS > 16
#error DAC resolution must be below 16-bits
#endif
void tx_sched_alsa_flush_and_wait_buffer(struct tx_sched_alsa *tx_sched)
{
	int i;
	for (i = 0; i < tx_sched->frames_buffer_len; i++)
		tx_sched->frames_buffer[i] = (tx_sched->frames_buffer[i] << (16 - AFE_DAC_BITS)) - 0x8000;
	write_buffer(tx_sched->snd_pcm_handle, tx_sched->frames_buffer, tx_sched->frames_buffer_len);
}

ATTR_INTERN struct tx_sched_alsa *tx_sched_alsa_create(struct plc_tx_sched_api *api,
		tx_fill_cycle_callback_t tx_fill_cycle_callback,
		tx_fill_cycle_callback_h tx_fill_cycle_callback_handle, uint32_t buffers_len,
		float freq_sampling_sps)
{
	CHECK_INTERFACE_MEMBERS_COUNT(plc_tx_sched_api, 8);
	struct tx_sched_alsa *tx_sched_alsa = calloc(1, sizeof(struct tx_sched_alsa));
	set_dummy_functions(api, sizeof(*api));
	api->tx_sched_release = tx_sched_alsa_release;
	api->tx_sched_get_effective_sampling_rate = tx_sched_alsa_get_effective_sampling_rate;
	api->tx_sched_start = tx_sched_alsa_start;
	api->tx_sched_request_stop = dummy;
	api->tx_sched_stop = tx_sched_alsa_stop;
	api->tx_sched_fill_next_buffer = tx_sched_alsa_fill_next_buffer;
	api->tx_sched_get_address_buffer_in_tx = tx_sched_alsa_get_address_buffer_in_tx;
	api->tx_sched_flush_and_wait_buffer = tx_sched_alsa_flush_and_wait_buffer;
	tx_sched_alsa->tx_fill_cycle_callback = tx_fill_cycle_callback;
	tx_sched_alsa->tx_fill_cycle_callback_handle = tx_fill_cycle_callback_handle;
	tx_sched_alsa->frames_buffer = NULL;
	tx_sched_alsa->frames_buffer_len = buffers_len;
	tx_sched_alsa->snd_pcm_handle = NULL;
	snd_pcm_hw_params_malloc(&tx_sched_alsa->hwparams);
	snd_pcm_sw_params_malloc(&tx_sched_alsa->swparams);
	tx_sched_alsa->freq_sampling_sps = tx_sched_alsa_get_closest_sampling_rate(tx_sched_alsa,
			freq_sampling_sps);
	return tx_sched_alsa;
}

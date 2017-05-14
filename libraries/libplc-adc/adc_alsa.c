/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

// NOTE: The code in this file requires the '-lasound' library
#define _GNU_SOURCE			// Required for proper declaration of 'pthread_timedjoin_np'
#include <alsa/asoundlib.h>
#include <byteswap.h>		// bswap_16
#include <pthread.h>
#include "+common/api/+base.h"
#define PLC_ADC_HANDLE_EXPLICIT_DEF
typedef struct plc_adc *plc_adc_h;
#include "adc.h"

#define VERBOSE

// Notes about ALSA API:
// * ALSA allows to specify the size of the buffer used on kernel DMA management
// * For better latency the buffer is split on 'periods' (so it's the transfer-unit)
// * A period stores frames which contain the samples captured at one point in time (so it's the
//   capture-unit). For example, in a stereo device the frame would contain samples for two channels
// More info:
//	http://www.linuxjournal.com/article/6735?page=0,1
#define _(text) text

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define COMPOSE_ID(a,b,c,d)	((a) | ((b)<<8) | ((c)<<16) | ((d)<<24))
#define LE_SHORT(v)		(v)
#define LE_INT(v)		(v)
#define BE_SHORT(v)		bswap_16(v)
#define BE_INT(v)		bswap_32(v)
#else /* __BIG_ENDIAN */
#define COMPOSE_ID(a,b,c,d)	((d) | ((c)<<8) | ((b)<<16) | ((a)<<24))
#define LE_SHORT(v)		bswap_16(v)
#define LE_INT(v)		bswap_32(v)
#define BE_SHORT(v)		(v)
#define BE_INT(v)		(v)
#endif

static const char *adc_capturing_device = "default";

struct plc_adc
{
	float freq_capture_sps;
	uint16_t *frames_buffer;
	uint32_t frames_buffer_len;
	snd_pcm_t *snd_pcm_handle;
	snd_pcm_hw_params_t *hwparams;
	snd_pcm_sw_params_t *swparams;
	rx_buffer_completed_callback_t rx_buffer_completed_callback;
	void *rx_buffer_completed_callback_data;
	pthread_t thread;
	volatile int end_thread;
	int capture_started;
};

int adc_set_hwparams(struct plc_adc *plc_adc, snd_pcm_uframes_t period_len)
{
	// Choose all parameters
	int err = snd_pcm_hw_params_any(plc_adc->snd_pcm_handle, plc_adc->hwparams);
	if (err >= 0)
		err = snd_pcm_hw_params_set_access(plc_adc->snd_pcm_handle, plc_adc->hwparams,
				SND_PCM_ACCESS_RW_INTERLEAVED);
	// Although the 'SND_PCM_FORMAT_U16' is an available format it is not accepted on some PCs
	if (err >= 0)
		err = snd_pcm_hw_params_set_format(plc_adc->snd_pcm_handle, plc_adc->hwparams,
				SND_PCM_FORMAT_S16);
	if (err >= 0)
		err = snd_pcm_hw_params_set_channels(plc_adc->snd_pcm_handle, plc_adc->hwparams, 1);
	if (err >= 0)
		err = snd_pcm_hw_params_set_rate(plc_adc->snd_pcm_handle, plc_adc->hwparams,
				(uint32_t) plc_adc->freq_capture_sps, 0);
	if (err >= 0)
	{
		// Buffers
		snd_pcm_uframes_t period_len_min;
		snd_pcm_uframes_t period_len_max;
		snd_pcm_uframes_t buffer_len_min;
		snd_pcm_uframes_t buffer_len_max;
		err = snd_pcm_hw_params_get_buffer_size_min(plc_adc->hwparams, &buffer_len_min);
		assert(err == 0);
		err = snd_pcm_hw_params_get_buffer_size_max(plc_adc->hwparams, &buffer_len_max);
		assert(err == 0);
		err = snd_pcm_hw_params_get_period_size_min(plc_adc->hwparams, &period_len_min,
		NULL);
		assert(err == 0);
		err = snd_pcm_hw_params_get_period_size_max(plc_adc->hwparams, &period_len_max,
		NULL);
		assert(err == 0);
		// Default periods = 4 as a reasonable value
		unsigned int nperiods = 4;
		err = snd_pcm_hw_params_set_periods_near(plc_adc->snd_pcm_handle, plc_adc->hwparams,
				&nperiods, NULL);
		if (err >= 0)
		{
			snd_pcm_uframes_t buffer_len = period_len * nperiods;
			err = snd_pcm_hw_params_set_buffer_size_near(plc_adc->snd_pcm_handle, plc_adc->hwparams,
					&buffer_len);
			if (err >= 0)
			{
				// Update the parameters
				err = snd_pcm_hw_params(plc_adc->snd_pcm_handle, plc_adc->hwparams);
				if (err >= 0)
				{
					err = snd_pcm_hw_params_get_buffer_size(plc_adc->hwparams, &buffer_len);
					assert(err >= 0);
					err = snd_pcm_hw_params_get_period_size(plc_adc->hwparams, &period_len,
					NULL);
					assert(
							(&err >= 0) && (period_len >= period_len_min)
									&& (period_len <= period_len_max)
									&& (buffer_len >= buffer_len_min)
									&& (buffer_len <= buffer_len_max));
					// To log some info uncomment these lines
					//	printf(_("Periods = %u\n"), nperiods);
					//	printf(_("was set period_size = %lu\n"), period_len);
					//	printf(_("was set buffer_size = %lu\n"), buffer_len);
					plc_adc->frames_buffer_len = period_len;
				}
			}
		}
	}
	return err;
}

int adc_set_swparams(struct plc_adc *plc_adc)
{
	// Get the current swparams
	int err = snd_pcm_sw_params_current(plc_adc->snd_pcm_handle, plc_adc->swparams);
	// start the transfer when a period is full
	if (err >= 0)
	{
		err = snd_pcm_sw_params_set_start_threshold(plc_adc->snd_pcm_handle, plc_adc->swparams,
				plc_adc->frames_buffer_len);
		if (err >= 0)
		{
			// Allow the transfer when at least period_size frames can be processed
			err = snd_pcm_sw_params_set_avail_min(plc_adc->snd_pcm_handle, plc_adc->swparams,
					plc_adc->frames_buffer_len);
			// Update the parameters
			if (err >= 0)
				err = snd_pcm_sw_params(plc_adc->snd_pcm_handle, plc_adc->swparams);
		}
	}
	return err;
}

float adc_get_sampling_frequency(struct plc_adc *plc_adc)
{
	return plc_adc->freq_capture_sps;
}

void adc_set_rx_buffer_completed_callback(struct plc_adc *plc_adc,
		rx_buffer_completed_callback_t rx_buffer_completed_callback,
		void *rx_buffer_completed_callback_data)
{
	plc_adc->rx_buffer_completed_callback = rx_buffer_completed_callback;
	plc_adc->rx_buffer_completed_callback_data = rx_buffer_completed_callback_data;
}

uint16_t adc_read_sample(struct plc_adc *plc_adc)
{
	// TODO: To be implemented
	return 0;
}

#if ADC_BITS > 16
#error Current implementation only accepts ADC samples <= 16-bits
#endif
void *adc_thread_capture_samples(void *arg)
{
	// If writing to file do it first to a memory buffer to minimize the impact on captures
	assert(ADC_BITS <= 16);
	struct plc_adc *plc_adc = (struct plc_adc *) arg;
	while (!plc_adc->end_thread)
	{
		ssize_t frames_captured = snd_pcm_readi(plc_adc->snd_pcm_handle, plc_adc->frames_buffer,
				plc_adc->frames_buffer_len);
		snd_pcm_wait(plc_adc->snd_pcm_handle, 100);
		assert((uint32_t )frames_captured == plc_adc->frames_buffer_len);
		// TODO: Accept signed samples instead of converting them to unsigned
		uint16_t *samples = plc_adc->frames_buffer;
		uint32_t samples_captured = plc_adc->frames_buffer_len;
		for (; samples_captured > 0; samples_captured--, samples++)
			*samples = ((uint16_t) ((int16_t) *samples + 0x8000)) >> (16 - ADC_BITS);
		if (plc_adc->rx_buffer_completed_callback)
			plc_adc->rx_buffer_completed_callback(plc_adc->rx_buffer_completed_callback_data,
					plc_adc->frames_buffer, plc_adc->frames_buffer_len);
	}
	return NULL;
}

int adc_start_capture(struct plc_adc *plc_adc, uint32_t buffer_samples, int kernel_buffering,
		float freq_capture_sps)
{
	plc_adc->freq_capture_sps = freq_capture_sps;
	int ret = snd_pcm_open(&plc_adc->snd_pcm_handle, adc_capturing_device, SND_PCM_STREAM_CAPTURE,
			0);
	if (ret < 0)
		return ret;
	ret = adc_set_hwparams(plc_adc, buffer_samples);
	if (ret < 0)
	{
		snd_pcm_close(plc_adc->snd_pcm_handle);
		// TODO: Enable error on libplc_adc
		// libplc_adc_set_error_msg("Error when configuring ALSA recorder 'hwparams': %s",
		//		strerror(-ret));
		return ret;
	}
	ret = adc_set_swparams(plc_adc);
	if (ret < 0)
	{
		snd_pcm_close(plc_adc->snd_pcm_handle);
		// TODO: Enable error on libplc_adc
		// libplc_adc_set_error_msg("Error when configuring ALSA recorder 'swparams': %s",
		//		strerror(-ret));
		return ret;
	}
	uint32_t period_bytes = snd_pcm_frames_to_bytes(plc_adc->snd_pcm_handle,
			plc_adc->frames_buffer_len);
	plc_adc->frames_buffer = (uint16_t*) malloc(period_bytes);
	assert(plc_adc->frames_buffer != NULL);
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
		snd_pcm_dump(plc_adc->snd_pcm_handle, log);
		snd_output_close(log);
	}
	fclose(log_file);
	// TODO: Enable logging on 'libplc_adc'
	// libplc_adc_log_line(log_text);
	free(log_text);
#endif
	plc_adc->capture_started = 1;
	plc_adc->end_thread = 0;
	ret = pthread_create(&plc_adc->thread, NULL, adc_thread_capture_samples, plc_adc);
	assert(ret == 0);
	return ret;
}

void adc_stop_capture(struct plc_adc *plc_adc)
{
	int ret;
	assert(plc_adc->capture_started);
	plc_adc->end_thread = 1;
	struct timespec timeout;
	ret = clock_gettime(CLOCK_REALTIME, &timeout);
	assert(ret == 0);
	timeout.tv_sec += THREAD_TIMEOUT_SECONDS;
	ret = pthread_timedjoin_np(plc_adc->thread, NULL, &timeout);
	assert(ret == 0);
	ret = snd_pcm_close(plc_adc->snd_pcm_handle);
	assert(ret >= 0);
	if (plc_adc->frames_buffer)
	{
		free(plc_adc->frames_buffer);
		plc_adc->frames_buffer = NULL;
	}
	plc_adc->capture_started = 0;
}

void adc_release(struct plc_adc *plc_adc)
{
	if (plc_adc->capture_started)
		adc_stop_capture(plc_adc);
	snd_pcm_sw_params_free(plc_adc->swparams);
	snd_pcm_hw_params_free(plc_adc->hwparams);
	free(plc_adc);
}

ATTR_INTERN struct plc_adc *plc_adc_alsa_create(struct plc_adc_api *api)
{
	struct plc_adc *plc_adc = calloc(1, sizeof(struct plc_adc));
	// set_dummy_functions(api, sizeof(*api));
	api->release = adc_release;
	api->get_sampling_frequency = adc_get_sampling_frequency;
	api->set_rx_buffer_completed_callback = adc_set_rx_buffer_completed_callback;
	api->read_sample = adc_read_sample;
	api->start_capture = adc_start_capture;
	api->stop_capture = adc_stop_capture;
	plc_adc->frames_buffer = NULL;
	plc_adc->frames_buffer_len = 0;
	plc_adc->snd_pcm_handle = NULL;
	snd_pcm_hw_params_malloc(&plc_adc->hwparams);
	snd_pcm_sw_params_malloc(&plc_adc->swparams);
	return plc_adc;
}

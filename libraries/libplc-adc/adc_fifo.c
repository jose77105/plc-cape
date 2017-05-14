/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#define _GNU_SOURCE		// Required for proper declaration of 'pthread_timedjoin_np'
#include <errno.h>		// errno.h, EAGAIN
#include <fcntl.h>		// open
#include <pthread.h>
#include <unistd.h>		// read

#include "+common/api/+base.h"
#define PLC_ADC_HANDLE_EXPLICIT_DEF
typedef struct plc_adc *plc_adc_h;
#include "adc.h"

struct plc_adc
{
	float freq_capture_sps;
	sample_rx_t *buffer;
	uint32_t buffer_len;
	rx_buffer_completed_callback_t rx_buffer_completed_callback;
	void *rx_buffer_completed_callback_data;
	pthread_t thread;
	volatile int end_thread;
	int capture_started;
	int fifo;
};

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

void *adc_thread_capture_samples(void *arg)
{
	struct plc_adc *plc_adc = (struct plc_adc *) arg;
	if (plc_adc->fifo == -1)
	{
		plc_adc->fifo = open(TXRX_SHARED_FIFO_PATH, O_RDONLY);
		assert(plc_adc->fifo != -1);
	}
	while (!plc_adc->end_thread)
	{
		uint8_t *buffer = (uint8_t *) plc_adc->buffer;
		uint32_t bytes_to_read = plc_adc->buffer_len * sizeof(sample_rx_t);
		int ret = 0;
		while (bytes_to_read > 0)
		{
			ret = read(plc_adc->fifo, buffer, bytes_to_read);
			if (ret == -1)
			{
				if (errno == EAGAIN)
				{
					continue;
				}
				else
				{
					assert(errno == EBADF);
					break;
				}
			}
			buffer += ret;
			bytes_to_read -= ret;
		}
		if (ret < 0)
			break;
		if (plc_adc->rx_buffer_completed_callback)
			plc_adc->rx_buffer_completed_callback(plc_adc->rx_buffer_completed_callback_data,
					plc_adc->buffer, plc_adc->buffer_len);
	}
	return NULL;
}

int adc_start_capture(struct plc_adc *plc_adc, uint32_t buffer_samples, int kernel_buffering, float freq_capture_sps)
{
	assert(plc_adc->buffer == NULL);
	plc_adc->freq_capture_sps = freq_capture_sps;
	plc_adc->buffer = (sample_rx_t *) malloc(buffer_samples * sizeof(sample_rx_t));
	plc_adc->buffer_len = buffer_samples;
	plc_adc->capture_started = 1;
	plc_adc->end_thread = 0;
	int ret = pthread_create(&plc_adc->thread, NULL, adc_thread_capture_samples, plc_adc);
	assert(ret == 0);
	return 0;
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
	if (plc_adc->buffer)
	{
		free(plc_adc->buffer);
		plc_adc->buffer = NULL;
	}
	// TODO: Try to close here the fifo without breaking the pipe with 'tx_sched_fifo'
	//	if (plc_adc->fifo != -1)
	//	{
	//		close(plc_adc->fifo);
	//		plc_adc->fifo = -1;
	//	}
	plc_adc->capture_started = 0;
}

void adc_release(struct plc_adc *plc_adc)
{
	// Close the fifo at the last moment to don't broke the pipe while some possible transmission
	// in progress
	if (plc_adc->fifo != -1)
		close(plc_adc->fifo);
	if (plc_adc->capture_started)
		adc_stop_capture(plc_adc);
	free(plc_adc);
}

ATTR_INTERN struct plc_adc *plc_adc_fifo_create(struct plc_adc_api *api)
{
	struct plc_adc *plc_adc = calloc(1, sizeof(struct plc_adc));
	// set_dummy_functions(api, sizeof(*api));
	api->release = adc_release;
	api->get_sampling_frequency = adc_get_sampling_frequency;
	api->set_rx_buffer_completed_callback = adc_set_rx_buffer_completed_callback;
	api->read_sample = adc_read_sample;
	api->start_capture = adc_start_capture;
	api->stop_capture = adc_stop_capture;
	plc_adc->buffer = NULL;
	plc_adc->buffer_len = 0;
	plc_adc->fifo = -1;
	return plc_adc;
}

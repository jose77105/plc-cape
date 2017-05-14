/**
 * @file
 * 
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include <pthread.h>
#include <signal.h>		// raise
#include <unistd.h>		// getpagesize
#include "+common/api/+base.h"
#include "api/afe.h"
#include "spi.h"
#define PLC_TX_SCHED_HANDLE_EXPLICIT_DEF
typedef struct tx_sched_async_thread *plc_tx_sched_h;
#include "tx_sched.h"

struct tx_sched_async_thread
{
	tx_fill_cycle_callback_t tx_fill_cycle_callback;
	tx_fill_cycle_callback_h tx_fill_cycle_callback_handle;
	struct spi *spi;
	uint16_t **buffers;
	uint8_t buffers_count;
	uint32_t buffers_len;
	pthread_t thread;
	pthread_mutex_t thread_mutex;
	pthread_cond_t thread_condition;
	uint8_t next_buffer_to_fill;
	uint8_t last_buffer_flushed;
	int buffer_flushed;
	volatile int end_thread;
};

void tx_sched_async_thread_release(struct tx_sched_async_thread *tx_sched)
{
	int ret = pthread_mutex_destroy(&tx_sched->thread_mutex);
	assert(ret == 0);
	ret = pthread_cond_destroy(&tx_sched->thread_condition);
	assert(ret == 0);
	// Both 'malloc' & 'posix_memalign' must be released with 'free'
	int n;
	for (n = 0; n < tx_sched->buffers_count; n++)
		free(tx_sched->buffers[n]);
	free(tx_sched->buffers);
	free(tx_sched);
}

float tx_sched_async_thread_get_effective_sampling_rate(struct tx_sched_async_thread *tx_sched)
{
	return spi_get_sampling_rate_sps(tx_sched->spi);
}

void *tx_sched_async_thread_thread(void *arg)
{
	struct tx_sched_async_thread *tx_sched = arg;
	while (!tx_sched->end_thread)
	{
		tx_sched->last_buffer_flushed++;
		if (tx_sched->last_buffer_flushed == tx_sched->buffers_count)
			tx_sched->last_buffer_flushed = 0;
		spi_transfer_dac_buffer(tx_sched->spi, tx_sched->buffers[tx_sched->last_buffer_flushed],
				tx_sched->buffers_len);
		tx_sched->buffer_flushed = 1;
		pthread_cond_signal(&tx_sched->thread_condition);
	}
	return NULL;
}
;

int tx_sched_async_thread_start(struct tx_sched_async_thread *tx_sched)
{
	// Initial buffer filling: let 1 buffer to be filled in parallel to the tx (for minimum
	//	starting time)
	for (tx_sched->next_buffer_to_fill = 0;
			tx_sched->next_buffer_to_fill < tx_sched->buffers_count - 1;
			tx_sched->next_buffer_to_fill++)
		tx_sched->tx_fill_cycle_callback(tx_sched->tx_fill_cycle_callback_handle,
				tx_sched->buffers[tx_sched->next_buffer_to_fill], tx_sched->buffers_len);
	tx_sched->last_buffer_flushed = tx_sched->next_buffer_to_fill;
	tx_sched->buffer_flushed = 0;
	tx_sched->end_thread = 0;
	int ret = pthread_create(&tx_sched->thread, NULL, tx_sched_async_thread_thread, tx_sched);
	assert(ret == 0);
	return 0;
}

void tx_sched_async_thread_stop(struct tx_sched_async_thread *tx_sched)
{
	tx_sched->end_thread = 1;
	pthread_cond_signal(&tx_sched->thread_condition);
	int ret = pthread_join(tx_sched->thread, NULL);
	assert(ret == 0);
}

uint16_t *tx_sched_async_thread_get_address_buffer_in_tx(struct tx_sched_async_thread *tx_sched)
{
	return tx_sched->buffers[tx_sched->next_buffer_to_fill];
}

// NOTE: 'fill_next_buffer' is expected to be blocks until the whole buffer filled
void tx_sched_async_thread_fill_next_buffer(struct tx_sched_async_thread *tx_sched)
{
	tx_sched->buffer_flushed = 0;
	tx_sched->tx_fill_cycle_callback(tx_sched->tx_fill_cycle_callback_handle,
			tx_sched->buffers[tx_sched->next_buffer_to_fill], tx_sched->buffers_len);
	tx_sched->next_buffer_to_fill++;
	if (tx_sched->next_buffer_to_fill == tx_sched->buffers_count)
		tx_sched->next_buffer_to_fill = 0;
}

void tx_sched_async_thread_flush_and_wait_buffer(struct tx_sched_async_thread *tx_sched)
{
	pthread_mutex_lock(&tx_sched->thread_mutex);
	while (!tx_sched->end_thread && (tx_sched->buffer_flushed == 0))
		pthread_cond_wait(&tx_sched->thread_condition, &tx_sched->thread_mutex);
	pthread_mutex_unlock(&tx_sched->thread_mutex);
	// Wait for the buffer to be sent and then re-synchronize 'next_buffer_to_fill' to properly deal
	//	with missed buffers
	// TODO: Improve for more than 2 buffers
	tx_sched->next_buffer_to_fill = tx_sched->last_buffer_flushed;
}

ATTR_INTERN struct tx_sched_async_thread *tx_sched_async_thread_create(struct plc_tx_sched_api *api,
		tx_fill_cycle_callback_t tx_fill_cycle_callback,
		tx_fill_cycle_callback_h tx_fill_cycle_callback_handle, struct spi *spi,
		uint32_t buffers_len)
{
	CHECK_INTERFACE_MEMBERS_COUNT(plc_tx_sched_api, 8);
	struct tx_sched_async_thread *tx_sched_async_thread = calloc(1,
			sizeof(struct tx_sched_async_thread));
	set_dummy_functions(api, sizeof(*api));
	api->tx_sched_release = tx_sched_async_thread_release;
	api->tx_sched_get_effective_sampling_rate = tx_sched_async_thread_get_effective_sampling_rate;
	api->tx_sched_start = tx_sched_async_thread_start;
	api->tx_sched_request_stop = dummy;
	api->tx_sched_stop = tx_sched_async_thread_stop;
	api->tx_sched_fill_next_buffer = tx_sched_async_thread_fill_next_buffer;
	api->tx_sched_get_address_buffer_in_tx = tx_sched_async_thread_get_address_buffer_in_tx;
	api->tx_sched_flush_and_wait_buffer = tx_sched_async_thread_flush_and_wait_buffer;
	tx_sched_async_thread->tx_fill_cycle_callback = tx_fill_cycle_callback;
	tx_sched_async_thread->tx_fill_cycle_callback_handle = tx_fill_cycle_callback_handle;
	tx_sched_async_thread->spi = spi;
	tx_sched_async_thread->buffers_len = buffers_len;
	tx_sched_async_thread->buffers_count = 2;
	tx_sched_async_thread->buffers = (uint16_t **) calloc(tx_sched_async_thread->buffers_count,
			sizeof(*tx_sched_async_thread->buffers));
	tx_sched_async_thread->buffers[0] = (uint16_t *) malloc(
			tx_sched_async_thread->buffers_len * sizeof(uint16_t));
	tx_sched_async_thread->buffers[1] = (uint16_t *) malloc(
			tx_sched_async_thread->buffers_len * sizeof(uint16_t));
	int ret = pthread_mutex_init(&tx_sched_async_thread->thread_mutex, NULL);
	assert(ret == 0);
	ret = pthread_cond_init(&tx_sched_async_thread->thread_condition, NULL);
	assert(ret == 0);
	return tx_sched_async_thread;
}

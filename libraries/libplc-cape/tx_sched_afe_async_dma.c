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
typedef struct tx_sched_async_dma *plc_tx_sched_h;
#include "tx_sched.h"

struct tx_sched_async_dma
{
	tx_fill_cycle_callback_t tx_fill_cycle_callback;
	tx_fill_cycle_callback_h tx_fill_cycle_callback_handle;
	struct spi *spi;
	uint16_t **buffers;
	uint8_t buffers_count;
	uint32_t buffers_len;
	uint8_t next_buffer_to_fill;
	uint8_t buffer_in_tx;
};

void tx_sched_async_dma_release(struct tx_sched_async_dma *tx_sched)
{
	// Release DMA buffers
	spi_release_buffers_dma(tx_sched->spi);
	// Both 'malloc' & 'posix_memalign' must be released with 'free'
	int n;
	for (n = 0; n < tx_sched->buffers_count; n++)
		free(tx_sched->buffers[n]);
	free(tx_sched->buffers);
	free(tx_sched);
}

float tx_sched_async_dma_get_effective_sampling_rate(struct tx_sched_async_dma *tx_sched)
{
	return spi_get_sampling_rate_sps(tx_sched->spi);
}

int tx_sched_async_dma_start(struct tx_sched_async_dma *tx_sched)
{
	for (tx_sched->next_buffer_to_fill = 0;
			tx_sched->next_buffer_to_fill < tx_sched->buffers_count - 1;
			tx_sched->next_buffer_to_fill++)
		tx_sched->tx_fill_cycle_callback(tx_sched->tx_fill_cycle_callback_handle,
				tx_sched->buffers[tx_sched->next_buffer_to_fill], tx_sched->buffers_len);
	tx_sched->buffer_in_tx = 0;
	spi_start_dma(tx_sched->spi);
	return 0;
}

void tx_sched_async_dma_stop(struct tx_sched_async_dma *tx_sched)
{
	spi_abort_dma(tx_sched->spi);
}

void tx_sched_async_dma_fill_next_buffer(struct tx_sched_async_dma *tx_sched)
{
	tx_sched->tx_fill_cycle_callback(tx_sched->tx_fill_cycle_callback_handle,
			tx_sched->buffers[tx_sched->next_buffer_to_fill], tx_sched->buffers_len);
	if (++tx_sched->next_buffer_to_fill == tx_sched->buffers_count)
		tx_sched->next_buffer_to_fill = 0;
}

uint16_t *tx_sched_async_dma_get_address_buffer_in_tx(struct tx_sched_async_dma *tx_sched)
{
	return tx_sched->buffers[tx_sched->next_buffer_to_fill];
}

void tx_sched_async_dma_flush_and_wait_buffer(struct tx_sched_async_dma *tx_sched)
{
	tx_sched->buffer_in_tx = spi_wait_dma_buffer_sent(tx_sched->spi);
	// Wait for the buffer to be sent and then re-synchronize 'next_buffer_to_fill' to properly deal
	//	with missed buffers
	if (tx_sched->buffer_in_tx == 0)
		tx_sched->next_buffer_to_fill = tx_sched->buffers_count - 1;
	else
		tx_sched->next_buffer_to_fill = tx_sched->buffer_in_tx - 1;
}

ATTR_INTERN struct tx_sched_async_dma *tx_sched_async_dma_create(struct plc_tx_sched_api *api,
		tx_fill_cycle_callback_t tx_fill_cycle_callback,
		tx_fill_cycle_callback_h tx_fill_cycle_callback_handle, struct spi *spi,
		uint32_t buffers_len)
{
	CHECK_INTERFACE_MEMBERS_COUNT(plc_tx_sched_api, 8);
	struct tx_sched_async_dma *tx_sched_async_dma = calloc(1, sizeof(struct tx_sched_async_dma));
	set_dummy_functions(api, sizeof(*api));
	api->tx_sched_release = tx_sched_async_dma_release;
	api->tx_sched_get_effective_sampling_rate = tx_sched_async_dma_get_effective_sampling_rate;
	api->tx_sched_start = tx_sched_async_dma_start;
	api->tx_sched_request_stop = dummy;
	api->tx_sched_stop = tx_sched_async_dma_stop;
	api->tx_sched_fill_next_buffer = tx_sched_async_dma_fill_next_buffer;
	api->tx_sched_get_address_buffer_in_tx = tx_sched_async_dma_get_address_buffer_in_tx;
	api->tx_sched_flush_and_wait_buffer = tx_sched_async_dma_flush_and_wait_buffer;
	tx_sched_async_dma->tx_fill_cycle_callback = tx_fill_cycle_callback;
	tx_sched_async_dma->tx_fill_cycle_callback_handle = tx_fill_cycle_callback_handle;
	tx_sched_async_dma->spi = spi;
	tx_sched_async_dma->buffers_len = buffers_len;
	tx_sched_async_dma->buffers_count = 2;
	tx_sched_async_dma->buffers = (uint16_t **) calloc(tx_sched_async_dma->buffers_count,
			sizeof(*tx_sched_async_dma->buffers));
	// NOTE: Sharing pages needs memory to be page-aligned (typically 4096 bytes)
	int page_size = getpagesize();
	assert(tx_sched_async_dma->buffers_len * sizeof(uint16_t) <= page_size);
	// Grab chunks of page-aligned memory
	int ret = posix_memalign((void **) &tx_sched_async_dma->buffers[0], page_size, page_size);
	assert((ret == 0) && (tx_sched_async_dma->buffers[0] != NULL));
	ret = posix_memalign((void **) &tx_sched_async_dma->buffers[1], page_size, page_size);
	assert((ret == 0) && (tx_sched_async_dma->buffers[1] != NULL));
	spi_allocate_buffers_dma(tx_sched_async_dma->spi, tx_sched_async_dma->buffers[0],
			tx_sched_async_dma->buffers_len, tx_sched_async_dma->buffers[1],
			tx_sched_async_dma->buffers_len);
	return tx_sched_async_dma;
}

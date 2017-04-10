/**
 * @file
 * 
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include <pthread.h>
#include <signal.h>		// raise
#include <unistd.h>		// getpagesize
#include "+common/api/+base.h"
#include "api/afe.h"
#include "api/tx.h"
#include "spi.h"
#include "tx_sched.h"

// TODO: reuse 'set_dummy_functions' or consider removing it
void dummy()
{
}

void set_dummy_functions(void *struct_ptr, uint32_t struct_size)
{
	// Set all the functions of the interface to a dummy function to avoid a segmentation fault if
	//	not implemented
	int functions_count = struct_size / sizeof(void (*)());
	void **fn_ptr = (void**) struct_ptr;
	for (; functions_count > 0; functions_count--, fn_ptr++)
		*fn_ptr = (void*) dummy;
}

//
// tx_sched_sync
// 

struct tx_sched_sync
{
	// PRECONDITION: The 'tx_sched' interface must be the first member of the struct
	//	This allows using 'tx_sched_sync' and 'tx_sched' intercheangly in a comfortable way
	//	avoiding up-castings everywhere
	struct tx_sched tx_sched;
	tx_fill_cycle_callback_t tx_fill_cycle_callback;
	tx_fill_cycle_callback_h tx_fill_cycle_callback_handle;
	struct spi *spi;
	uint16_t *buffer;
	uint32_t buffers_len;
	int sample_by_sample;
};

void tx_sched_sync_release(struct tx_sched_sync *tx_sched)
{
	free(tx_sched->buffer);
	free(tx_sched);
}

void tx_sched_sync_start(struct tx_sched_sync *tx_sched)
{
}

void tx_sched_sync_stop(struct tx_sched_sync *tx_sched)
{
}

// Final Freq = speed_dac/bits_dac/NUM_SAMPLES
// Si speed_dac = 100kHz y 10 bits_dac -> FinalFreq = 9.7Hz

// Si usamos todo el margen dinámico del DAC es muy fácil que la señal sature a la salida del filtro
//	PGA, incluso con un 'samples_range = MAX_DAC/4'
// PRECONDITION: samples_range debe ser par:
//	samples_min_value = samples_offset-samples_range/2
//	samples_max_value = samples_offset-samples_range/2-1
void tx_sched_sync_fill_next_buffer(struct tx_sched_sync *tx_sched)
{
	tx_sched->tx_fill_cycle_callback(tx_sched->tx_fill_cycle_callback_handle, tx_sched->buffer,
			tx_sched->buffers_len);
}

uint16_t *tx_sched_sync_get_address_buffer_in_tx(struct tx_sched_sync *tx_sched)
{
	return tx_sched->buffer;
}

void tx_sched_sync_flush_and_wait_buffer(struct tx_sched_sync *tx_sched)
{
	// TODO: sample_by_sample has sense only on real-time tx: calculation of next sample and send it
	if (tx_sched->sample_by_sample)
	{
		int index;
		for (index = 0; index < tx_sched->buffers_len; index++)
			spi_transfer_dac_sample(tx_sched->spi, tx_sched->buffer[index]);
	}
	else
	{
		spi_transfer_dac_buffer(tx_sched->spi, tx_sched->buffer, tx_sched->buffers_len);
	}
}

ATTR_EXTERN struct tx_sched *tx_sched_sync_create(tx_fill_cycle_callback_t tx_fill_cycle_callback,
		tx_fill_cycle_callback_h tx_fill_cycle_callback_handle, struct spi *spi,
		uint32_t buffers_len, int sample_by_sample)
{
	struct tx_sched_sync *tx_sched_sync = calloc(1, sizeof(struct tx_sched_sync));
	set_dummy_functions(&tx_sched_sync->tx_sched, sizeof(tx_sched_sync->tx_sched));
	tx_sched_sync->tx_sched.tx_sched_release = (void*) tx_sched_sync_release;
	tx_sched_sync->tx_sched.tx_sched_start = (void*) tx_sched_sync_start;
	tx_sched_sync->tx_sched.tx_sched_stop = (void*) tx_sched_sync_stop;
	tx_sched_sync->tx_sched.tx_sched_fill_next_buffer = (void*) tx_sched_sync_fill_next_buffer;
	tx_sched_sync->tx_sched.tx_sched_get_address_buffer_in_tx =
			(void*) tx_sched_sync_get_address_buffer_in_tx;
	tx_sched_sync->tx_sched.tx_sched_flush_and_wait_buffer =
			(void*) tx_sched_sync_flush_and_wait_buffer;
	tx_sched_sync->tx_fill_cycle_callback = tx_fill_cycle_callback;
	tx_sched_sync->tx_fill_cycle_callback_handle = tx_fill_cycle_callback_handle;
	tx_sched_sync->spi = spi;
	tx_sched_sync->buffers_len = buffers_len;
	tx_sched_sync->sample_by_sample = sample_by_sample;
	tx_sched_sync->buffer = (uint16_t *) malloc(buffers_len * sizeof(uint16_t));
	assert((void* )tx_sched_sync == (void* )&tx_sched_sync->tx_sched);
	return &tx_sched_sync->tx_sched;
}

//
// tx_sched_async_thread
//

struct tx_sched_async_thread
{
	// PRECONDITION: The 'tx_sched' interface must be the first member of the struct
	struct tx_sched tx_sched;
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

void tx_sched_async_thread_start(struct tx_sched_async_thread *tx_sched)
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
}

void tx_sched_async_thread_stop(struct tx_sched_async_thread *tx_sched)
{
	tx_sched->end_thread = 1;
	pthread_cond_signal(&tx_sched->thread_condition);
	int ret = pthread_join(tx_sched->thread, NULL);
	assert(ret == 0);
}

uint16_t *tx_sched_async_thread_get_address_buffer_in_tx(
		struct tx_sched_async_thread *tx_sched)
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

ATTR_EXTERN struct tx_sched *tx_sched_async_thread_create(
		tx_fill_cycle_callback_t tx_fill_cycle_callback,
		tx_fill_cycle_callback_h tx_fill_cycle_callback_handle,
		struct spi *spi, uint32_t buffers_len)
{
	struct tx_sched_async_thread *tx_sched_async_thread = calloc(1,
			sizeof(struct tx_sched_async_thread));
	set_dummy_functions(&tx_sched_async_thread->tx_sched, sizeof(tx_sched_async_thread->tx_sched));
	tx_sched_async_thread->tx_sched.tx_sched_release = (void*) tx_sched_async_thread_release;
	tx_sched_async_thread->tx_sched.tx_sched_start = (void*) tx_sched_async_thread_start;
	tx_sched_async_thread->tx_sched.tx_sched_stop = (void*) tx_sched_async_thread_stop;
	tx_sched_async_thread->tx_sched.tx_sched_fill_next_buffer =
			(void*) tx_sched_async_thread_fill_next_buffer;
	tx_sched_async_thread->tx_sched.tx_sched_get_address_buffer_in_tx =
			(void*) tx_sched_async_thread_get_address_buffer_in_tx;
	tx_sched_async_thread->tx_sched.tx_sched_flush_and_wait_buffer =
			(void*) tx_sched_async_thread_flush_and_wait_buffer;
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
	assert((void* )tx_sched_async_thread == (void* )&tx_sched_async_thread->tx_sched);
	return &tx_sched_async_thread->tx_sched;
}

//
// tx_sched_async_dma
//

struct tx_sched_async_dma
{
	// PRECONDITION: The 'tx_sched' interface must be the first member of the struct
	struct tx_sched tx_sched;
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

void tx_sched_async_dma_start(struct tx_sched_async_dma *tx_sched)
{
	for (tx_sched->next_buffer_to_fill = 0;
			tx_sched->next_buffer_to_fill < tx_sched->buffers_count - 1;
			tx_sched->next_buffer_to_fill++)
		tx_sched->tx_fill_cycle_callback(tx_sched->tx_fill_cycle_callback_handle,
				tx_sched->buffers[tx_sched->next_buffer_to_fill], tx_sched->buffers_len);
	tx_sched->buffer_in_tx = 0;
	spi_start_dma(tx_sched->spi);
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

ATTR_EXTERN struct tx_sched *tx_sched_async_dma_create(
		tx_fill_cycle_callback_t tx_fill_cycle_callback,
		tx_fill_cycle_callback_h tx_fill_cycle_callback_handle,
		struct spi *spi, uint32_t buffers_len)
{
	struct tx_sched_async_dma *tx_sched_async_dma = calloc(1, sizeof(struct tx_sched_async_dma));
	set_dummy_functions(&tx_sched_async_dma->tx_sched, sizeof(tx_sched_async_dma->tx_sched));
	tx_sched_async_dma->tx_sched.tx_sched_release = (void*) tx_sched_async_dma_release;
	tx_sched_async_dma->tx_sched.tx_sched_start = (void*) tx_sched_async_dma_start;
	tx_sched_async_dma->tx_sched.tx_sched_stop = (void*) tx_sched_async_dma_stop;
	tx_sched_async_dma->tx_sched.tx_sched_fill_next_buffer =
			(void*) tx_sched_async_dma_fill_next_buffer;
	tx_sched_async_dma->tx_sched.tx_sched_get_address_buffer_in_tx =
			(void*) tx_sched_async_dma_get_address_buffer_in_tx;
	tx_sched_async_dma->tx_sched.tx_sched_flush_and_wait_buffer =
			(void*) tx_sched_async_dma_flush_and_wait_buffer;
	tx_sched_async_dma->tx_fill_cycle_callback = tx_fill_cycle_callback;
	tx_sched_async_dma->tx_fill_cycle_callback_handle = tx_fill_cycle_callback_handle;
	tx_sched_async_dma->spi = spi;
	tx_sched_async_dma->buffers_len = buffers_len;
	tx_sched_async_dma->buffers_count = 2;
	tx_sched_async_dma->buffers = (uint16_t **) calloc(tx_sched_async_dma->buffers_count,
			sizeof(*tx_sched_async_dma->buffers));
	// NOTE: Sharing pages needs mem to be page-aligned (typically 4096 bytes)
	int page_size = getpagesize();
	assert(tx_sched_async_dma->buffers_len * sizeof(uint16_t) <= page_size);
	// Grab chuncks of page-aligned memory
	int ret = posix_memalign((void **) &tx_sched_async_dma->buffers[0], page_size, page_size);
	assert((ret == 0) && (tx_sched_async_dma->buffers[0] != NULL));
	ret = posix_memalign((void **) &tx_sched_async_dma->buffers[1], page_size, page_size);
	assert((ret == 0) && (tx_sched_async_dma->buffers[1] != NULL));
	spi_allocate_buffers_dma(tx_sched_async_dma->spi, tx_sched_async_dma->buffers[0],
			tx_sched_async_dma->buffers_len, tx_sched_async_dma->buffers[1],
			tx_sched_async_dma->buffers_len);
	assert((void* )tx_sched_async_dma == (void* )&tx_sched_async_dma->tx_sched);
	return &tx_sched_async_dma->tx_sched;
}

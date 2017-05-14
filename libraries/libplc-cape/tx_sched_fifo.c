/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include <errno.h>		// errno, EAGAIN
#include <sys/stat.h>	// mkfifo
#include <fcntl.h>		// open
#include <unistd.h>		// close
#include "+common/api/+base.h"
#include "libraries/libplc-tools/api/time.h"
#define PLC_TX_SCHED_HANDLE_EXPLICIT_DEF
typedef struct tx_sched_fifo *plc_tx_sched_h;
#include "tx_sched.h"

struct tx_sched_fifo
{
	tx_fill_cycle_callback_t tx_fill_cycle_callback;
	tx_fill_cycle_callback_h tx_fill_cycle_callback_handle;
	sample_tx_t *buffer;
	uint32_t buffer_len;
	int fifo;
	float freq_sampling_sps;
	float buffer_time_us;
	struct timespec next_buffer_stamp;
};

void tx_sched_fifo_release(struct tx_sched_fifo *tx_sched)
{
	assert(tx_sched->fifo == -1);
	if (tx_sched->buffer)
		free(tx_sched->buffer);
	free(tx_sched);
	int ret = unlink(TXRX_SHARED_FIFO_PATH);
	assert(ret == 0);
}

float tx_sched_fifo_get_effective_sampling_rate(struct tx_sched_fifo *tx_sched)
{
	return tx_sched->freq_sampling_sps;
}

int tx_sched_fifo_start(struct tx_sched_fifo *tx_sched)
{
	assert(tx_sched->buffer == NULL);
	tx_sched->buffer = malloc(tx_sched->buffer_len * sizeof(sample_tx_t));
	tx_sched->next_buffer_stamp = plc_time_get_hires_stamp();
	return 0;
}

void tx_sched_fifo_request_stop(struct tx_sched_fifo *tx_sched)
{
	if (tx_sched->fifo != -1)
	{
		// Closing the fifo unlocks any pending 'write' returning '-1' and 'errno == EBADF'
		close(tx_sched->fifo);
		tx_sched->fifo = -1;
	}
}

void tx_sched_fifo_stop(struct tx_sched_fifo *tx_sched)
{
	assert(tx_sched->fifo == -1);
	free(tx_sched->buffer);
	tx_sched->buffer = NULL;
}

void tx_sched_fifo_fill_next_buffer(struct tx_sched_fifo *tx_sched)
{
	tx_sched->tx_fill_cycle_callback(tx_sched->tx_fill_cycle_callback_handle, tx_sched->buffer,
			tx_sched->buffer_len);
}

uint16_t *tx_sched_fifo_get_address_buffer_in_tx(struct tx_sched_fifo *tx_sched)
{
	return tx_sched->buffer;
}

void tx_sched_fifo_flush_and_wait_buffer(struct tx_sched_fifo *tx_sched)
{
	if (tx_sched->fifo == -1)
	{
		// Note: 'open' blocks until a sink connected
		tx_sched->fifo = open(TXRX_SHARED_FIFO_PATH, O_WRONLY | O_TRUNC);
		assert(tx_sched->fifo != -1);
	}
	uint8_t *buffer = (uint8_t *) tx_sched->buffer;
	uint32_t bytes_to_write = tx_sched->buffer_len * sizeof(sample_tx_t);
	// TODO: To simulate a stable SPI baud rate add a timer or cumulative sleep
	struct timespec stamp = plc_time_get_hires_stamp();
	int32_t delay_to_next_buffer_us = plc_time_hires_interval_to_usec(stamp,
			tx_sched->next_buffer_stamp);
	if (delay_to_next_buffer_us > 0)
		usleep(delay_to_next_buffer_us);
	plc_time_add_usec_to_hires_interval(&tx_sched->next_buffer_stamp, tx_sched->buffer_time_us);
	while (bytes_to_write > 0)
	{
		int ret = write(tx_sched->fifo, buffer, bytes_to_write);
		if (ret == -1)
		{
			if (errno == EAGAIN)
			{
				continue;
			}
			else
			{
				// EBADF is produced if the tx_sched->fifo is closed on the tx side (here)
				// EPIPE is produced if the tx_sched->fifo is closed on the rx side
				assert((errno == EBADF) || (errno == EPIPE));
				break;
			}
		}
		buffer += ret;
		bytes_to_write -= ret;
	}
}

ATTR_INTERN struct tx_sched_fifo *tx_sched_fifo_create(struct plc_tx_sched_api *api,
		tx_fill_cycle_callback_t tx_fill_cycle_callback,
		tx_fill_cycle_callback_h tx_fill_cycle_callback_handle, uint32_t buffers_len,
		float freq_sampling_sps)
{
	CHECK_INTERFACE_MEMBERS_COUNT(plc_tx_sched_api, 8);
	// TODO: The shared pipe should be created elsewhere (plc-cape-lab?)
	//	'tx_sched_fifo' and 'adc_fifo' should assume proper shared pipe existence
	// Create the fifo (clean previous results if any)
	if (access(TXRX_SHARED_FIFO_PATH, F_OK) != -1)
		unlink(TXRX_SHARED_FIFO_PATH);
	int ret = mkfifo(TXRX_SHARED_FIFO_PATH, S_IRUSR | S_IWUSR);
	assert(ret == 0);
	struct tx_sched_fifo *tx_sched_fifo = calloc(1, sizeof(struct tx_sched_fifo));
	set_dummy_functions(api, sizeof(*api));
	api->tx_sched_release = tx_sched_fifo_release;
	api->tx_sched_get_effective_sampling_rate = tx_sched_fifo_get_effective_sampling_rate;
	api->tx_sched_start = tx_sched_fifo_start;
	api->tx_sched_request_stop = tx_sched_fifo_request_stop;
	api->tx_sched_stop = tx_sched_fifo_stop;
	api->tx_sched_fill_next_buffer = tx_sched_fifo_fill_next_buffer;
	api->tx_sched_get_address_buffer_in_tx = tx_sched_fifo_get_address_buffer_in_tx;
	api->tx_sched_flush_and_wait_buffer = tx_sched_fifo_flush_and_wait_buffer;
	tx_sched_fifo->tx_fill_cycle_callback = tx_fill_cycle_callback;
	tx_sched_fifo->tx_fill_cycle_callback_handle = tx_fill_cycle_callback_handle;
	tx_sched_fifo->buffer = NULL;
	tx_sched_fifo->buffer_len = buffers_len;
	tx_sched_fifo->fifo = -1;
	tx_sched_fifo->freq_sampling_sps = freq_sampling_sps;
	tx_sched_fifo->buffer_time_us = (buffers_len / freq_sampling_sps) * 1000000.0f;
	return tx_sched_fifo;
}

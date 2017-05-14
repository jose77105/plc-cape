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
typedef struct tx_sched_sync *plc_tx_sched_h;
#include "tx_sched.h"

struct tx_sched_sync
{
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

float tx_sched_sync_get_effective_sampling_rate(struct tx_sched_sync *tx_sched)
{
	return spi_get_sampling_rate_sps(tx_sched->spi);
}

int tx_sched_sync_start(struct tx_sched_sync *tx_sched)
{
	return 0;
}

void tx_sched_sync_stop(struct tx_sched_sync *tx_sched)
{
}

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

ATTR_INTERN struct tx_sched_sync *tx_sched_sync_create(struct plc_tx_sched_api *api,
		tx_fill_cycle_callback_t tx_fill_cycle_callback,
		tx_fill_cycle_callback_h tx_fill_cycle_callback_handle, struct spi *spi,
		uint32_t buffers_len, int sample_by_sample)
{
	CHECK_INTERFACE_MEMBERS_COUNT(plc_tx_sched_api, 8);
	struct tx_sched_sync *tx_sched_sync = calloc(1, sizeof(struct tx_sched_sync));
	set_dummy_functions(api, sizeof(*api));
	api->tx_sched_release = tx_sched_sync_release;
	api->tx_sched_get_effective_sampling_rate = tx_sched_sync_get_effective_sampling_rate;
	api->tx_sched_start = tx_sched_sync_start;
	api->tx_sched_request_stop = dummy;
	api->tx_sched_stop = tx_sched_sync_stop;
	api->tx_sched_fill_next_buffer = tx_sched_sync_fill_next_buffer;
	api->tx_sched_get_address_buffer_in_tx =
			tx_sched_sync_get_address_buffer_in_tx;
	api->tx_sched_flush_and_wait_buffer =
			tx_sched_sync_flush_and_wait_buffer;
	tx_sched_sync->tx_fill_cycle_callback = tx_fill_cycle_callback;
	tx_sched_sync->tx_fill_cycle_callback_handle = tx_fill_cycle_callback_handle;
	tx_sched_sync->spi = spi;
	tx_sched_sync->buffers_len = buffers_len;
	tx_sched_sync->sample_by_sample = sample_by_sample;
	tx_sched_sync->buffer = (uint16_t *) malloc(buffers_len * sizeof(uint16_t));
	return tx_sched_sync;
}

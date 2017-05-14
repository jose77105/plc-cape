/**
 * @file
 * @brief	Transmission scheduling modes
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef TX_SCHED_H
#define TX_SCHED_H

#include "api/tx.h"

#define THREAD_TIMEOUT_SECONDS 5

#ifndef PLC_TX_SCHED_HANDLE_EXPLICIT_DEF
typedef void *plc_tx_sched_h;
#endif

struct spi;

void dummy();
void set_dummy_functions(void *struct_ptr, uint32_t struct_size);

struct plc_tx_sched_api
{
	void (*tx_sched_release)(plc_tx_sched_h handle);
	float (*tx_sched_get_effective_sampling_rate)(plc_tx_sched_h handle);
	int (*tx_sched_start)(plc_tx_sched_h handle);
	void (*tx_sched_request_stop)(plc_tx_sched_h handle);
	void (*tx_sched_stop)(plc_tx_sched_h handle);
	void (*tx_sched_fill_next_buffer)(plc_tx_sched_h handle);
	uint16_t *(*tx_sched_get_address_buffer_in_tx)(plc_tx_sched_h handle);
	void (*tx_sched_flush_and_wait_buffer)(plc_tx_sched_h handle);
};

plc_tx_sched_h tx_sched_sync_create(struct plc_tx_sched_api *api,
		tx_fill_cycle_callback_t tx_fill_cycle_callback,
		tx_fill_cycle_callback_h tx_fill_cycle_callback_handle, struct spi *spi,
		uint32_t buffers_len, int sample_by_sample);
plc_tx_sched_h tx_sched_async_thread_create(struct plc_tx_sched_api *api,
		tx_fill_cycle_callback_t tx_fill_cycle_callback,
		tx_fill_cycle_callback_h tx_fill_cycle_callback_handle, struct spi *spi,
		uint32_t buffers_len);
plc_tx_sched_h tx_sched_async_dma_create(struct plc_tx_sched_api *api,
		tx_fill_cycle_callback_t tx_fill_cycle_callback,
		tx_fill_cycle_callback_h tx_fill_cycle_callback_handle, struct spi *spi,
		uint32_t buffers_len);

plc_tx_sched_h tx_sched_alsa_create(struct plc_tx_sched_api *api,
		tx_fill_cycle_callback_t tx_fill_cycle_callback,
		tx_fill_cycle_callback_h tx_fill_cycle_callback_handle, uint32_t buffers_len,
		float freq_sampling_sps);

plc_tx_sched_h tx_sched_fifo_create(struct plc_tx_sched_api *api,
		tx_fill_cycle_callback_t tx_fill_cycle_callback,
		tx_fill_cycle_callback_h tx_fill_cycle_callback_handle, uint32_t buffers_len,
		float freq_sampling_sps);

#endif /* TX_SCHED_H */

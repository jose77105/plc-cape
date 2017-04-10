/**
 * @file
 * @brief	Transmission scheduling modes
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef TX_SCHED_H
#define TX_SCHED_H

struct tx_sched
{
	void (*tx_sched_release)(struct tx_sched *tx_sched);
	void (*tx_sched_start)(struct tx_sched *tx_sched);
	void (*tx_sched_stop)(struct tx_sched *tx_sched);
	void (*tx_sched_fill_next_buffer)(struct tx_sched *tx_sched);
	uint16_t *(*tx_sched_get_address_buffer_in_tx)(struct tx_sched *tx_sched);
	void (*tx_sched_flush_and_wait_buffer)(struct tx_sched *tx_sched);
};

struct spi;

struct tx_sched *tx_sched_sync_create(tx_fill_cycle_callback_t tx_fill_cycle_callback,
		tx_fill_cycle_callback_h tx_fill_cycle_callback_handle, struct spi *spi,
		uint32_t buffers_len, int sample_by_sample);
struct tx_sched *tx_sched_async_thread_create(
		tx_fill_cycle_callback_t tx_fill_cycle_callback,
		tx_fill_cycle_callback_h tx_fill_cycle_callback_handle, struct spi *spi,
		uint32_t buffers_len);
struct tx_sched *tx_sched_async_dma_create(
		tx_fill_cycle_callback_t tx_fill_cycle_callback,
		tx_fill_cycle_callback_h tx_fill_cycle_callback_handle, struct spi *spi,
		uint32_t buffers_len);

#endif /* TX_SCHED_H */

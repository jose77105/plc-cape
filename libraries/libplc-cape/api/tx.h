/**
 * @file
 * @brief	Helper functions for data transmission
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef LIBPLC_CAPE_TX_H
#define LIBPLC_CAPE_TX_H

extern const char *spi_tx_mode_enum_text[];
enum spi_tx_mode_enum
{
	spi_tx_mode_none = 0,
	spi_tx_mode_sample_by_sample,
	spi_tx_mode_buffer_by_buffer,
	spi_tx_mode_ping_pong_thread,
	spi_tx_mode_ping_pong_dma,
	spi_tx_mode_COUNT
};

struct tx_statistics
{
	uint32_t buffers_handled;
	uint32_t buffers_overflow;
	uint32_t buffer_preparation_us;
	uint32_t buffer_preparation_min_us;
	uint32_t buffer_preparation_max_us;
	uint32_t buffer_cycle_us;
	uint32_t buffer_cycle_min_us;
	uint32_t buffer_cycle_max_us;
};

#ifndef TX_NODEF_FILL_CYCLE_CALLBACK_HANDLE
typedef void *tx_fill_cycle_callback_h;
#endif
typedef void (*tx_fill_cycle_callback_t)(tx_fill_cycle_callback_h tx_fill_cycle_callback_handle,
		uint16_t *buffer, uint32_t buffer_count);

#ifndef TX_NODEF_ON_BUFFER_SENT_CALLBACK_HANDLE
typedef void *tx_on_buffer_sent_callback_h;
#endif
// TODO: Revisar la funcionad de esta función. El TX ya conoce qué ha enviado -> Información del
//	buffer innecesaria
typedef void (*tx_on_buffer_sent_callback_t)(
		tx_on_buffer_sent_callback_h tx_on_buffer_sent_callback_handle, uint16_t *buffer,
		uint32_t buffer_count);

struct plc_afe;
struct plc_tx;

/** 
 * @brief	Creates an object for buffered chained transmission
 * @param	tx_fill_cycle_callback
 *					Function that will be called to prepare the next buffer of samples to be sent
 * @param	tx_fill_cycle_callback_handle
 *					Optional user value that will be sent as the first parameter of
 *					_tx_fill_cycle_callback_
 * @param	tx_on_buffer_sent_callback
 *					Function that will be called after a buffer of data has been sent
 * @param	tx_on_buffer_sent_callback_handle
 *					Optional user value that will be sent as the first parameter of
 *					_tx_on_buffer_sent_callback_
 * @param	tx_mode	The transmission mode to be used. Someones are only available for the custom
 *					SPI driver
 * @param	tx_buffers_len
 *					The length of the buffer
 * @param	plc_afe	A pointer to the plc_afe handler object
 * @return	Pointer to the handler object
 */
struct plc_tx *plc_tx_create(tx_fill_cycle_callback_t tx_fill_cycle_callback,
		tx_fill_cycle_callback_h tx_fill_cycle_callback_handle,
		tx_on_buffer_sent_callback_t tx_on_buffer_sent_callback,
		tx_on_buffer_sent_callback_h tx_on_buffer_sent_callback_handle,
		enum spi_tx_mode_enum tx_mode, uint32_t tx_buffers_len, struct plc_afe *plc_afe);
/**
 * @brief	Gets some statistics related with the transmission
 * @param	plc_tx	Pointer to the handler object
 * @return	A struct with the statistics data
 */
const struct tx_statistics *tx_get_tx_statistics(struct plc_tx *plc_tx);
/**
 * @brief	Releases a handler object
 * @param	plc_tx	Pointer to the handler object
 */
void plc_tx_release(struct plc_tx *plc_tx);
void plc_tx_fill_buffer_iteration(struct plc_tx *plc_tx, uint16_t *buffer, uint32_t buffer_samples);
/**
 * @brief	Starts the standalone transmission process
 * @param	plc_tx	Pointer to the handler object
 */
void plc_tx_start_transmission(struct plc_tx *plc_tx);
/**
 * @brief	Stops the transmission process
 * @param	plc_tx	Pointer to the handler object
 */
void plc_tx_stop_transmission(struct plc_tx *plc_tx);

#endif /* LIBPLC_CAPE_TX_H */

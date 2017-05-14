/**
 * @file
 * @brief	SPI transmission management
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef LIBPLC_CAPE_SPI_H
#define LIBPLC_CAPE_SPI_H

struct plc_gpio;

struct spi *spi_create(int plc_driver, struct plc_gpio *plc_gpio);
void spi_release(struct spi *spi);
void spi_configure(struct spi *spi, uint32_t spi_dac_freq, uint16_t spi_dac_delay);
void spi_configure_sps(struct spi *spi, uint32_t sampling_rate_sps);
float spi_get_sampling_rate_sps(struct spi *spi);
char *spi_get_info(struct spi *spi);
void spi_set_dac_mode(struct spi *spi, int enable);
void spi_transfer_dac_buffer(struct spi *spi, uint16_t *samples_tx,
		int samples_tx_count);
void spi_transfer_dac_sample(struct spi *spi, uint16_t sample);
void spi_execute_command_fullduplex(struct spi *spi, uint8_t reg, uint8_t value);
void spi_write_command(struct spi *spi, uint8_t reg, uint8_t value);
uint8_t spi_read_command(struct spi *spi, uint8_t reg);

void spi_allocate_buffers_dma(struct spi *spi, uint16_t *buf1, uint32_t buf1_count,
		uint16_t *buf2, uint32_t buf2_count);
void spi_release_buffers_dma(struct spi *spi);
void spi_start_dma(struct spi *spi);
void spi_abort_dma(struct spi *spi);
uint8_t spi_wait_dma_buffer_sent(struct spi *spi);

#endif /* LIBPLC_CAPE_SPI_H */

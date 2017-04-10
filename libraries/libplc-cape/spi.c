/**
 * @file
 * 
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#define _GNU_SOURCE		// Required for 'asprintf' declaration
#include <errno.h>		// errno
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "+common/api/+base.h"
#include "+common/api/bbb.h"
#include "error.h"
#include "libraries/libplc-gpio/api/gpio.h"
#include "spi.h"

#define GPIO_DAC_MASK GPIO_P9_24_MASK
#define GPIO_DAC_BANK GPIO_P9_24_BANK

#define AFEREG_READ_MASK 0x80
#define AFEREG_WRITE_MASK 0x0

// Uncomment the following line to trace the SPI interaction
// #define VERBOSE

static const char *device_spi = "/dev/spidev1.0";
static const char *device_spi_plc = "/dev/spidev_plc1.0";

struct spi
{
	int file_debugfs;
	uint8_t mode;
	uint8_t lsb;
	uint8_t bits_cmd;
	uint32_t speed_cmd;
	uint16_t delay_cmd;
	uint8_t bits_dac;
	uint32_t speed_dac;
	uint16_t delay_dac;
	struct plc_gpio_pin_out *pin_dac;
	int spi_fd;
	int dac_mode;
};

// 'debugfs' driver communication
// TODO: Reuse the same struct as in 'spi-omap2-mcspi_plc.c'. Currently manually synchronized
struct command_set_buffers_t
{
	uint16_t *buf1;
	uint32_t buf1_count;
	uint16_t *buf2;
	uint32_t buf2_count;
};
struct debugfs_command_t
{
	uint8_t command;
	union
	{
		struct command_set_buffers_t set_buffers;
	};
}__attribute__((aligned(1),packed));

static void spi_debugfs_reset(int file_debugfs)
{
	// Send Reset (0x0) to initialize any previous resident configuration
	int bytes_written;
	char buf[1] =
	{ 0x0 };
	bytes_written = write(file_debugfs, buf, 1);
	assert(bytes_written == 1);
}

static void spi_debugfs_send_pid(int file_debugfs)
{
	// 0x1 means PID
	int len;
	int bytes_written;
	char buf[12];
	buf[0] = 0x1;
	snprintf(buf + 1, sizeof(buf) - 1, "%d", getpid());
	len = 1 + strlen(buf + 1);
	bytes_written = write(file_debugfs, buf, len);
	assert(bytes_written == len);
}

static void spi_debugfs_allocate_buffers_dma(int file_debugfs, uint16_t *buf1, uint32_t buf1_count,
		uint16_t *buf2, uint32_t buf2_count)
{
	// 0x2 means ActivatePingPong
	struct debugfs_command_t debugfs_cmd;
	debugfs_cmd.command = 0x2;
	debugfs_cmd.set_buffers.buf1 = buf1;
	debugfs_cmd.set_buffers.buf1_count = buf1_count;
	debugfs_cmd.set_buffers.buf2 = buf2;
	debugfs_cmd.set_buffers.buf2_count = buf2_count;
	int len = 1 + sizeof(debugfs_cmd.set_buffers);
	assert(len == 17);
	// Alternative way
	/*
	 buf[0] = 0x2;
	 *(uint32_t*)(buf+1) = (uint32_t)samples_tx1_page;
	 *(uint32_t*)(buf+5) = sigtest_num_samples;
	 *(uint32_t*)(buf+9) = (uint32_t)samples_tx2_page;
	 *(uint32_t*)(buf+13) = sigtest_num_samples;
	 len = 17;
	 */
	int bytes_written = write(file_debugfs, &debugfs_cmd, len);
	assert(bytes_written == len);
}

static void spi_debugfs_release_buffers_dma(int file_debugfs)
{
	// 0x3 means 'release DMA buffers'
	int bytes_written;
	char buf[1] =
	{ 0x3 };
	bytes_written = write(file_debugfs, buf, 1);
	assert(bytes_written == 1);
}

static void spi_debugfs_start_dma(int file_debugfs)
{
	// 0x4 means 'start SPI by ping-pong DMA'
	int bytes_written;
	char buf[1] =
	{ 0x4 };
	bytes_written = write(file_debugfs, buf, 1);
	assert(bytes_written == 1);
}

static void spi_debugfs_abort_dma(int file_debugfs)
{
	// 0x5 means 'abort DMA'
	int bytes_written;
	char buf[1] =
	{ 0x5 };
	bytes_written = write(file_debugfs, buf, 1);
	assert(bytes_written == 1);
}

#define DEBUGFS_IOC_MAGIC 'd'
#define DEBUGFS_IOC_RD_WAIT_DMA_BUFFER _IOR(DEBUGFS_IOC_MAGIC, 1, uint8_t)
static uint8_t spi_debugfs_wait_dma_buffer_sent(int file_debugfs)
{
	uint8_t buffer_in_dma_index;
	int ret = ioctl(file_debugfs, DEBUGFS_IOC_RD_WAIT_DMA_BUFFER, &buffer_in_dma_index);
	assert(ret != -1);
	return buffer_in_dma_index;
}

// Only 10th first bits are accepted. Remaining 6 bits are ignored
// Most significant byte corresponds to the "reg" parameter
// Barrido lineal del DAC en su margen dinámico
// TODO : Revisar si el truncado se come el 1er. bit ya que con 0xFF, Tou = 1V, y con 0xC0 = 0V

// TODO: Refactor. Move 'plc_cape_emulation' from global to parameter
extern int plc_cape_emulation;
ATTR_EXTERN struct spi *spi_create(int plc_driver, struct plc_gpio *plc_gpio)
{
	struct spi *spi = (struct spi*) calloc(1, sizeof(struct spi));
	if (!plc_cape_emulation)
	{
		// TODO: Change 'file_debugfs' by another interactive means
		spi->file_debugfs = open("/sys/kernel/debug/plc_signal_spi_omap2", O_WRONLY, S_IWUSR);
		if (spi->file_debugfs < 0)
		{
			libplc_cape_set_error_msg_errno("Error at SPI-debugfs initialization");
			free(spi);
			return NULL;
		}
		spi_debugfs_reset(spi->file_debugfs);
		spi_debugfs_send_pid(spi->file_debugfs);
	}

	spi->pin_dac = plc_gpio_pin_out_create(plc_gpio, GPIO_DAC_BANK, GPIO_DAC_MASK);
	spi->mode = SPI_MODE_0;
	spi->lsb = 0;
	spi->bits_cmd = 16;
	// TODO: Revisar que 'speed_cmd == 20000' es una buena velocidad -> En realidad se convierte a
	//	'11718'
	// The real speed of the SPI is the nearest accepted one. See pags 4777, 4781 & 4865 of
	//	'AM335x_TechnicalManual_spruh73l.pdf' for more info
	// Note also that AFE ADC uses 10-data-bits + 1-cs-bit for DAC = 11 bits
	spi->speed_cmd = 20000;
	spi->delay_cmd = 0;
	// Two solutions for bit_dac:
	//	- Use 16 bits with 6-bits-left-shifting the samples before tx (in order the AFE to make the
	//		proper truncation)
	//	- Use 10 bits without shifting. Truncation is properly working. As less bits involved this
	//		offers the best solution
	// Note that the AFE understands the end of the symbol when the CS line is toggled
	spi->bits_dac = 10;

	if (plc_cape_emulation)
		return spi;

	int ret;
	spi->spi_fd = open(plc_driver ? device_spi_plc : device_spi, O_RDWR);
	if (spi->spi_fd < 0)
	{
		int last_error = errno;
		plc_gpio_pin_out_release(spi->pin_dac);
		free(spi);
		errno = last_error;
		libplc_cape_set_error_msg_errno("Can't open SPI device");
		return NULL;
	}

	// SPI & LSB/MSB modes
	ret = ioctl(spi->spi_fd, SPI_IOC_WR_MODE, &spi->mode);
	if (ret != -1)
	{
		uint8_t mode;
		ret = ioctl(spi->spi_fd, SPI_IOC_RD_MODE, &mode);
		if (ret != -1)
		{
			assert(mode == spi->mode);
			ret = ioctl(spi->spi_fd, SPI_IOC_WR_LSB_FIRST, &spi->lsb);
			if (ret != -1)
			{
				uint8_t lsb;
				ret = ioctl(spi->spi_fd, SPI_IOC_RD_LSB_FIRST, &lsb);
				if (ret != -1)
					assert(lsb == spi->lsb);
			}
		}
	}
	if (ret == -1)
	{
		int last_error = errno;
		plc_gpio_pin_out_release(spi->pin_dac);
		close(spi->spi_fd);
		free(spi);
		errno = last_error;
		libplc_cape_set_error_msg_errno("Can't initialize SPI device");
		return NULL;
	}

	// Set command mode as the default one
	spi_set_dac_mode(spi, 0);

	return spi;
}

ATTR_EXTERN void spi_release(struct spi *spi)
{

	if (!plc_cape_emulation)
	{
		spi_debugfs_reset(spi->file_debugfs);
		close(spi->file_debugfs);
		close(spi->spi_fd);
	}
	plc_gpio_pin_out_set(spi->pin_dac, 0);
	plc_gpio_pin_out_release(spi->pin_dac);
	free(spi);
}

ATTR_EXTERN void spi_configure(struct spi *spi, uint32_t spi_dac_freq, uint16_t spi_dac_delay)
{
	// Working tested values for 'speed_dac': 20000 (= 20k), 60k, 200k, 1M, 10M
	// For 1M it has been seen that measured SPI freq about 750kHz; For 10M SPI freq measured about
	//	6MHz
	// In both cases using continuous one-sample-IoControls calls at maximum throughput there is
	//	about 20us between symbols
	// This would limit the abs max freq to 1/20us = 50kHz. Considering the symbol time we have
	//	measured a maximum tx rate of 27000 samples/s for 'speed_dac = 1M' and 40ksps for 10M.
	// With buffer-based-IoControls we reduce the time between symbols within the buffer to 0.2us
	//	-> 5MHz
	spi->speed_dac = spi_dac_freq;
	spi->delay_dac = spi_dac_delay;
}

// POST_CONDITION: pointer returned should be released with 'free'
ATTR_EXTERN char *spi_get_info(struct spi *spi)
{
	char *info;
	if (!spi)
	{
		asprintf(&info, "N/A\n");
		return info;
	}
	asprintf(&info, "Mode: %d\n"
			"Bit order: %d\n"
			"Bits per word: %d\n"
			"Rate: %d bauds\n", spi->mode, spi->lsb, spi->bits_dac, spi->speed_dac);
	return info;
}

// Set default values into the driver
// Keeping the proper mode avoids explicitly specifying per-command-parameters as these ones:
//	tr.speed_hz = dac_mode?spi->speed_dac:spi->speed_cmd;
//	tr.bits_per_word = dac_mode?spi->bits_dac:spi->bits_cmd;
// The driver is a bit faster if 'tr.speed_hz' and 'tr.bits_per_word' are sent as zero (meaning
//	default value) because avoids forcing the new values for each command/transfer and reverting
//	back to defaults after them
ATTR_EXTERN void spi_set_dac_mode(struct spi *spi, int enable)
{
	spi->dac_mode = enable;

	if (plc_cape_emulation)
		return;

	int ret;
	// Bits per word
	uint8_t bits_per_word = enable ? spi->bits_dac : spi->bits_cmd;
	ret = ioctl(spi->spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits_per_word);
	assert(ret != -1);
	uint8_t bits_per_word_read;
	ret = ioctl(spi->spi_fd, SPI_IOC_RD_BITS_PER_WORD, &bits_per_word_read);
	assert(ret != -1);
	assert(bits_per_word_read == bits_per_word);
	// Baud rate
	uint32_t speed = enable ? spi->speed_dac : spi->speed_cmd;
	ret = ioctl(spi->spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	assert(ret != -1);
	uint32_t speed_read;
	ret = ioctl(spi->spi_fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed_read);
	assert(ret != -1);
	assert(speed_read == speed);
	plc_gpio_pin_out_set(spi->pin_dac, enable);
}

ATTR_EXTERN void spi_transfer_dac_buffer(struct spi *spi, uint16_t *samples_tx,
		int samples_tx_count)
{
	// Ensuring that 'dac_mode == 1' allows using the default values for the 'speed_hz' and
	//	'bits_per_word'
	assert(spi->dac_mode);
	// IMPORTANT: The AFE requires the CS signal toggling between symbols. In buffered mode the CS
	//	is keep active during the whole buffer transmission
	//	Even using the '.cs_change' property (set to 1) the behavior is the same
	struct spi_ioc_transfer tr;
	memset(&tr, 0, sizeof(tr));
	tr.tx_buf = (unsigned long) samples_tx;
	tr.rx_buf = (unsigned long) NULL;
	tr.len = samples_tx_count * sizeof(uint16_t);
	tr.delay_usecs = spi->delay_dac;
	// Using default driver values is a bit faster because avoid reverting back to defaults per
	//	command
	//		tr.speed_hz = spi->speed_dac;
	//		tr.bits_per_word = spi->bits_dac;
	if (!plc_cape_emulation)
	{
		int ret = ioctl(spi->spi_fd, SPI_IOC_MESSAGE(1), &tr);
		if (ret < 1)
			libplc_cape_set_error_msg_errno("Can't send spi message");
	}
}

/*
 // Another alternative mode for transfering the whole block at once is using 'write'.
 // It has been checked that produces the same results as the 'ioctl' call. For the 'speed_hz'
 //		settings and so
 // it it supposed to use the default values set in 'spi_init' through the corresponding 'ioctl'
 // The limitations are the same as ioctl (e.g. the 'bufsiz == 4096' of spidev_plc)
 void spi_transfer_dac_buffer_using_write(struct spi *spi, uint16_t *samples_tx,
		int samples_tx_count)
 {
 uint32_t count = samples_tx_count*2;
 uint8_t *tx = (uint8_t*)samples_tx;
 while (count > 0)
 {
 ssize_t bytes_written = write(spi->spi_fd, tx, count);
 if (bytes_written <= 0) pexit("error in SPI transmission");
 tx += bytes_written;
 count -= bytes_written;
 }
 }
 */

ATTR_EXTERN void spi_transfer_dac_sample(struct spi *spi, uint16_t sample)
{
	assert(spi->dac_mode);
	// C-style struct initialization (incompatible in C++)
	struct spi_ioc_transfer tr;
	memset(&tr, 0, sizeof(tr));
	tr.tx_buf = (unsigned long) &sample;
	tr.rx_buf = (unsigned long) NULL;
	tr.len = 2;
	tr.delay_usecs = spi->delay_dac;
	if (!plc_cape_emulation)
	{
		int ret = ioctl(spi->spi_fd, SPI_IOC_MESSAGE(1), &tr);
		if (ret < 1)
			libplc_cape_set_error_msg_errno("Can't send spi message");
	}
}

#ifdef VERBOSE
void spi_log_frame(char frame_id, uint8_t *frame, uint32_t frame_count)
{
	int n;
	logger_log_sequence_format(logger, "%c ", frame_id);
	for (n = 0; n < frame_count; n++)
	logger_log_sequence_format(logger, "%.2X ", frame[n]);
	logger_log_sequence(logger, "\n");
}
#endif

ATTR_EXTERN void spi_execute_command_fullduplex(struct spi *spi, uint8_t reg, uint8_t value)
{
	assert(!spi->dac_mode);
	uint8_t tx[] =
	{ value, reg };
	uint8_t rx[ARRAY_SIZE(tx)] =
	{ 0, };
	struct spi_ioc_transfer tr;
	memset(&tr, 0, sizeof(tr));
	tr.tx_buf = (unsigned long) tx;
	tr.rx_buf = (unsigned long) rx;
	tr.len = ARRAY_SIZE(tx);
	tr.delay_usecs = spi->delay_cmd;
	if (!plc_cape_emulation)
	{
		int ret = ioctl(spi->spi_fd, SPI_IOC_MESSAGE(1), &tr);
		if (ret < 1)
			libplc_cape_set_error_msg_errno("Can't send spi message");
	}
#ifdef VERBOSE
	spi_log_frame('>', tx, ARRAY_SIZE(tx));
	spi_log_frame('<', rx, ARRAY_SIZE(rx));
#endif
}

ATTR_EXTERN void spi_write_command(struct spi *spi, uint8_t reg, uint8_t value)
{
	assert(spi->dac_mode == 0);
	uint8_t buffer[] =
	{ value, AFEREG_WRITE_MASK | reg };
	struct spi_ioc_transfer tr;
	memset(&tr, 0, sizeof(tr));
	tr.tx_buf = (unsigned long) buffer;
	tr.rx_buf = (unsigned long) NULL;
	tr.len = ARRAY_SIZE(buffer);
	tr.delay_usecs = spi->delay_cmd;
	if (!plc_cape_emulation)
	{
		int ret = ioctl(spi->spi_fd, SPI_IOC_MESSAGE(1), &tr);
		if (ret < 1)
			libplc_cape_set_error_msg_errno("Can't send spi message");
	}
#ifdef VERBOSE
	spi_log_frame('>', buffer, ARRAY_SIZE(buffer));
#endif
}

// Command execution in half-duplex mode
ATTR_EXTERN uint8_t spi_read_command(struct spi *spi, uint8_t reg)
{
	assert(spi->dac_mode == 0);
	uint8_t buffer[] =
	{ 0, AFEREG_READ_MASK | reg };
	struct spi_ioc_transfer tr;
	memset(&tr, 0, sizeof(tr));
	tr.len = ARRAY_SIZE(buffer);
	tr.delay_usecs = spi->delay_cmd;
	// Execute command
	tr.tx_buf = (unsigned long) buffer;
	tr.rx_buf = (unsigned long) NULL;
	if (plc_cape_emulation)
		return 0;
	int ret = ioctl(spi->spi_fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1)
		libplc_cape_set_error_msg_errno("Can't send spi message");
#ifdef VERBOSE
	spi_log_frame('>', buffer, ARRAY_SIZE(buffer));
#endif
	// Get response
	tr.tx_buf = (unsigned long) NULL;
	tr.rx_buf = (unsigned long) buffer;
	ret = ioctl(spi->spi_fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1)
		libplc_cape_set_error_msg_errno("Can't send spi message");
	// The AFE031 keeps the buffer[1] value in the response
	assert(buffer[1] == (AFEREG_READ_MASK | reg));
#ifdef VERBOSE
	spi_log_frame('<', buffer, 1);
#endif
	return buffer[0];
}

ATTR_EXTERN void spi_allocate_buffers_dma(struct spi *spi, uint16_t *buf1, uint32_t buf1_count,
		uint16_t *buf2, uint32_t buf2_count)
{
	spi_debugfs_allocate_buffers_dma(spi->file_debugfs, buf1, buf1_count, buf2, buf2_count);
}

ATTR_EXTERN void spi_release_buffers_dma(struct spi *spi)
{
	spi_debugfs_release_buffers_dma(spi->file_debugfs);
}

ATTR_EXTERN void spi_start_dma(struct spi *spi)
{
	spi_debugfs_start_dma(spi->file_debugfs);
}

ATTR_EXTERN void spi_abort_dma(struct spi *spi)
{
	spi_debugfs_abort_dma(spi->file_debugfs);
}

ATTR_EXTERN uint8_t spi_wait_dma_buffer_sent(struct spi *spi)
{
	return spi_debugfs_wait_dma_buffer_sent(spi->file_debugfs);
}

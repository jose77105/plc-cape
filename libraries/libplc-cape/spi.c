/**
 * @file
 * 
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
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
#include "afe_commands.h"
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
	uint32_t rate_bps;
	uint16_t delay_dac;
	struct plc_gpio_pin_out *pin_dac;
	int spi_fd;
	int dac_mode;
	// TODO: Move to a dedicated 'spi_emulation' struct
	uint8_t emulated_reg[256];
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

ATTR_INTERN uint32_t spi_sps_to_bps(float requested_sampling_rate_sps)
{
	// Translate sampling_rate to theoretical exact bps
	float requested_bps = 10.5 / (1.0 / requested_sampling_rate_sps - 165e-9);
	if (requested_bps < 0)
		return 0.0;
	// Process settings when reinitializing the controller to refresh dependent data
	// SPI DAC valid frequencies = 187.5kHz, 375k, 750k, 1500k, 3M, 6M, 12M...
	// NOTE: 'ceil' function avoided to minimize dependencies. Trick used instead
	uint32_t min_bps = (uint32_t)requested_bps;
	if (requested_bps > (float)min_bps) min_bps++;
	uint32_t closest_accepted_bps;
	uint32_t next_bps = 12000000;
	do {
		closest_accepted_bps = next_bps;
		next_bps = closest_accepted_bps/2;
	} while (min_bps <= next_bps);
	return closest_accepted_bps;
}

ATTR_INTERN float spi_bps_to_sps(uint32_t spi_rate_bps)
{
	// 10-data-bits + 0.5 clock between words (TCS) + 165ns CS pulse =
	//	10.5 proportional factor + 165ns fixed time
	return 1.0 / (10.5 / spi_rate_bps + 165e-9);
}

// TODO: Refactor. Move 'plc_cape_emulation' from global to parameter
extern int plc_cape_emulation;
ATTR_INTERN struct spi *spi_create(int plc_driver, struct plc_gpio *plc_gpio)
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
	{
		spi->emulated_reg[AFEREG_REVISION] = 2;
		return spi;
	}
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

ATTR_INTERN void spi_release(struct spi *spi)
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

ATTR_INTERN void spi_configure(struct spi *spi, uint32_t spi_rate_bps, uint16_t spi_delay)
{
	spi->rate_bps = spi_rate_bps;
	spi->delay_dac = spi_delay;
}

ATTR_INTERN void spi_configure_sps(struct spi *spi, uint32_t sampling_rate_sps)
{
	uint32_t spi_rate_bps = spi_sps_to_bps(sampling_rate_sps);
	spi_configure(spi, spi_rate_bps, 0);
}

ATTR_INTERN float spi_get_sampling_rate_sps(struct spi *spi)
{
	return spi_bps_to_sps(spi->rate_bps);
}

// POST_CONDITION: pointer returned should be released with 'free'
ATTR_INTERN char *spi_get_info(struct spi *spi)
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
			"Rate: %d bauds\n", spi->mode, spi->lsb, spi->bits_dac, spi->rate_bps);
	return info;
}

// Set default values into the driver
// Keeping the proper mode avoids explicitly specifying per-command-parameters as these ones:
//	tr.speed_hz = dac_mode?spi->speed_dac:spi->speed_cmd;
//	tr.bits_per_word = dac_mode?spi->bits_dac:spi->bits_cmd;
// The driver is a bit faster if 'tr.speed_hz' and 'tr.bits_per_word' are sent as zero (meaning
//	default value) because avoids forcing the new values for each command/transfer and reverting
//	back to defaults after them
ATTR_INTERN void spi_set_dac_mode(struct spi *spi, int enable)
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
	uint32_t speed = enable ? spi->rate_bps : spi->speed_cmd;
	ret = ioctl(spi->spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	assert(ret != -1);
	uint32_t speed_read;
	ret = ioctl(spi->spi_fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed_read);
	assert(ret != -1);
	assert(speed_read == speed);
	plc_gpio_pin_out_set(spi->pin_dac, enable);
}

ATTR_INTERN void spi_transfer_dac_buffer(struct spi *spi, uint16_t *samples_tx,
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

ATTR_INTERN void spi_transfer_dac_sample(struct spi *spi, uint16_t sample)
{
	assert(spi->dac_mode);
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

ATTR_INTERN void spi_execute_command_fullduplex(struct spi *spi, uint8_t reg, uint8_t value)
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

ATTR_INTERN void spi_write_command(struct spi *spi, uint8_t reg, uint8_t value)
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
	// TODO: Instead of 'if' make redirection on spi_write & spi_read commands
	if (plc_cape_emulation)
	{
		spi->emulated_reg[reg] = value;
	}
	else
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
ATTR_INTERN uint8_t spi_read_command(struct spi *spi, uint8_t reg)
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
		return spi->emulated_reg[reg];
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

ATTR_INTERN void spi_allocate_buffers_dma(struct spi *spi, uint16_t *buf1, uint32_t buf1_count,
		uint16_t *buf2, uint32_t buf2_count)
{
	spi_debugfs_allocate_buffers_dma(spi->file_debugfs, buf1, buf1_count, buf2, buf2_count);
}

ATTR_INTERN void spi_release_buffers_dma(struct spi *spi)
{
	spi_debugfs_release_buffers_dma(spi->file_debugfs);
}

ATTR_INTERN void spi_start_dma(struct spi *spi)
{
	spi_debugfs_start_dma(spi->file_debugfs);
}

ATTR_INTERN void spi_abort_dma(struct spi *spi)
{
	spi_debugfs_abort_dma(spi->file_debugfs);
}

ATTR_INTERN uint8_t spi_wait_dma_buffer_sent(struct spi *spi)
{
	return spi_debugfs_wait_dma_buffer_sent(spi->file_debugfs);
}

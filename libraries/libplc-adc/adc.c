/**
 * @file
 * @brief	**Main** file
 *
 * @see		@ref library-libplc-adc
 *
 * @cond COPYRIGHT_NOTES
 *
 * ##LICENSE
 *
 *		This file is part of plc-cape project.
 *
 *		plc-cape project is free software: you can redistribute it and/or modify
 *		it under the terms of the GNU General Public License as published by
 *		the Free Software Foundation, either version 3 of the License, or
 *		(at your option) any later version.
 *
 *		plc-cape project is distributed in the hope that it will be useful,
 *		but WITHOUT ANY WARRANTY; without even the implied warranty of
 *		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *		GNU General Public License for more details.
 *
 *		You should have received a copy of the GNU General Public License
 *		along with plc-cape project.  If not, see <http://www.gnu.org/licenses/>. 
 *
 * @copyright
 *	Copyright (C) 2016 Jose Maria Ortega
 * 
 * @endcond
 */

#define _GNU_SOURCE		// Required for proper declaration of 'pthread_timedjoin_np'
#include <err.h>		// warnx
#include <errno.h>		// EAGAIN
#include <fcntl.h>		// O_RDONLY
#include <poll.h>		// poll
#include <pthread.h>
#include <sys/ioctl.h>	// _IOxxx defines
#include <unistd.h>		// open
#include "+common/api/+base.h"
#include "+common/api/error.h"
#include "+common/api/logger.h"
#include "api/adc.h"
#include "api/analysis.h"
#include "libraries/libplc-tools/api/file.h"
#include "libraries/libplc-tools/api/time.h"
#include "meta.h"

#define ADCHS_IOC_MAGIC 'a'
// TODO: Revisar si no debería ser _IOW(ADCHS_IOC_MAGIC, 1, void) o si se deberían enviar
//	un parámetro uint8_t en lugar de NULL en estas IOCTL
#define ADCHS_IOC_WR_START_CAPTURE _IOW(ADCHS_IOC_MAGIC, 1, uint8_t)
#define ADCHS_IOC_WR_STOP_CAPTURE _IOW(ADCHS_IOC_MAGIC, 2, uint8_t)

// ADC devices
const char *device_adc = "/sys/devices/ocp.3/helper.15/AIN0";
const char *device_adc_plc = "/dev/adchs_plc";
// IIO interface
const char *iio_dev = "/dev/iio:device0";
const char *iio_dir = "/sys/bus/iio/";
const char *iio_dev_dir = "/sys/bus/iio/devices/iio:device0/";

// TODO: Refactor. Remove global variable (in fact most are used only in
//	'adc_capture_kernel_buffering')
// 'iio_std_exchange' to use the std IIO functionality based on '/dev/iio:device0'
const int iio_std_exchange = 0;
const float freq_adc_sps = 200000.0;

struct plc_adc
{
	int kernel_buffering;
	rx_buffer_completed_callback_t rx_buffer_completed_callback;
	void *rx_buffer_completed_callback_data;
	int plc_driver;
	int fd;
	uint32_t buffer_samples;
	pthread_t thread;
	volatile int end_thread;
	int wakeup_pipe[2];
	int capture_started;
	union
	{
		struct iio
		{
			int fd;
			struct pollfd pollfdset[2];
		} iio;
		struct custom
		{
			int fd;
		} custom;
	} capture_mode;
};

ATTR_EXTERN float plc_adc_get_sampling_frequency(void)
{
	return freq_adc_sps;
}

ATTR_EXTERN struct plc_adc *plc_adc_create(int plc_driver)
{
	// TODO: Replace 'malloc' + 'memset' by 'calloc'
	// 'ptr = calloc(1, size);' equals to 'ptr = malloc(size); memset(ptr, 0, size);'
	struct plc_adc *plc_adc = calloc(1, sizeof(struct plc_adc));
	plc_adc->plc_driver = plc_driver;
	if (plc_driver)
	{
		plc_adc->fd = open(device_adc_plc, O_RDONLY);
		if (plc_adc->fd < 0)
		{
			// Save 'errno' just to prevent another subsquesent call to reset it
			int last_error = errno;
			free(plc_adc);
			errno = last_error;
			return NULL;
		}
	}
	else
	{
		plc_adc->fd = -1;
	}
	return plc_adc;
}

ATTR_EXTERN void plc_adc_release(struct plc_adc *plc_adc)
{
	if (plc_adc->capture_started)
		plc_adc_stop_capture(plc_adc);
	if (plc_adc->fd != -1)
		close(plc_adc->fd);
	free(plc_adc);
}

ATTR_EXTERN void plc_adc_set_rx_buffer_completed_callback(struct plc_adc *plc_adc,
		rx_buffer_completed_callback_t rx_buffer_completed_callback,
		void *rx_buffer_completed_callback_data)
{
	plc_adc->rx_buffer_completed_callback = rx_buffer_completed_callback;
	plc_adc->rx_buffer_completed_callback_data = rx_buffer_completed_callback_data;
}

ATTR_EXTERN uint16_t plc_adc_read_sample(struct plc_adc *plc_adc)
{
	char adc_value_text[5];
	adc_value_text[4] = '\0';
	if (plc_adc->plc_driver)
	{
		int ret = read(plc_adc->fd, adc_value_text, 4);
		assert(ret != -1);
	}
	else
	{
		// The 'plc_adc' driver requires to open & close the connection for each value read
		int adc_fd = open(device_adc, O_RDONLY);
		if (adc_fd < 0)
		{
			// TODO: Improve error controlo
			warnx("Can't open ADC device: %s", strerror(errno));
			exit(EXIT_FAILURE);
		}
		int ret = read(adc_fd, adc_value_text, 4);
		assert(ret != -1);
		close(adc_fd);
	}
	return atoi(adc_value_text);
}

// TODO: Refactor. Improve the usage of 'union' and the calling way (fn-based instead of if-based)
void adc_iio_capture_init(struct plc_adc *plc_adc)
{
	struct iio *iio = &plc_adc->capture_mode.iio;
	char buffer_adc_count_text[11];
	sprintf(buffer_adc_count_text, "%u", plc_adc->buffer_samples);
	plc_file_write_string(iio_dir, "devices/iio_sysfs_trigger/add_trigger", "1");
	plc_file_write_string(iio_dev_dir, "trigger/current_trigger", "sysfstrig1");
	plc_file_write_string(iio_dev_dir, "scan_elements/in_voltage0_en", "1");
	plc_file_write_string(iio_dev_dir, "buffer/length", buffer_adc_count_text);
	plc_file_write_string(iio_dev_dir, "buffer/enable", "1");
	// Attempt to open non blocking the access dev
	iio->fd = open(iio_dev, O_RDONLY | O_NONBLOCK);
	assert(iio->fd != -1);
	// Pollfd
	memset((void*) &iio->pollfdset, 0, sizeof(iio->pollfdset));
	iio->pollfdset[0].fd = plc_adc->capture_mode.iio.fd;
	// It seems (to review) that 'POLLPRI' is required in order the 'poll' works ok when the 'read'
	// has been interrupted by a signal and a partial transfer is done.
	// If not set the app hangs quite easily on the 'poll'
	// Anyway, the current system sometimes still hangs on 'poll' for no apparent reason
	iio->pollfdset[0].events = POLLIN | POLLPRI;
	// Use an auxiliar pipe to wakeup the 'poll' to terminate it nicely
	iio->pollfdset[1].fd = plc_adc->wakeup_pipe[0];
	iio->pollfdset[1].events = POLLIN | POLLPRI;
	// For continuos transfer only a 'trigger_now' is required
	plc_file_write_string(iio_dir, "devices/trigger0/trigger_now", "1");
}

void adc_iio_capture_end(struct plc_adc *plc_adc)
{
	close(plc_adc->capture_mode.iio.fd);
	plc_file_write_string(iio_dev_dir, "buffer/enable", "0");
	/* Disconnect the trigger - just write a dummy name. */
	plc_file_write_string(iio_dev_dir, "trigger/current_trigger", "NULL");
	plc_file_write_string(iio_dir, "devices/iio_sysfs_trigger/remove_trigger", "1");
}

int adc_iio_capture_cycle(struct plc_adc *plc_adc, sample_rx_t *buffer_adc_cur)
{
	struct iio *iio = &plc_adc->capture_mode.iio;
	int buffer_rx_ok = 1;
	int adc_samples_remaining = plc_adc->buffer_samples;
	// scan_size hard-coded to 4 bytes for simplicity
	// Can be obtained dynamically through:
	//	$ cat /sys/bus/iio/devices/iio\:device0/scan_elements/in_voltage0_type
	//	le:u12/32>>0
	// where le = little-endian, unsigned, 12-bits samples, sample storage size, shift bits
	int scan_size = 4;
	assert(scan_size == sizeof(sample_rx_t));
	// NOTES on EINTR with 'read':
	//	- http://www.gnu.org/software/libc/manual/html_node/Interrupted-Primitives.html
	//	- http://stackoverflow.com/questions/4959524/...
	//		...when-to-check-for-eintr-and-repeat-the-function-call
	//	- http://programmers.stackexchange.com/questions/267748/...
	//		...proper-way-of-handling-eintr-in-libraries
	while (adc_samples_remaining > 0)
	{
		iio->pollfdset[0].revents = 0;
		// int rc = poll(iio->pollfdset, ARRAY_SIZE(iio->pollfdset), -1);
		int rc = poll(iio->pollfdset, ARRAY_SIZE(iio->pollfdset), 2000);
		if (rc == 0)
		{
			plc_libadc_log_line(">> Timeout");
			buffer_rx_ok = 0;
			break;
		}
		assert(rc > 0);
		if (iio->pollfdset[0].revents)
		{
			size_t bytes_to_read = adc_samples_remaining * scan_size;
			ssize_t bytes_read = read(iio->fd, buffer_adc_cur, bytes_to_read);
			assert(bytes_read > 0);
			int scaned_samples = bytes_read / scan_size;
			buffer_adc_cur += scaned_samples;
			assert(adc_samples_remaining < scaned_samples);
			adc_samples_remaining -= scaned_samples;
		}
		else if (iio->pollfdset[1].revents)
		{
			// Bytes remaining yet: 'adc_samples_remaining'
			if (plc_adc_logger_api)
				plc_adc_logger_api->log_sequence_format(plc_adc_logger_handle,
						">> Partial buffer rx. Samples remaining %u\n", adc_samples_remaining);
			buffer_rx_ok = 0;
			break;
		}
	}
	return buffer_rx_ok = 0;
}

void adc_custom_capture_init(struct plc_adc *plc_adc)
{
	// TODO: Remove this 'iio' interface when driver adapted to work without
	//	Note that even the 'add_trigger' is required to the custom implementation to work
	char buffer_adc_count_text[11];
	sprintf(buffer_adc_count_text, "%u", plc_adc->buffer_samples);
	plc_file_write_string(iio_dir, "devices/iio_sysfs_trigger/add_trigger", "1");
	plc_file_write_string(iio_dev_dir, "trigger/current_trigger", "sysfstrig1");
	plc_file_write_string(iio_dev_dir, "scan_elements/in_voltage0_en", "1");
	plc_file_write_string(iio_dev_dir, "buffer/length", buffer_adc_count_text);
	plc_file_write_string(iio_dev_dir, "buffer/enable", "1");
	plc_adc->capture_mode.custom.fd = plc_adc->fd;
	int ret = ioctl(plc_adc->capture_mode.custom.fd, ADCHS_IOC_WR_START_CAPTURE, NULL);
	assert(ret != -1);
}

void adc_custom_capture_end(struct plc_adc *plc_adc)
{
	int ret = ioctl(plc_adc->capture_mode.custom.fd, ADCHS_IOC_WR_STOP_CAPTURE, NULL);
	assert(ret != -1);
	plc_file_write_string(iio_dev_dir, "buffer/enable", "0");
	/* Disconnect the trigger - just write a dummy name. */
	plc_file_write_string(iio_dev_dir, "trigger/current_trigger", "NULL");
	plc_file_write_string(iio_dir, "devices/iio_sysfs_trigger/remove_trigger", "1");
}

int adc_custom_capture_cycle(struct plc_adc *plc_adc, sample_rx_t *buffer_adc_cur)
{
	int adc_samples_remaining = plc_adc->buffer_samples;
	int scan_size = 2;
	assert(scan_size == sizeof(sample_rx_t));
	while (adc_samples_remaining > 0)
	{
		size_t bytes_to_read = adc_samples_remaining * scan_size;
		// TODO: Parece ser que el 'read' a veces se queda bloqueado esperando datos. Check why
		ssize_t bytes_read = read(plc_adc->capture_mode.custom.fd, buffer_adc_cur, bytes_to_read);
		// assert(bytes_read > 0);
		int scaned_samples = bytes_read / scan_size;
		buffer_adc_cur += scaned_samples;
		assert(scaned_samples <= adc_samples_remaining);
		adc_samples_remaining -= scaned_samples;
		// TODO: Instead of waiting make data_decodification in parallel by chunks or pushes
		// Wait 1 ms to don't intensively poll the ADC
		usleep(1000);
	}
	return 1;
}

int adc_user_buffering_capture_cycle(struct plc_adc *plc_adc, sample_rx_t *buffer_adc_cur)
{
	int adc_samples_remaining = plc_adc->buffer_samples;
	for (; adc_samples_remaining > 0; adc_samples_remaining--)
		*buffer_adc_cur++ = plc_adc_read_sample(plc_adc);
	return 1;
}

void *adc_thread_capture_samples(void *arg)
{
	// Boost 'self' thread
	// For more details look to 'thread_spi_tx_buffer'
	// TODO: Revisar por qué con el SCHED_RR peores resultados
	// NOTE: TX has more priority because more sensible at present
	struct sched_param param;
	param.sched_priority = 1;
	int ret = sched_setscheduler(0, SCHED_RR, &param);
	assert(ret == 0);

	// If writing to file do it first to a memory buffer to minimize the impact on captures
	struct plc_adc *plc_adc = (struct plc_adc *) arg;

	static const int BUFFERS_ADC_COUNT = 4;
	sample_rx_t *buffer_adc[BUFFERS_ADC_COUNT];
	int buffer_index = 0;
	int i;
	for (i = 0; i < BUFFERS_ADC_COUNT; i++)
		buffer_adc[i] = (sample_rx_t*) malloc(sizeof(sample_rx_t) * plc_adc->buffer_samples);
	sample_rx_t *buffer_adc_cur = buffer_adc[0];
	if (plc_adc->kernel_buffering)
	{
		if (iio_std_exchange)
			adc_iio_capture_init(plc_adc);
		else
			adc_custom_capture_init(plc_adc);
	}
	// TODO: Refactor. This patch is to avoid the 'read' function on 'iio_std_exchange' to be
	//	launched before the trigger
	// usleep(5000);
	while (!plc_adc->end_thread)
	{
		// logger_log_sequence(logger, "<");
		buffer_adc_cur = buffer_adc[buffer_index];
		int buffer_rx_ok = 1;
		if (plc_adc->kernel_buffering)
		{
			// TODO: NEXT: Implement kernel buffering from 'generic_buffer.c'
			if (iio_std_exchange)
				buffer_rx_ok = adc_iio_capture_cycle(plc_adc, buffer_adc_cur);
			else
				buffer_rx_ok = adc_custom_capture_cycle(plc_adc, buffer_adc_cur);
		}
		else
		{
			adc_user_buffering_capture_cycle(plc_adc, buffer_adc_cur);
		}
		if (plc_adc->rx_buffer_completed_callback)
		{
			plc_adc->rx_buffer_completed_callback(plc_adc->rx_buffer_completed_callback_data,
					buffer_adc_cur, plc_adc->buffer_samples);

		}
		// Timeout -> Retrigger
		// TODO: Revise why 'buffer/enable = 0' needs to be set. If not it seems that from time to
		//	time the "trigger_now = 1" is firing
		//	more than 1 trigger leading to a captured time < 100us which is a nonsense, but the
		//	buffer seems to be properly filled (?)
		//	A more expected behavior seems to happen if disabling and reenabling 'buffer/enable'
		//		plc_file_write_string(iio_dev_dir, "buffer/enable", "0");
		if (!buffer_rx_ok)
		{
			plc_libadc_log_line(">> Retrigger");
			continue;
		}
		if (++buffer_index == BUFFERS_ADC_COUNT)
			buffer_index = 0;
		// TODO: Sleep required to yield control to other less priority threads
		//	Remove? Maybe it's good because while in sleep the ADC buffer is going to be filled in
		//	the background
		//	Then better usage of data bursts
		// usleep(1000);
		// sleep(1);
	}
	if (plc_adc->kernel_buffering)
	{
		if (iio_std_exchange)
			adc_iio_capture_end(plc_adc);
		else
			adc_custom_capture_end(plc_adc);
	}
	// To debug internal buffers enable the following code
	/*
	 #define ADC_FILE_CAPTURE_CHUNK "plc_adc%d.csv"
	 if (plc_adc->samples_to_file)
	 {
	 logger_log_sequence_format(logger, "Buffer index in process = %d\n", buffer_index);
	 for (i = 0; i < BUFFERS_ADC_COUNT; i++)
	 {
	 char szFilename[16];
	 sprintf(szFilename, ADC_FILE_CAPTURE_CHUNK, i);
	 ret = plc_file_write_csv(szFilename, sample_rx_csv_enum, buffer_adc[i],
			plc_adc->buffer_samples);
	 assert(ret >= 0);
	 }
	 if (plc_adc->log_callback)
	 plc_adc->log_callback(plc_adc->log_callback_data, "ADC-Buffers saved\n");
	 }
	 */
	for (i = 0; i < BUFFERS_ADC_COUNT; i++)
		free(buffer_adc[i]);

	return NULL;
}

ATTR_EXTERN void plc_adc_start_capture(struct plc_adc *plc_adc, uint32_t buffer_samples,
		int kernel_buffering)
{
	int ret;
	plc_adc->buffer_samples = buffer_samples;
	plc_adc->end_thread = 0;
	plc_adc->kernel_buffering = kernel_buffering;
	if (iio_std_exchange)
	{
		ret = pipe(plc_adc->wakeup_pipe);
		assert(ret == 0);
	}
	ret = pthread_create(&plc_adc->thread, NULL, adc_thread_capture_samples, plc_adc);
	assert(ret == 0);
	plc_adc->capture_started = 1;
}

ATTR_EXTERN void plc_adc_stop_capture(struct plc_adc *plc_adc)
{
	int ret;
	assert(plc_adc->capture_started);
	plc_adc->end_thread = 1;
	if (iio_std_exchange)
	{
		char dummy = 0;
		// Write some data to wakeup the 'poll'
		write(plc_adc->wakeup_pipe[1], &dummy, sizeof(dummy));
		ret = pthread_join(plc_adc->thread, NULL);
		assert(ret == 0);
		close(plc_adc->wakeup_pipe[0]);
		close(plc_adc->wakeup_pipe[1]);
	}
	else
	{
		// TODO: Due to some problem on the ADC driver the 'read' call in 'adc_custom_capture_cycle'
		//	blocks some time preventing the 'plc_adc_stop_capture' to cancel an indefinte waiting
		//	-> Ugly patch: force releasing anyway
		// ret = pthread_join(plc_adc->thread, NULL);
		// assert(ret == 0);
		struct timespec timeout =
		{ 2, 0 };
		ret = pthread_timedjoin_np(plc_adc->thread, NULL, &timeout);
		// assert(ret == 0);
	}
	plc_adc->capture_started = 0;
}

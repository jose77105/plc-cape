/**
 * @file
 * @brief	**Main** file
 *
 * @see		@ref library-libplc-gpio
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

#define _GNU_SOURCE		// Required for 'asprintf' declaration
#include <errno.h>		// errno
#include <fcntl.h>
#include <poll.h>		// poll
#include <pthread.h>
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>
#include "+common/api/+base.h"
#include "+common/api/bbb.h"
#include "+common/api/error.h"
#include "libraries/libplc-gpio/api/gpio.h"
#include "libraries/libplc-tools/api/file.h"

// Code from 'http://stackoverflow.com/questions/13124271/driving-beaglebone-gpio1-through-dev-mem'
#define GPIO0_BASE 0x44E07000
#define GPIO1_BASE 0x4804C000
#define GPIO2_BASE 0x481AC000
#define GPIO3_BASE 0x481AE000

#define GPIO_SIZE  0x00000FFF

// OE: 0 is output, 1 is input
#define GPIO_OE 0x4d
#define GPIO_IN 0x4e
#define GPIO_OUT 0x4f
#define GPIO_CLEARDATAOUT 0x64
#define GPIO_SETDATAOUT 0x65

static const off_t bank_base_offset[] =
{ GPIO0_BASE, GPIO1_BASE, GPIO2_BASE, GPIO3_BASE };

static int soft_emulation = 0;

/****************************************************************
 * plc_gpio
 ****************************************************************/
struct plc_gpio
{
	unsigned *map[ARRAY_SIZE(bank_base_offset)];
};

ATTR_EXTERN void plc_gpio_set_soft_emulation(int soft_emulation_arg)
{
	soft_emulation = soft_emulation_arg;
}

ATTR_EXTERN struct plc_gpio *plc_gpio_create(void)
{
	struct plc_gpio *plc_gpio = (struct plc_gpio*) calloc(1, sizeof(struct plc_gpio));
	int last_error = 0;
	if (!soft_emulation)
	{
		int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
		// Enable all GPIO banks
		// Without this, access to deactivated banks (i.e. those with no clock source set up) will
		//	(logically) fail with SIGBUS
		// Idea from https://groups.google.com/forum/#!msg/beagleboard/OYFp4EXawiI/Mq6s3sg14HoJ
		//	system("echo 5 > /sys/class/gpio1/export");
		//	system("echo 65 > /sys/class/gpio1/export");
		//	system("echo 105 > /sys/class/gpio1/export");
		if (mem_fd < 0)
		{
			last_error = errno;
			assert(last_error != 0);
		}
		else
		{
			int n;
			for (n = 0; n < ARRAY_SIZE(plc_gpio->map); n++)
			{
				plc_gpio->map[n] = (unsigned *) mmap(0, GPIO_SIZE, PROT_READ | PROT_WRITE,
						MAP_SHARED, mem_fd, bank_base_offset[n]);
				if (plc_gpio->map[0] == MAP_FAILED)
				{
					last_error = errno;
					assert(last_error != 0);
					break;
				}
			}
			close(mem_fd);
		}
	}
	if (last_error == 0)
	{
		return plc_gpio;
	}
	else
	{
		free(plc_gpio);
		errno = last_error;
		return NULL;
	}
}

ATTR_EXTERN void plc_gpio_release(struct plc_gpio *plc_gpio)
{
	free(plc_gpio);
}

/****************************************************************
 * plc_gpio_pin_out
 ****************************************************************/
struct plc_gpio_pin_out
{
	volatile unsigned *addr_oe;
	volatile unsigned *addr_out;
	uint32_t pin_mask;
};

ATTR_EXTERN struct plc_gpio_pin_out *plc_gpio_pin_out_create(struct plc_gpio *plc_gpio,
		uint32_t pin_bank, uint32_t pin_mask)
{
	struct plc_gpio_pin_out *plc_gpio_pin_out = (struct plc_gpio_pin_out*) calloc(1,
			sizeof(struct plc_gpio_pin_out));
	if (!soft_emulation)
	{
		plc_gpio_pin_out->pin_mask = pin_mask;
		assert(pin_bank < ARRAY_SIZE(plc_gpio->map));
		// Always use the volatile pointer
		volatile unsigned *addr_gpio = plc_gpio->map[pin_bank];
		plc_gpio_pin_out->addr_oe = addr_gpio + GPIO_OE;
		plc_gpio_pin_out->addr_out = addr_gpio + GPIO_OUT;
		// Configure pin as output
		unsigned int creg = *plc_gpio_pin_out->addr_oe;
		creg = creg & ~pin_mask;
		*plc_gpio_pin_out->addr_oe = creg;
	}
	return plc_gpio_pin_out;
}

ATTR_EXTERN void plc_gpio_pin_out_release(struct plc_gpio_pin_out *plc_gpio_pin_out)
{
	free(plc_gpio_pin_out);
}

ATTR_EXTERN void plc_gpio_pin_out_set(struct plc_gpio_pin_out *plc_gpio_pin_out, int on)
{
	if (soft_emulation)
		return;
	if (on)
		*plc_gpio_pin_out->addr_out = *plc_gpio_pin_out->addr_out | plc_gpio_pin_out->pin_mask;
	else
		*plc_gpio_pin_out->addr_out = *plc_gpio_pin_out->addr_out & ~plc_gpio_pin_out->pin_mask;
}

ATTR_EXTERN void plc_gpio_pin_out_toggle(struct plc_gpio_pin_out *plc_gpio_pin_out)
{
	if (soft_emulation)
		return;
	*plc_gpio_pin_out->addr_out = *plc_gpio_pin_out->addr_out ^ plc_gpio_pin_out->pin_mask;
}

/****************************************************************
 * plc_gpio_pin_in
 ****************************************************************/
struct plc_gpio_pin_in
{
	volatile unsigned *addr_oe;
	volatile unsigned *addr_in;
	uint32_t pin_mask;
};

ATTR_EXTERN struct plc_gpio_pin_in *plc_gpio_pin_in_create(struct plc_gpio *plc_gpio,
		uint32_t pin_bank, uint32_t pin_mask)
{
	struct plc_gpio_pin_in *plc_gpio_pin_in =
			(struct plc_gpio_pin_in*) calloc(1, sizeof(struct plc_gpio_pin_in));
	if (!soft_emulation)
	{
		plc_gpio_pin_in->pin_mask = pin_mask;
		assert(pin_bank < ARRAY_SIZE(plc_gpio->map));
		// Always use the volatile pointer
		volatile unsigned *addr_gpio = plc_gpio->map[pin_bank];
		plc_gpio_pin_in->addr_oe = addr_gpio + GPIO_OE;
		plc_gpio_pin_in->addr_in = addr_gpio + GPIO_IN;
		// Configure pin as input
		unsigned int creg = *plc_gpio_pin_in->addr_oe;
		creg = creg | pin_mask;
		*plc_gpio_pin_in->addr_oe = creg;
	}
	return plc_gpio_pin_in;
}

ATTR_EXTERN void plc_gpio_pin_in_release(struct plc_gpio_pin_in *plc_gpio_pin_in)
{
	free(plc_gpio_pin_in);
}

ATTR_EXTERN int plc_gpio_pin_in_get(struct plc_gpio_pin_in *plc_gpio_pin_in)
{
	if (soft_emulation)
		return 0;
	return (*plc_gpio_pin_in->addr_in & plc_gpio_pin_in->pin_mask) != 0;
}

/****************************************************************
 * plc_gpio_sysfs_pin
 ****************************************************************/
static const char *gpio_sysfs_dir = "/sys/class/gpio/";

struct plc_gpio_sysfs_pin
{
	int gpio_number;
	char *sysfs_path;
	void (*callback)(void *data, int flag_status);
	void *callback_data;
	int pipe_to_callback[2];
	pthread_t thread;
	volatile int end_thread;
};

ATTR_EXTERN struct plc_gpio_sysfs_pin *plc_gpio_sysfs_pin_create(uint32_t gpio_number, int output)
{
	struct plc_gpio_sysfs_pin *plc_gpio_sysfs_pin = (struct plc_gpio_sysfs_pin*) calloc(1,
			sizeof(struct plc_gpio_sysfs_pin));
	if (!soft_emulation)
	{
		plc_gpio_sysfs_pin->gpio_number = gpio_number;
		plc_file_write_string_format(gpio_sysfs_dir, "export", "%d", gpio_number);
		asprintf(&plc_gpio_sysfs_pin->sysfs_path, "%sgpio%d/", gpio_sysfs_dir, gpio_number);
		plc_file_write_string(plc_gpio_sysfs_pin->sysfs_path, "direction", output ? "out" : "in");
		plc_file_write_string(plc_gpio_sysfs_pin->sysfs_path, "edge", "both");
	}
	return plc_gpio_sysfs_pin;
}

ATTR_EXTERN void plc_gpio_sysfs_pin_release(struct plc_gpio_sysfs_pin *plc_gpio_sysfs_pin)
{
	if (!soft_emulation)
	{
		if (plc_gpio_sysfs_pin->callback)
		{
			plc_gpio_sysfs_pin->end_thread = 1;
			char dummy = 0;
			// Write some data to wakeup the 'poll'
			write(plc_gpio_sysfs_pin->pipe_to_callback[1], &dummy, sizeof(dummy));
			int ret = pthread_join(plc_gpio_sysfs_pin->thread, NULL);
			assert(ret == 0);
			close(plc_gpio_sysfs_pin->pipe_to_callback[0]);
			close(plc_gpio_sysfs_pin->pipe_to_callback[1]);
		}
		plc_file_write_string_format(gpio_sysfs_dir, "unexport", "%d",
				plc_gpio_sysfs_pin->gpio_number);
		free(plc_gpio_sysfs_pin->sysfs_path);
	}
	free(plc_gpio_sysfs_pin);
}

ATTR_EXTERN int plc_gpio_sysfs_pin_get(struct plc_gpio_sysfs_pin *plc_gpio_sysfs_pin)
{
	if (soft_emulation)
		return 0;
	return plc_file_get_int(plc_gpio_sysfs_pin->sysfs_path, "value");
}

ATTR_EXTERN void plc_gpio_sysfs_pin_set(struct plc_gpio_sysfs_pin *plc_gpio_sysfs_pin, int on)
{
	if (soft_emulation)
		return;
	plc_file_write_string(plc_gpio_sysfs_pin->sysfs_path, "value", on ? "1" : "0");
}

static void *thread_gpio_pin(void *arg)
{
	struct plc_gpio_sysfs_pin *plc_gpio_sysfs_pin = arg;

	struct pollfd pollfdset[2];
	memset((void*) &pollfdset, 0, sizeof(pollfdset));

	char *gpio_value_path;
	asprintf(&gpio_value_path, "%sgpio%d/value", gpio_sysfs_dir, plc_gpio_sysfs_pin->gpio_number);
	pollfdset[0].fd = open(gpio_value_path, O_RDONLY | O_NONBLOCK);
	assert(pollfdset[0].fd >= 0);
	free(gpio_value_path);
	pollfdset[0].events = POLLPRI;
	// Use an auxiliar pipe to wakeup the 'poll' to terminate it nicely
	pollfdset[1].fd = plc_gpio_sysfs_pin->pipe_to_callback[0];
	pollfdset[1].events = POLLIN | POLLPRI;
	// Clear previous events
	static const int MAX_BUF = 64;
	char buf[MAX_BUF];
	int rc = poll(pollfdset, ARRAY_SIZE(pollfdset), 0);
	assert(rc >= 0);
	int len = read(pollfdset[0].fd, buf, MAX_BUF);
	while (!plc_gpio_sysfs_pin->end_thread)
	{
		pollfdset[0].revents = 0;
		rc = poll(pollfdset, ARRAY_SIZE(pollfdset), -1);
		len = read(pollfdset[0].fd, buf, MAX_BUF);
		if (pollfdset[0].revents & POLLPRI)
		{
			assert(len == 0);
			plc_gpio_sysfs_pin->callback(plc_gpio_sysfs_pin->callback_data,
					plc_gpio_sysfs_pin_get(plc_gpio_sysfs_pin));
		}
	}
	close(pollfdset[0].fd);
	return NULL;
}

ATTR_EXTERN void plc_gpio_sysfs_pin_callback(struct plc_gpio_sysfs_pin *plc_gpio_sysfs_pin,
		void (*callback)(void *data, int flag_status), void *data)
{
	if (soft_emulation)
		return;
	plc_gpio_sysfs_pin->callback = callback;
	plc_gpio_sysfs_pin->callback_data = data;
	// The recommended way to cancel a 'poll' is using an auxiliar 'pipe':
	//	* http://stackoverflow.com/questions/4048886/abort-linux-polling
	//	* http://stackoverflow.com/questions/28681076/...
	//		...c-pthreads-abort-poll-in-infinite-loop-from-main-thread
	//
	// Another possible way could be using a signal to interrupt the 'poll' but it
	// is not 100% because if the signal is fired before the 'poll' it is ignored
	//		static void sigusr_handler(int signo) {}
	//		...
	//		if (signal(SIGUSR1, sigusr_handler) == SIG_ERR)
	//			pexit("Cannot register the SIGINT handler");
	//		pthread_kill(plc_gpio_sysfs_pin->thread, SIGUSR1);
	int ret = pipe(plc_gpio_sysfs_pin->pipe_to_callback);
	assert(ret == 0);
	plc_gpio_sysfs_pin->end_thread = 0;
	ret = pthread_create(&plc_gpio_sysfs_pin->thread, NULL, thread_gpio_pin, plc_gpio_sysfs_pin);
	assert(ret == 0);
}

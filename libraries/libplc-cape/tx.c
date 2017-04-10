/**
 * @file
 * 
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>		// clock_gettime
#include "+common/api/+base.h"
#include "afe.h"
#include "api/afe.h"
#include "api/tx.h"
#include "error.h"
#include "libraries/libplc-tools/api/time.h"
#include "logger.h"
#include "spi.h"
#include "tx_sched.h"

ATTR_EXTERN const char *spi_tx_mode_enum_text[spi_tx_mode_COUNT] =
{ "none", "sample_by_sample", "buffer_by_buffer", "ping_pong_thread", "ping_pong_dma", };

struct plc_tx
{
	struct spi *spi;
	tx_fill_cycle_callback_t tx_fill_cycle_callback;
	tx_fill_cycle_callback_h tx_fill_cycle_callback_handle;
	tx_on_buffer_sent_callback_t tx_on_buffer_sent_callback;
	tx_on_buffer_sent_callback_h tx_on_buffer_sent_callback_handle;
	struct tx_sched *tx_sched;
	uint32_t tx_sched_buffers_len;
	pthread_t thread;
	volatile int end_thread;
	struct tx_statistics tx_statistics;
};

// TODO: Refactor. Don't use global variables nor signals here
uint32_t ping_pong_buffers_missed;

void sig_app_warning_handler(int signum)
{
	assert(signum == PLC_SIGNUM_APP_WARNING);
	// TODO: Encapsulate logging functionality as a global resource
	libplc_cape_log_line("APP WARNING!");
	// TODO: This really doesn't count 'ping_pong_buffers_missed' but 'ping_pong_warnings' at 1 Hz
	//	rate
	ping_pong_buffers_missed++;
}

void report_tx_statistics(struct tx_statistics *tx_stat, uint32_t buffers_overflow,
		uint32_t buffer_preparation_us, uint32_t buffer_cycle_us)
{
	tx_stat->buffers_handled++;
	tx_stat->buffers_overflow = buffers_overflow;
	tx_stat->buffer_preparation_us = buffer_preparation_us;
	tx_stat->buffer_cycle_us = buffer_cycle_us;
	// Ignore the first measurement (plc_tx.buffers_handled == 1) that could be affected by
	//	initialization procedures
	if (tx_stat->buffers_handled <= 2)
	{
		tx_stat->buffer_preparation_min_us = buffer_preparation_us;
		tx_stat->buffer_preparation_max_us = buffer_preparation_us;
		tx_stat->buffer_cycle_min_us = buffer_cycle_us;
		tx_stat->buffer_cycle_max_us = buffer_cycle_us;
	}
	else
	{
		if (buffer_preparation_us < tx_stat->buffer_preparation_min_us)
			tx_stat->buffer_preparation_min_us = buffer_preparation_us;
		if (buffer_preparation_us > tx_stat->buffer_preparation_max_us)
			tx_stat->buffer_preparation_max_us = buffer_preparation_us;
		if (buffer_cycle_us < tx_stat->buffer_cycle_min_us)
			tx_stat->buffer_cycle_min_us = buffer_cycle_us;
		if (buffer_cycle_us > tx_stat->buffer_cycle_max_us)
			tx_stat->buffer_cycle_max_us = buffer_cycle_us;
	}
}

// TODO: Refactor
void *thread_spi_tx_buffer(void *arg)
{
	// Boost 'self' thread
	// This is necessary because if in normal scheduling the thread can be interrupted easily
	// by other threads leading to missed filled buffers and incorrect output
	// For example, with a 'tx_sched_fill_buffer' taking 4ms (time for filling 1000 tx_sched with
	//	'sin' and 'float') this function is frequently interrupted leading to 'tx_sched_fill_buffer'
	//	lapses > 20ms which is over the time cycle of 750000 DAC bauds: tcycle = 1000*11/750000
	//	= 14.6ms
	struct sched_param param;
	// param.sched_priority = sched_get_priority_max(policy);
	param.sched_priority = 2;
	// Two realtime schedulers can be used:
	// * SCHED_RR: for round-robbin in threads with same priority
	// * SCHED_FIFO: active thread only is interrupted by higher priority ones
	int ret = sched_setscheduler(0, SCHED_RR, &param);
	assert(ret == 0);

	struct plc_tx *plc_tx = arg;
	// TODO: Do counter operations atomically
	//	http://gcc.gnu.org/onlinedocs/gcc-4.4.3/gcc/Atomic-Builtins.html
	ping_pong_buffers_missed = 0;
	struct timespec stamp_cycle = plc_time_get_hires_stamp();
	while (!plc_tx->end_thread)
	{
		struct timespec stamp_ini = plc_time_get_hires_stamp();
		plc_tx->tx_sched->tx_sched_fill_next_buffer(plc_tx->tx_sched);
		uint32_t buffer_preparation_us = plc_time_hires_interval_to_usec(stamp_ini,
				plc_time_get_hires_stamp());
		plc_tx->tx_sched->tx_sched_flush_and_wait_buffer(plc_tx->tx_sched);
		struct timespec stamp_cycle_new = plc_time_get_hires_stamp();
		// NOTE: 
		// Expected buffer cycle = plc_tx->tx_sched_buffers_len*11bits/sample/bauds
		//	If buffer = 1000 at 750000 bauds -> Time plc_tx buffer = 1000*11/750000 = 14.6 ms
		uint32_t buffer_cycle_us = plc_time_hires_interval_to_usec(stamp_cycle, stamp_cycle_new);
		stamp_cycle = stamp_cycle_new;
		// Monitor buffer in progress
		uint16_t *samples_buffer_to_tx = plc_tx->tx_sched->tx_sched_get_address_buffer_in_tx(
				plc_tx->tx_sched);
		plc_tx->tx_on_buffer_sent_callback(plc_tx->tx_on_buffer_sent_callback_handle,
				samples_buffer_to_tx, plc_tx->tx_sched_buffers_len);
		report_tx_statistics(&plc_tx->tx_statistics, ping_pong_buffers_missed,
				buffer_preparation_us, buffer_cycle_us);
	}
	return NULL;
}

ATTR_EXTERN struct plc_tx *plc_tx_create(tx_fill_cycle_callback_t tx_fill_cycle_callback,
		tx_fill_cycle_callback_h tx_fill_cycle_callback_handle,
		tx_on_buffer_sent_callback_t tx_on_buffer_sent_callback,
		tx_on_buffer_sent_callback_h tx_on_buffer_sent_callback_handle,
		enum spi_tx_mode_enum tx_mode, uint32_t tx_buffers_len, struct plc_afe *plc_afe)
{
	struct plc_tx *plc_tx = calloc(1, sizeof(struct plc_tx));
	plc_tx->tx_fill_cycle_callback = tx_fill_cycle_callback;
	plc_tx->tx_fill_cycle_callback_handle = tx_fill_cycle_callback_handle;
	plc_tx->tx_on_buffer_sent_callback = tx_on_buffer_sent_callback;
	plc_tx->tx_on_buffer_sent_callback_handle = tx_on_buffer_sent_callback_handle;
	plc_tx->spi = plc_afe_get_spi(plc_afe);
	plc_tx->tx_sched_buffers_len = tx_buffers_len;
	if ((tx_mode == spi_tx_mode_ping_pong_thread) || (tx_mode == spi_tx_mode_ping_pong_dma))
	{
		// Signal kernel->user
		if (signal(PLC_SIGNUM_APP_WARNING, sig_app_warning_handler) == SIG_ERR)
		{
			libplc_cape_set_error_msg("Cannot register the SIGIO handler");
			free(plc_tx);
			return NULL;
		}
	}
	switch (tx_mode)
	{
	case spi_tx_mode_sample_by_sample:
		plc_tx->tx_sched = tx_sched_sync_create(tx_fill_cycle_callback,
				tx_fill_cycle_callback_handle, plc_tx->spi, plc_tx->tx_sched_buffers_len, 1);
		break;
	case spi_tx_mode_buffer_by_buffer:
		plc_tx->tx_sched = tx_sched_sync_create(tx_fill_cycle_callback,
				tx_fill_cycle_callback_handle, plc_tx->spi, plc_tx->tx_sched_buffers_len, 0);
		break;
	case spi_tx_mode_ping_pong_thread:
		plc_tx->tx_sched = tx_sched_async_thread_create(tx_fill_cycle_callback,
				tx_fill_cycle_callback_handle, plc_tx->spi, plc_tx->tx_sched_buffers_len);
		break;
	case spi_tx_mode_ping_pong_dma:
		plc_tx->tx_sched = tx_sched_async_dma_create(tx_fill_cycle_callback,
				tx_fill_cycle_callback_handle, plc_tx->spi, plc_tx->tx_sched_buffers_len);
		break;
	default:
		assert(0);
		break;
	}
	return plc_tx;
}

ATTR_EXTERN void plc_tx_release(struct plc_tx *plc_tx)
{
	__sighandler_t sig_res = signal(PLC_SIGNUM_APP_WARNING, SIG_DFL);
	assert(sig_res != SIG_ERR);
	if (plc_tx->tx_sched)
		plc_tx->tx_sched->tx_sched_release(plc_tx->tx_sched);
	free(plc_tx);
}

// POSTCONDITION: 'rx_statistics' items must be atomic but it's not required for the whole struct
//	(for performance and simplicity)
ATTR_EXTERN const struct tx_statistics *tx_get_tx_statistics(struct plc_tx *plc_tx)
{
	return &plc_tx->tx_statistics;
}

ATTR_EXTERN void plc_tx_fill_buffer_iteration(struct plc_tx *plc_tx, uint16_t *buffer,
		uint32_t buffer_samples)
{
	plc_tx->tx_fill_cycle_callback(plc_tx->tx_fill_cycle_callback_handle, buffer, buffer_samples);
}

ATTR_EXTERN void plc_tx_start_transmission(struct plc_tx *plc_tx)
{
	memset(&plc_tx->tx_statistics, 0, sizeof(plc_tx->tx_statistics));
	plc_tx->tx_sched->tx_sched_start(plc_tx->tx_sched);
	int ret;
	// Boost the priority of the created thread
	// However, with 'ps -eLF | grep plc' we see this doesn't seem to work (all threads is 'TS'
	//	class)
	// To set priority we can use:
	//	* 'pthread_setschedprio'
	//	* 'pthread_setschedparam(pthread_self(), policy, &param)'
	// More info: http://man7.org/linux/man-pages/man7/sched.7.html
	/*
	 pthread_attr_t attr;
	 pthread_attr_init(&attr);
	 // 'SCHED_FIFO' seems to give better results than 'SCHED_RR' although
	 // it seems that there still are other higher priority threads with a time slice > 10 ms
	 // To get current policy use 'pthread_attr_getschedpolicy(&attr, &policy)'s
	 int policy = SCHED_FIFO;
	 ret = pthread_attr_setschedpolicy(&attr, policy);
	 assert(ret == 0);
	 struct sched_param param;
	 param.sched_priority = sched_get_priority_max(policy);
	 ret = pthread_attr_setschedparam(&attr, &param);
	 assert(ret == 0);
	 ret = pthread_create(&plc_tx->thread, &attr, thread_spi_tx_buffer, plc_tx);
	 pthread_attr_destroy(&attr);
	 */
	plc_tx->end_thread = 0;
	ret = pthread_create(&plc_tx->thread, NULL, thread_spi_tx_buffer, plc_tx);
	assert(ret == 0);
}

ATTR_EXTERN void plc_tx_stop_transmission(struct plc_tx *plc_tx)
{
	// Caution: 'plc_tx->end_thread' must be called before 'tx_sched_stop' to avoid
	// the 'thread_spi_tx_buffer' real-time priority monopolize control
	plc_tx->end_thread = 1;
	plc_tx->tx_sched->tx_sched_stop(plc_tx->tx_sched);
	int ret = pthread_join(plc_tx->thread, NULL);
	assert(ret == 0);
}

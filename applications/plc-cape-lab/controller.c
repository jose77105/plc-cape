/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#define _GNU_SOURCE				// Required for 'asprintf' declaration
#include <signal.h>				// SIGEV_THREAD
#include <time.h>				// CLOCK_REALTIME
#include <unistd.h>
#include "+common/api/+base.h"
#include "+common/api/ui.h"		// event_id_tx_flag
#include "common.h"
#include "controller.h"
#include "decoder.h"
#include "encoder.h"
#include "libraries/libplc-adc/api/adc.h"
#include "libraries/libplc-cape/api/afe.h"
// Castings to simplify usage of 'cape.h'
#define TX_NODEF_FILL_CYCLE_CALLBACK_HANDLE
#define TX_NODEF_ON_BUFFER_SENT_CALLBACK_HANDLE
typedef struct encoder *tx_fill_cycle_callback_h;
typedef struct monitor *tx_on_buffer_sent_callback_h;
#include "libraries/libplc-cape/api/cape.h"
#include "libraries/libplc-cape/api/leds.h"
#include "libraries/libplc-cape/api/tx.h"
#include "libraries/libplc-tools/api/plugin.h"
#include "libraries/libplc-tools/api/time.h"
#include "rx.h"
#include "settings.h"
#include "ui.h"

#define SIG_COMUNICATION_TIMER SIGRTMIN
#define SAMPLES_TO_FILE_DEFAULT 10000
#define SUPERVISOR_PERIOD_MS 500

#define ENCODER_WAVE "encoder-wave"
#define ENCODER_OOK "encoder-ook"
#define ENCODER_PWM "encoder-pwm"
#define DECODER_PWM "decoder-pwm"
#define DECODER_OOK "decoder-ook"

ATTR_EXTERN const char *configuration_profile_enum_text[cfg_COUNT] =
{ "none", "txrx_cal_sin_2kHz_defaults", "txrx_cal_sin_2kHz", "txrx_cal_ramp",
		"txrx_cal_sin_ook_10kHz", "txrx_cal_sin_ook_Hi_10kHz", "txrx_cal_msg_ook_10kHz_preload",
		"txrx_cal_msg_pwm_10kHz_preload", "txrx_cal_filter_msg_pwm_110kHz_preload",
		"txrx_cal_filter_msg_pwm_110kHz_preload_timed", "tx_sin_2kHz", "tx_sin_100kHz_3Mbps",
		"tx_file_93750bps", "tx_freq_sweep", "tx_pa_sin_10kHz", "tx_pa_sin_110kHz_6Mbps_preload",
		"tx_pa_sin_quarter", "tx_pa_freq_sweep", "tx_pa_freq_sweep_preload", "tx_pa_sin_ook_20kHz",
		"tx_pa_msg_pwm_110kHz_preload", "tx_pa_constant", "tx_pa_rx_sin_2kHz",
		"tx_pa_rx_sin_ook_9kHz", "rx", "cfg_rx_msg_pwm_110kHz" };

static struct plc_cape *plc_cape = NULL;
static struct plc_afe *plc_afe = NULL;
static struct plc_leds *plc_leds = NULL;
static struct plc_adc *plc_adc = NULL;
static struct rx *rx = NULL;
static struct plc_tx *plc_tx = NULL;
static struct encoder *encoder = NULL;
sample_tx_t *tx_preload_buffer = NULL;
static struct decoder *decoder = NULL;
static struct monitor *monitor = NULL;
static struct plc_plugin_list *encoder_plugins = NULL;
static struct plc_plugin_list *decoder_plugins = NULL;
static int controller_activated = 0;
static int controller_communication_in_progress = 0;
static timer_t supervisor_timer_id = 0;
static int use_communication_timer = 0;
static timer_t communication_timer_id = 0;
static int stop_communication_on_timer = 0;
static struct itimerspec its_next_start;
static struct itimerspec its_next_stop;

// 'settings' linked
static struct settings *settings = NULL;
static struct settings *settings_defaults = NULL;
static struct ui *ui = NULL;

// Forward declarations
static void controller_initialize(void);
static void controller_terminate(void);
static void controller_stop_communication(void);
static void controller_set_configuration_profile_in_stop(
		enum configuration_profile_enum configuration_profile);

char *controller_get_settings_text(void)
{
	return settings_get_info(settings);
}

static void controller_flags_callback(void *data, int tx_flag, int rx_flag, int ok_flag)
{
	ui_set_event(ui, event_id_tx_flag, tx_flag);
	ui_set_event(ui, event_id_rx_flag, rx_flag);
	ui_set_event(ui, event_id_ok_flag, ok_flag);
}

void controller_create(struct settings *settings_arg, struct ui *ui_arg)
{
	TRACE(3, "Creating controller");
	// 'settings' will keep a link to an existing object
	settings = settings_arg;
	// 'settings_defaults' is a new object to be released when no longer required
	settings_defaults = settings_clone(settings);
	ui = ui_arg;

	// Initialize device
	TRACE(3, "Creating 'plc_cape'");
	plc_cape = plc_cape_create(!settings->std_driver);
	if (plc_cape == NULL)
		log_and_exit(get_last_error());
	plc_afe = plc_cape_get_afe(plc_cape);
	plc_leds = plc_cape_get_leds(plc_cape);

	TRACE(3, "Creating 'monitor'");
	monitor = monitor_create(ui);

	TRACE(3, "Loading lists of plugins");
	encoder_plugins = plc_plugin_list_create(plc_plugin_category_encoder);
	decoder_plugins = plc_plugin_list_create(plc_plugin_category_decoder);

	TRACE(3, "Configuring profile");
	if (settings->configuration_profile != cfg_none)
		controller_set_configuration_profile_in_stop(settings->configuration_profile);

	TRACE(3, "Starting controller");
	controller_initialize();

	TRACE(3, "Controller started");
}

void controller_release(void)
{
	controller_terminate();
	if (decoder_plugins)
	{
		plc_plugin_list_release(decoder_plugins);
		decoder_plugins = NULL;
	}
	if (encoder_plugins)
	{
		plc_plugin_list_release(encoder_plugins);
		encoder_plugins = NULL;
	}
	if (monitor)
	{
		monitor_release(monitor);
		monitor = NULL;
	}
	if (plc_cape)
	{
		plc_cape_release(plc_cape);
		plc_cape = NULL;
	}
	if (settings_defaults)
	{
		settings_release(settings_defaults);
		settings_defaults = NULL;
	}
}

static void controller_initialize(void)
{
	TRACE(3, "Starting controller");
	// Process settings when reinitializing the controller to refresh dependent data
	settings_update(settings);
	TRACE(3, "Settings updated");
	if (plc_afe)
	{
		plc_afe_configure_spi(plc_afe, settings->tx.freq_bps, settings->tx.samples_delay_us);
		TRACE(3, "SPI configured");
		plc_afe_set_standy(plc_afe, 0);
		struct afe_settings afe_settings;
		afe_settings.cenelec_a = settings->cenelec_a;
		afe_settings.gain_tx_pga = settings->tx.gain_tx_pga;
		afe_settings.gain_rx_pga1 = settings->rx.gain_rx_pga1;
		afe_settings.gain_rx_pga2 = settings->rx.gain_rx_pga2;
		plc_afe_configure(plc_afe, &afe_settings);
		plc_afe_set_flags_callback(plc_afe, controller_flags_callback, NULL);
		TRACE(3, "CAPE configured");
	}
	if (settings->tx.tx_mode != spi_tx_mode_none)
	{
		const char *plugin_name = encoder_plugins->list[encoder_plugins->active_index];
		char *plugin_path = plc_plugin_get_abs_path(plc_plugin_category_encoder, plugin_name);
		encoder = encoder_create(plugin_path);
		free(plugin_path);
		// In file transmission use the whole page as the intermediate buffer
		uint32_t tx_buffers_len =
				(settings->tx.stream_type == stream_file) ?
						getpagesize() / sizeof(uint16_t) : settings->tx.tx_buffers_len;
		plc_tx = plc_tx_create(encoder_prepare_next_samples, encoder, monitor_on_buffer_sent,
				monitor, settings->tx.tx_mode, tx_buffers_len, plc_afe);
		if (plc_tx == NULL)
			log_and_exit("Error at 'plc_tx' component initialization");
		if (strcmp(plugin_name, ENCODER_OOK) == 0)
			encoder_configure_OOK(encoder, settings->tx.samples_offset, settings->tx.samples_range,
					settings->tx.samples_freq, settings->data_bit_us, settings->tx.message_to_send);
		else if (strcmp(plugin_name, ENCODER_PWM) == 0)
			encoder_configure_PWM(encoder, settings->tx.samples_offset, settings->tx.samples_range,
					settings->tx.samples_freq, settings->data_bit_us, settings->tx.message_to_send);
		else
			encoder_configure_WAVE(encoder, settings->tx.stream_type, settings->tx.samples_offset,
					settings->tx.samples_range, settings->tx.samples_freq,
					settings->tx.dac_filename, settings->data_bit_us);
		monitor_set_tx(monitor, plc_tx);
		TRACE(3, "TX mode prepared");
	}
	if (settings->rx.rx_mode != rx_mode_none)
	{
		const char *plugin_name = decoder_plugins->list[decoder_plugins->active_index];
		char *plugin_path = plc_plugin_get_abs_path(plc_plugin_category_decoder, plugin_name);
		decoder = decoder_create(plugin_path);
		free(plugin_path);

		plc_adc = plc_adc_create(!settings->std_driver);
		monitor_set_adc(monitor, plc_adc);
		struct rx_settings rx_settings;
		rx_settings.rx_mode = settings->rx.rx_mode;
		rx_settings.samples_to_file = settings->rx.samples_to_file;
		rx_settings.demod_mode = settings->rx.demod_mode;
		rx_settings.data_bit_us = settings->data_bit_us;
		rx_settings.demod_data_hi_threshold = settings->rx.demod_data_hi_threshold;
		rx_settings.data_offset = settings->rx.data_offset;
		rx_settings.data_hi_threshold_detection = settings->rx.data_hi_threshold_detection;
		rx = rx_create(&rx_settings, monitor, plc_leds, plc_adc, decoder);
		TRACE(3, "TX mode prepared");
	}
	monitor_set_profile(monitor, settings->monitor_profile);
}

static void controller_terminate(void)
{
	if (controller_activated)
		controller_deactivate();
	if (rx)
	{
		rx_release(rx);
		rx = NULL;
	}
	if (decoder)
	{
		decoder_release(decoder);
		decoder = NULL;
	}
	if (plc_tx)
	{
		plc_tx_release(plc_tx);
		plc_tx = NULL;
	}
	if (encoder)
	{
		encoder_release(encoder);
		encoder = NULL;
	}
	if (plc_adc)
	{
		plc_adc_release(plc_adc);
		plc_adc = NULL;
	}
	if (plc_afe)
	{
		plc_afe_disable_all(plc_afe);
		plc_afe_set_standy(plc_afe, 1);
	}
	if (monitor)
	{
		monitor_set_adc(monitor, NULL);
		monitor_set_tx(monitor, NULL);
	}
}

static void controller_start_communication(void)
{
	assert(!controller_communication_in_progress);
	log_line("AFE active");
	if (plc_afe)
	{
		enum afe_calibration_enum afe_calibration_mode = afe_calibration_none;
		enum afe_block_enum afe_blocks = afe_block_none;
		switch (settings->operating_mode)
		{
		case operating_mode_none:
			break;
		case operating_mode_tx_dac:
			afe_blocks = afe_block_dac;
			break;
		case operating_mode_tx_dac_txpga_txfilter:
			afe_blocks = afe_block_dac | afe_block_tx | afe_block_ref2;
			break;
		case operating_mode_tx_dac_txpga_txfilter_pa:
			afe_blocks = afe_block_dac | afe_block_tx | afe_block_pa | afe_block_ref1
					| afe_block_ref2 | afe_block_pa_out;
			break;
		case operating_mode_tx_dac_txpga_txfilter_pa_rx:
			afe_blocks = afe_block_dac | afe_block_tx | afe_block_pa | afe_block_ref1
					| afe_block_ref2 | afe_block_pa_out | afe_block_rx;
			break;
		case operating_mode_calib_dac_txpga:
			afe_blocks = afe_block_dac | afe_block_tx | afe_block_ref2;
			afe_calibration_mode = afe_calibration_dac_txpga;
			break;
		case operating_mode_calib_dac_txpga_txfilter:
			afe_blocks = afe_block_dac | afe_block_tx | afe_block_ref2;
			afe_calibration_mode = afe_calibration_dac_txpga_txfilter;
			break;
		case operating_mode_calib_dac_txpga_rxpga1_rxfilter_rxpag2:
			afe_blocks = afe_block_dac | afe_block_tx | afe_block_ref2;
			afe_calibration_mode = afe_calibration_dac_txpga_rxpga1_rxfilter_rxpga2;
			break;
		case operating_mode_rx:
			afe_blocks = afe_block_rx | afe_block_ref2;
			break;
		case operating_mode_COUNT:
		default:
			assert(0);
		}
		plc_afe_activate_blocks(plc_afe, afe_blocks);
		plc_afe_set_calibration_mode(plc_afe, afe_calibration_mode);
		plc_afe_set_dac_mode(plc_afe, 1);
	}
	if (settings->tx.tx_mode != spi_tx_mode_none)
	{
		encoder_reset(encoder);
		if (settings->tx.preload_buffer_len > 0)
		{
			assert(tx_preload_buffer == NULL);
			log_format("Preloading buffer: %u samples...\n", settings->tx.preload_buffer_len);
			struct timespec stamp_ini = plc_time_get_hires_stamp();
			tx_preload_buffer = malloc(settings->tx.preload_buffer_len * sizeof(sample_tx_t));
			encoder_prepare_next_samples(encoder, tx_preload_buffer,
					settings->tx.preload_buffer_len);
			uint32_t buffer_preparation_us = plc_time_hires_interval_to_usec(stamp_ini,
					plc_time_get_hires_stamp());
			log_format("Buffer preloaded in %u us\n", buffer_preparation_us);
			encoder_set_copy_mode(encoder, tx_preload_buffer, settings->tx.preload_buffer_len, 1);
		}
		if (settings->tx.stream_type == stream_file)
			log_format("Data retrieved from '%s'\n", settings->tx.dac_filename);
		plc_tx_start_transmission(plc_tx);
		plc_leds_set_tx_activity(plc_leds, 1);
	}
	if (settings->rx.rx_mode != rx_mode_none)
		rx_start_capture(rx);
	monitor_start(monitor);
	controller_communication_in_progress = 1;
}

static void controller_stop_communication(void)
{
	assert(controller_communication_in_progress);
	if (settings->rx.rx_mode != rx_mode_none)
		rx_stop_capture(rx);
	if (settings->tx.tx_mode != spi_tx_mode_none)
	{
		plc_tx_stop_transmission(plc_tx);
		plc_leds_set_tx_activity(plc_leds, 0);
		if (tx_preload_buffer)
		{
			encoder_set_copy_mode(encoder, NULL, 0, 0);
			free(tx_preload_buffer);
			tx_preload_buffer = NULL;
		}
	}
	monitor_stop(monitor);
	if (plc_afe)
	{
		plc_afe_set_dac_mode(plc_afe, 0);
		plc_afe_disable_all(plc_afe);
	}
	log_line("AFE idle");
	controller_communication_in_progress = 0;
}

static void sigev_timer_communication_thread(union sigval data)
{
	if (stop_communication_on_timer)
	{
		controller_stop_communication();
		if (settings->communication_interval_ms > 0)
		{
			int ret = timer_settime(communication_timer_id, 0, &its_next_start, NULL);
			assert(ret == 0);
		}
		else
		{
			controller_deactivate();
		}
	}
	else
	{
		controller_start_communication();
		int ret = timer_settime(communication_timer_id, 0, &its_next_stop, NULL);
		assert(ret == 0);
	}
	stop_communication_on_timer = !stop_communication_on_timer;
}

static void sigev_timer_supervisor_thread(union sigval data)
{
	plc_leds_toggle_app_activity(plc_leds);
}

static void create_periodic_supervisor(void)
{
	int ret;
	struct itimerspec its;
	struct sigevent sev;
	sev.sigev_notify = SIGEV_THREAD;
	sev.sigev_notify_function = sigev_timer_supervisor_thread;
	sev.sigev_notify_attributes = NULL;
	ret = timer_create(CLOCK_REALTIME, &sev, &supervisor_timer_id);
	assert(ret == 0);
	long long freq_nanosecs = (long long)1000000 * SUPERVISOR_PERIOD_MS;
	its.it_value.tv_sec = freq_nanosecs / 1000000000;
	its.it_value.tv_nsec = freq_nanosecs % 1000000000;
	its.it_interval.tv_sec = its.it_value.tv_sec;
	its.it_interval.tv_nsec = its.it_value.tv_nsec;
	ret = timer_settime(supervisor_timer_id, 0, &its, NULL);
	assert(ret == 0);
}

static void terminate_periodic_supervisor(void)
{
	int ret = timer_delete(supervisor_timer_id);
	assert(ret == 0);
	plc_leds_set_app_activity(plc_leds, 1);
}

// Marked as 'async' because for periodic interval the controller will be activated some time later
void controller_activate_async(void)
{
	if (controller_activated)
	{
		log_line("Controller already ON");
		return;
	}
	if (settings->communication_interval_ms > 0)
	{
		// For periodic communication at intervals it is required that 'communication_timeout_ms'
		// has a value greater than 0 (i.e. timeout enabled) and smaller than
		// 'communication_interval_ms'
		if ((settings->communication_timeout_ms == 0)
				|| (settings->communication_timeout_ms >= settings->communication_interval_ms))
		{
			log_line("The 'communication duration' setting doesn't allow periodic communication");
			log_line("Revise the current settings. Communication not initiated");
			return;
		}
	}
	log_line("Controller ON");
	create_periodic_supervisor();
	controller_activated = 1;
	use_communication_timer = 0;
	if (settings->communication_timeout_ms > 0)
	{
		long long freq_nanosecs = (long long)1000000 * settings->communication_timeout_ms;
		its_next_stop.it_value.tv_sec = freq_nanosecs / 1000000000;
		its_next_stop.it_value.tv_nsec = freq_nanosecs % 1000000000;
		its_next_stop.it_interval.tv_sec = 0;
		its_next_stop.it_interval.tv_nsec = 0;
		use_communication_timer = 1;
	}
	if (settings->communication_interval_ms > 0)
	{
		long long freq_nanosecs = (long long)1000000 *
				(settings->communication_interval_ms - settings->communication_timeout_ms);
		its_next_start.it_value.tv_sec = freq_nanosecs / 1000000000;
		its_next_start.it_value.tv_nsec = freq_nanosecs % 1000000000;
		its_next_start.it_interval.tv_sec = 0;
		its_next_start.it_interval.tv_nsec = 0;
		use_communication_timer = 1;
	}
	if (use_communication_timer) 
	{
		struct sigevent sev;
		sev.sigev_notify = SIGEV_THREAD;
		sev.sigev_notify_function = sigev_timer_communication_thread;
		sev.sigev_notify_attributes = NULL;
		int ret = timer_create(CLOCK_REALTIME, &sev, &communication_timer_id);
		assert(ret == 0);
	}
	if (settings->communication_interval_ms > 0)
	{
		// To avoid timer overlaping when using two timers, only one timer is used switching between
		// time-to-stop and time-to-start
		// It is not an accurate timer but it is not required and preferred for simplicity
		stop_communication_on_timer = 0;
		int ret = timer_settime(communication_timer_id, 0, &its_next_start, NULL);
		assert(ret == 0);
	}
	else
	{
		if (settings->communication_timeout_ms > 0)
		{
			TRACE1(1, "Init timer %d", its_next_stop.it_value.tv_sec);
			stop_communication_on_timer = 1;
			int ret = timer_settime(communication_timer_id, 0, &its_next_stop, NULL);
			assert(ret == 0);
		}
		controller_start_communication();
	}
}

void controller_deactivate(void)
{
	if (!controller_activated)
	{
		log_line("Controller already OFF");
		return;
	}
	if (use_communication_timer)
	{
		int ret = timer_delete(communication_timer_id);
		assert(ret == 0);
	}
	// In timeout or periodic modes the communication can have been already stopped
	if (controller_communication_in_progress)
	{
		controller_stop_communication();
	}
	controller_activated = 0;
	terminate_periodic_supervisor();
	log_line("Controller OFF");
}

int controlles_is_active(void)
{
	return controller_activated;
}

void controller_update_configuration(void)
{
	controller_terminate();
	controller_initialize();
}

static void controller_set_default_settings(void)
{
	settings_copy(settings, settings_defaults);
}

static void controller_set_default_calibration_settings(void)
{
	controller_set_default_settings();
	settings->operating_mode = operating_mode_calib_dac_txpga;
	settings->tx.tx_mode = spi_tx_mode_ping_pong_dma;
	settings->rx.rx_mode = rx_mode_kernel_buffering;
	settings->tx.samples_offset = 800;
	settings->tx.samples_range = 400;
	settings->rx.demod_data_hi_threshold = 50;
	settings->rx.data_offset = 1100;
	settings->rx.data_hi_threshold_detection = 20;
	if (settings->rx.samples_to_file == 0)
		settings->rx.samples_to_file = SAMPLES_TO_FILE_DEFAULT;
}

static void controller_set_default_txpga_settings(void)
{
	controller_set_default_settings();
	settings->operating_mode = operating_mode_tx_dac_txpga_txfilter;
	settings->tx.tx_mode = spi_tx_mode_ping_pong_dma;
	settings->rx.rx_mode = rx_mode_none;
	settings->tx.samples_offset = 800;
	settings->tx.samples_range = 400;
}

static void controller_set_default_tx_pa_settings(void)
{
	controller_set_default_settings();
	settings->operating_mode = operating_mode_tx_dac_txpga_txfilter_pa;
	settings->tx.tx_mode = spi_tx_mode_ping_pong_dma;
	settings->rx.rx_mode = rx_mode_none;
	settings->tx.samples_offset = 500;
	settings->tx.samples_range = 100;
}

static void controller_set_default_tx_pa_rx_settings(void)
{
	controller_set_default_settings();
	settings->operating_mode = operating_mode_tx_dac_txpga_txfilter_pa_rx;
	settings->tx.tx_mode = spi_tx_mode_ping_pong_dma;
	settings->rx.rx_mode = rx_mode_kernel_buffering;
	settings->tx.samples_offset = 500;
	settings->tx.samples_range = 100;
	settings->rx.demod_data_hi_threshold = 50;
	settings->rx.data_offset = 1885;
	settings->rx.data_hi_threshold_detection = 20;
}

static void controller_set_default_rx_settings(void)
{
	controller_set_default_settings();
	settings->operating_mode = operating_mode_rx;
	settings->tx.tx_mode = spi_tx_mode_none;
	settings->rx.rx_mode = rx_mode_kernel_buffering;
	settings->rx.demod_mode = demod_mode_deferred;
	settings->rx.demod_data_hi_threshold = 50;
	settings->rx.data_offset = 1885;
	settings->rx.data_hi_threshold_detection = 20;
}

static void select_encoder(const char *name)
{
	encoder_plugins->active_index = plc_plugin_list_find_name(encoder_plugins, name);
	if (encoder_plugins->active_index == -1)
		log_and_exit("Required encoder-plugin is not present");
}

static void select_decoder(const char *name)
{
	decoder_plugins->active_index = plc_plugin_list_find_name(decoder_plugins, name);
	if (decoder_plugins->active_index == -1)
		log_and_exit("Required decoder-plugin is not present");
}

// TODO: Refactor
#include "plugins/encoder/api/encoder.h"	// stream_freq_sinus
static void controller_set_configuration_profile_in_stop(
		enum configuration_profile_enum configuration_profile)
{
	// TODO: Permitir especificar encoder & decoder = NULL con significado no generar input ni
	//	output
	char *encoder_plugin = ENCODER_WAVE;
	char *decoder_plugin = DECODER_PWM;
	// Set defaults
	switch (configuration_profile)
	{
	case cfg_txrx_cal_sin_2kHz_defaults:
		controller_set_default_calibration_settings();
		settings->tx.stream_type = stream_freq_sinus;
		settings->tx.samples_freq = 2000;
		settings->tx.freq_bps = 750000;
		settings->tx.tx_buffers_len = 1000;
		break;
	case cfg_txrx_cal_sin_2kHz:
		controller_set_default_calibration_settings();
		settings->tx.stream_type = stream_freq_sinus;
		settings->tx.samples_freq = 2000;
		break;
	case cfg_txrx_cal_ramp:
		controller_set_default_calibration_settings();
		settings->tx.stream_type = stream_ramp;
		// TODO: Revise. Maximum swing on Calibration with TX_PGA_GAIN=0.25 experimentally found for
		//	Offset = 700 and Range = 600
		//	It seems that for values below < 400 there is a lot of noise at the output
		//	For a GAIN=0.5 the Offset=700 and Range=600 saturates 25% on output at top values
		//		ADC=2750 (= 1.8/4096*2750 = 1.2V aprox)
		//	To get maximum ADC swing use Offset=600 Range=450 (ADC=2250..2750).
		//		Note 500 ADC values = 500*1.8/4096 = 220mV swing
		//	For a GAIN=0.71 the Offset=700 and Range=600 saturates 25% on output at top values
		//		ADC=2750
		//	To get maximum ADC swing use Offset=502 Range=300 (ADC=2250..2750)
		//	Curious effect!!
		//		* If O=452 and R=200 -> ADC=2250..2600 ramp ok
		//		* If O=451 and R=200 -> ADC almost < 20 (noise) all the time
		//		* If O=501 and R=300 -> ADC on 0V more than 75% of ramp-cycle
		//	It seems that under some value -> wrong behavior (maybe a hidden command!)
		settings->tx.samples_offset = 700;
		settings->tx.samples_range = 600;
		break;
	case cfg_txrx_cal_wave_ook_10kHz:
		controller_set_default_calibration_settings();
		settings->tx.stream_type = stream_ook_pattern;
		settings->tx.samples_freq = 10000;
		break;
	case cfg_txrx_cal_wave_ook_Hi_10kHz:
		controller_set_default_calibration_settings();
		settings->tx.stream_type = stream_ook_hi;
		settings->tx.samples_freq = 10000;
		settings->rx.demod_mode = demod_mode_real_time;
		settings->tx.preload_buffer_len = 50000;
		encoder_plugin = ENCODER_OOK;
		decoder_plugin = DECODER_OOK;
		break;
	case cfg_txrx_cal_msg_ook_10kHz_preload:
		controller_set_default_calibration_settings();
		settings->tx.samples_freq = 10000;
		settings->rx.demod_mode = demod_mode_real_time;
		settings->tx.preload_buffer_len = 50000;
		encoder_plugin = ENCODER_OOK;
		decoder_plugin = DECODER_OOK;
		break;
	case cfg_txrx_cal_msg_pwm_10kHz_preload:
		controller_set_default_calibration_settings();
		settings->tx.samples_freq = 10000;
		settings->rx.demod_mode = demod_mode_real_time;
		settings->tx.preload_buffer_len = 50000;
		settings->rx.data_offset = 1141;
		settings->data_bit_us = 100;
		encoder_plugin = ENCODER_PWM;
		decoder_plugin = DECODER_PWM;
		break;
	case cfg_txrx_cal_filter_msg_pwm_110kHz_preload:
		controller_set_default_calibration_settings();
		settings->operating_mode = operating_mode_calib_dac_txpga_txfilter;
		settings->tx.samples_freq = 110000;
		settings->tx.freq_bps = 6000000;
		settings->rx.demod_mode = demod_mode_real_time;
		settings->tx.preload_buffer_len = 200000;
		settings->rx.data_offset = 1667;
		settings->data_bit_us = 100;
		encoder_plugin = ENCODER_PWM;
		decoder_plugin = DECODER_PWM;
		break;
	case cfg_txrx_cal_filter_msg_pwm_110kHz_preload_timed:
		controller_set_default_calibration_settings();
		settings->operating_mode = operating_mode_calib_dac_txpga_txfilter;
		settings->tx.samples_freq = 110000;
		settings->tx.freq_bps = 6000000;
		settings->rx.demod_mode = demod_mode_real_time;
		settings->tx.preload_buffer_len = 200000;
		settings->rx.data_offset = 1667;
		settings->data_bit_us = 100;
		settings->communication_timeout_ms = 500;
		settings->communication_interval_ms = 5000;
		encoder_plugin = ENCODER_PWM;
		decoder_plugin = DECODER_PWM;
		break;
	case cfg_tx_sin_2kHz:
		controller_set_default_txpga_settings();
		settings->tx.stream_type = stream_freq_sinus;
		settings->tx.samples_freq = 2000;
		break;
	case cfg_tx_file_93750bps:
		controller_set_default_txpga_settings();
		settings->tx.stream_type = stream_file;
		settings->tx.freq_bps = 93750;
		break;
	case cfg_tx_freq_sweep:
		controller_set_default_txpga_settings();
		settings->tx.stream_type = stream_freq_sweep;
		break;
	case cfg_tx_sin_100kHz_3Mbps:
		controller_set_default_txpga_settings();
		settings->tx.stream_type = stream_freq_sinus;
		settings->tx.samples_freq = 100000;
		settings->tx.freq_bps = 3000000;
		break;
	case cfg_tx_pa_sin_10kHz:
		controller_set_default_tx_pa_settings();
		settings->tx.stream_type = stream_freq_sinus;
		settings->tx.samples_freq = 10000;
		break;
	case cfg_tx_pa_sin_110kHz_6Mbps_preload:
		controller_set_default_tx_pa_settings();
		settings->tx.stream_type = stream_freq_sinus;
		settings->tx.samples_freq = 110000;
		settings->tx.freq_bps = 6000000;
		settings->tx.preload_buffer_len = 10000;
		break;
	case cfg_tx_pa_sin_quarter:
		controller_set_default_tx_pa_settings();
		settings->tx.stream_type = stream_freq_max_div2;
		break;
	case cfg_tx_pa_freq_sweep:
		controller_set_default_tx_pa_settings();
		settings->tx.stream_type = stream_freq_sweep;
		break;
	case cfg_tx_pa_freq_sweep_preload:
		controller_set_default_tx_pa_settings();
		settings->tx.stream_type = stream_freq_sweep_preload;
		settings->tx.freq_bps = 6000000;
		break;
	case cfg_tx_pa_sin_ook_20kHz:
		controller_set_default_tx_pa_settings();
		settings->tx.stream_type = stream_ook_pattern;
		settings->tx.samples_freq = 20000;
		break;
	case cfg_tx_pa_msg_pwm_110kHz_preload:
		controller_set_default_tx_pa_settings();
		settings->tx.samples_freq = 110000;
		settings->tx.freq_bps = 6000000;
		settings->tx.preload_buffer_len = 200000;
		settings->data_bit_us = 100;
		encoder_plugin = ENCODER_PWM;
		break;
	case cfg_tx_pa_constant:
		controller_set_default_tx_pa_settings();
		settings->tx.stream_type = stream_constant;
		break;
	case cfg_tx_pa_rx_sin_2kHz:
		controller_set_default_tx_pa_rx_settings();
		settings->tx.stream_type = stream_freq_sinus;
		settings->tx.samples_freq = 2000;
		break;
	case cfg_tx_pa_rx_sin_ook_9kHz:
		controller_set_default_tx_pa_rx_settings();
		settings->tx.stream_type = stream_ook_pattern;
		settings->tx.samples_freq = 9000;
		break;
	case cfg_rx:
		controller_set_default_rx_settings();
		break;
	case cfg_rx_msg_pwm_110kHz:
		controller_set_default_rx_settings();
		settings->rx.demod_mode = demod_mode_real_time;
		settings->rx.data_offset = 1667;
		settings->data_bit_us = 100;
		decoder_plugin = DECODER_PWM;
		break;
	default:
		assert(0);
	}
	select_encoder(encoder_plugin);
	select_decoder(decoder_plugin);
}

void controller_set_configuration_profile(enum configuration_profile_enum configuration_profile)
{
	controller_terminate();
	controller_set_configuration_profile_in_stop(configuration_profile);
	controller_initialize();
}

void controller_set_monitoring_profile(enum monitor_profile_enum profile)
{
	settings->monitor_profile = profile;
	monitor_set_profile(monitor, profile);
}

void controller_clear_overloads(void)
{
	plc_afe_clear_overloads(plc_afe);
}

void controller_log_overloads(void)
{
	uint8_t ret = plc_afe_get_overloads(plc_afe);
	log_format("AFEREG_RESET = 0x%x\n", ret);
}

void controller_view_current_settings(void)
{
	char *info_settings = settings_get_info(settings);
	char *info_afe = plc_afe_get_info(plc_afe);
	char *configuration_text;
	asprintf(&configuration_text, ">> SETTINGS\n%s>> AFE\n%s", info_settings, info_afe);
	free(info_afe);
	free(info_settings);
	ui_log_text(ui, configuration_text);
	free(configuration_text);
}

void controller_measure_encoder_time(void)
{
	if (plc_tx == NULL)
	{
		log_line("No TX mode configured");
		return;
	}
	log_line("Modulation simulation in progress...");
	static const int buffer_samples = 1000;
	uint16_t *buffer = (void*) malloc(buffer_samples * sizeof(uint16_t));
	struct timespec stamp_ini = plc_time_get_hires_stamp();
	plc_tx_fill_buffer_iteration(plc_tx, buffer, buffer_samples);
	uint32_t buffer_preparation_us = plc_time_hires_interval_to_usec(stamp_ini,
			plc_time_get_hires_stamp());
	log_format("Preparation time = %.2f us per sample -> Max. real-time freq = %.0f kHz\n",
			buffer_preparation_us / (float) buffer_samples,
			buffer_samples / (float) buffer_preparation_us * 1000.0);
	free(buffer);
}

struct plc_plugin_list *controller_get_encoder_plugins(void)
{
	return encoder_plugins;
}

struct plc_plugin_list *controller_get_decoder_plugins(void)
{
	return decoder_plugins;
}

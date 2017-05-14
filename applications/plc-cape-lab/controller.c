/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#define _GNU_SOURCE				// Required for 'asprintf' declaration
#include <signal.h>				// SIGEV_THREAD
#include <time.h>				// CLOCK_REALTIME
#include <unistd.h>
#include "+common/api/+base.h"
#include "+common/api/setting.h"
#include "+common/api/ui.h"		// event_id_tx_flag
#include "cmdline.h"
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
#include "profiles.h"
#include "rx.h"
#include "settings.h"
#include "singletons_provider.h"
#include "ui.h"

#define UI_PLUGIN_NAME_DEFAULT "ui-ncurses"
#define SIG_COMUNICATION_TIMER SIGRTMIN
#define SUPERVISOR_PERIOD_MS 500

// Global variables
int in_emulation_mode = 0;

// 'logger' is a global object publicly accessible
struct logger *logger = NULL;

// Local variables
static struct settings *settings = NULL;
static struct ui *ui = NULL;
static struct plc_cape *plc_cape = NULL;
static struct plc_afe *plc_afe = NULL;
static struct plc_leds *plc_leds = NULL;
static struct plc_adc *plc_adc = NULL;
static struct rx *rx = NULL;
static struct plc_tx *plc_tx = NULL;
static struct encoder *encoder = NULL;
static sample_tx_t *tx_preload_buffer = NULL;
static struct decoder *decoder = NULL;
static struct monitor *monitor = NULL;
static struct plc_plugin_list *encoder_plugins = NULL;
static struct plc_plugin_list *decoder_plugins = NULL;
static struct profiles *profiles = NULL;
static int controller_activated = 0;
static int controller_communication_in_progress = 0;
static timer_t supervisor_timer_id = 0;
static int use_communication_timer = 0;
static timer_t communication_timer_id = 0;
static int stop_communication_on_timer = 0;
static struct itimerspec its_next_start;
static struct itimerspec its_next_stop;

char *controller_get_settings_text(void)
{
	return settings_get_info(settings, encoder, decoder);
}

static void controller_flags_callback(void *data, int tx_flag, int rx_flag, int ok_flag)
{
	ui_set_event(ui, event_id_tx_flag, tx_flag);
	ui_set_event(ui, event_id_rx_flag, rx_flag);
	ui_set_event(ui, event_id_ok_flag, ok_flag);
}

void controller_encoder_set_default_configuration(void)
{
	encoder_set_default_configuration(encoder);
}

void controller_decoder_set_default_configuration(void)
{
	decoder_set_default_configuration(decoder);
}

void controller_encoder_apply_configuration(void)
{
	encoder_apply_configuration(encoder, settings->tx.sampling_rate_sps, settings->bit_width_us);
}

void controller_decoder_apply_configuration(void)
{
	// NOTE: By default reuse 'settings->rx.data_hi_threshold_detection' for 'data_hi_threshold'
	decoder_apply_configuration(decoder, settings->rx.data_offset,
			settings->rx.data_hi_threshold_detection, settings->rx.sampling_rate_sps,
			settings->bit_width_us, settings->rx.samples_to_file);
}

// PRECONDITION: encoder_plugins->active_index != (uint32_t)-1
void controller_reload_encoder_plugin(void)
{
	if (encoder)
		encoder_release(encoder);
	const char *plugin_name = encoder_plugins->list[encoder_plugins->active_index];
	char *plugin_path = plc_plugin_get_abs_path(plc_plugin_category_encoder, plugin_name);
	encoder = encoder_create(plugin_path);
	free(plugin_path);
}

// PRECONDITION: decoder_plugins->active_index != (uint32_t)-1
void controller_reload_decoder_plugin(void)
{
	if (decoder)
		decoder_release(decoder);
	const char *plugin_name = decoder_plugins->list[decoder_plugins->active_index];
	char *plugin_path = plc_plugin_get_abs_path(plc_plugin_category_decoder, plugin_name);
	decoder = decoder_create(plugin_path);
	free(plugin_path);
}

void controller_push_profile(const char *profile_identifier,
		struct setting_list_item **encoder_settings, struct setting_list_item **decoder_settings)
{
	settings_set_defaults(settings);
	char *encoder_name;
	char *decoder_name;
	profiles_push_profile_settings(profiles, profile_identifier, settings, &encoder_name,
			encoder_settings, &decoder_name, decoder_settings);
	if (encoder_name != NULL)
	{
		encoder_plugins->active_index = plc_plugin_list_find_name(encoder_plugins, encoder_name);
		if (encoder_plugins->active_index == -1)
			log_line_and_exit("Required encoder-plugin is not present");
		free(encoder_name);
	}
	else
	{
		encoder_plugins->active_index = -1;
	}
	if (decoder_name != NULL)
	{
		decoder_plugins->active_index = plc_plugin_list_find_name(decoder_plugins, decoder_name);
		if (decoder_plugins->active_index == -1)
			log_line_and_exit("Required decoder-plugin is not present");
		free(decoder_name);
	}
	else
	{
		decoder_plugins->active_index = -1;
	}
	if (settings->configuration_profile)
		free(settings->configuration_profile);
	settings->configuration_profile = strdup(profile_identifier);
}

static void controller_initialize(int reload_plugins,
		const struct setting_list_item *encoder_settings,
		const struct setting_list_item *decoder_settings)
{
	TRACE(3, "Starting controller");
	// Update dependent settings
	// Calibration mode
	switch (settings->operating_mode)
	{
	case operating_mode_calib_dac_txpga:
		settings->tx.afe_calibration_mode = afe_calibration_dac_txpga;
		break;
	case operating_mode_calib_dac_txpga_txfilter:
		settings->tx.afe_calibration_mode = afe_calibration_dac_txpga_txfilter;
		break;
	case operating_mode_calib_dac_txpga_rxpga1_rxfilter_rxpag2:
		settings->tx.afe_calibration_mode = afe_calibration_dac_txpga_rxpga1_rxfilter_rxpga2;
		break;
	default:
		settings->tx.afe_calibration_mode = afe_calibration_none;
		break;
	}
	// TX & RX devices
	if (!in_emulation_mode)
	{
		settings->tx.device = plc_tx_device_plc_cape;
		settings->rx.device =
				settings->std_driver ? plc_rx_device_adc_bbb_std_drivers : plc_rx_device_adc_bbb;
	}
	else if (settings->tx.afe_calibration_mode == afe_calibration_none)
	{
		settings->tx.device = plc_tx_device_alsa;
		settings->rx.device = plc_rx_device_alsa;
	}
	else
	{
		settings->tx.device = plc_tx_device_internal_fifo;
		settings->rx.device = plc_rx_device_internal_fifo;
	}
	if (plc_afe)
	{
		plc_afe_set_standy(plc_afe, 0);
		struct afe_settings afe_settings;
		afe_settings.cenelec_a = settings->cenelec_a;
		afe_settings.gain_tx_pga = settings->tx.gain_tx_pga;
		afe_settings.gain_rx_pga1 = settings->rx.gain_rx_pga1;
		afe_settings.gain_rx_pga2 = settings->rx.gain_rx_pga2;
		plc_afe_configure(plc_afe, &afe_settings);
		plc_afe_set_flags_callback(plc_afe, controller_flags_callback, NULL);
		plc_afe_set_calibration_mode(plc_afe, settings->tx.afe_calibration_mode);
		TRACE(3, "CAPE configured");
	}
	if (settings->tx.tx_mode != spi_tx_mode_none)
	{
		if (encoder_plugins->active_index == (uint32_t) -1)
			log_line_and_exit("Configuration error: TX mode but no valid encoder");
		if (reload_plugins)
		{
			controller_reload_encoder_plugin();
			encoder_set_configuration(encoder, encoder_settings);
		}
		plc_tx = plc_tx_create(settings->tx.device, encoder_prepare_next_samples, encoder,
				monitor_on_buffer_sent, monitor, settings->tx.tx_mode,
				settings->tx.sampling_rate_sps, settings->tx.tx_buffers_len, plc_afe);
		if (plc_tx == NULL)
			log_line_and_exit("Error at 'plc_tx' component initialization");
		settings->tx.sampling_rate_sps = plc_tx_get_effective_sampling_rate(plc_tx);
		monitor_set_tx(monitor, plc_tx);
		// Some encoder may depend on 'sampling_rate_sps' which also may depend on 'plc_tx'
		controller_encoder_apply_configuration();
		TRACE(3, "TX mode prepared");
	}
	if (settings->rx.rx_mode != rx_mode_none)
	{
		if (decoder_plugins->active_index == (uint32_t) -1)
			log_line_and_exit("Configuration error: RX mode but no valid decoder");
		if (reload_plugins)
		{
			controller_reload_decoder_plugin();
			decoder_set_configuration(decoder, decoder_settings);
		}
		controller_decoder_apply_configuration();
		plc_adc = plc_adc_create(settings->rx.device);
		monitor_set_adc(monitor, plc_adc);
		struct rx_settings rx_settings;
		rx_settings.rx_mode = settings->rx.rx_mode;
		rx_settings.capturing_rate_sps = settings->rx.sampling_rate_sps;
		rx_settings.samples_filename = settings->rx.samples_filename;
		rx_settings.data_filename = settings->rx.data_filename;
		rx_settings.samples_to_file = settings->rx.samples_to_file;
		rx_settings.demod_mode = settings->rx.demod_mode;
		rx_settings.bit_width_us = settings->bit_width_us;
		rx_settings.data_offset = settings->rx.data_offset;
		rx_settings.data_hi_threshold_detection = settings->rx.data_hi_threshold_detection;
		rx = rx_create(&rx_settings, monitor, plc_leds, plc_adc, decoder);
		TRACE(3, "TX mode prepared");
	}
	monitor_set_profile(monitor, settings->monitor_profile);
}

static void controller_terminate(int release_plugins)
{
	if (controller_activated)
		controller_deactivate();
	if (rx)
	{
		rx_release(rx);
		rx = NULL;
	}
	if (release_plugins && decoder)
	{
		decoder_release(decoder);
		decoder = NULL;
	}
	if (plc_tx)
	{
		plc_tx_release(plc_tx);
		plc_tx = NULL;
	}
	if (release_plugins && encoder)
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

void controller_create(int argc, char *argv[])
{
	TRACE(3, "Initializing settings");
	settings = settings_create();
	char *error_msg;

	TRACE(3, "Loading lists of plugins");
	encoder_plugins = plc_plugin_list_create(plc_plugin_category_encoder);
	decoder_plugins = plc_plugin_list_create(plc_plugin_category_decoder);

	TRACE(3, "Loading profiles");
	profiles = profiles_create();

	TRACE(3, "Parsing command line");
	struct setting_list_item *encoder_settings = NULL;
	struct setting_list_item *decoder_settings = NULL;
	struct setting_list_item *encoder_explicit_settings = NULL;
	struct setting_list_item *decoder_explicit_settings = NULL;
	char *initial_profile = NULL;
	char *ui_plugin_name = strdup(UI_PLUGIN_NAME_DEFAULT);
	cmdline_parse_args(argc, argv, &error_msg, settings, &ui_plugin_name, &initial_profile,
			&encoder_explicit_settings, &decoder_explicit_settings);
	if (error_msg != NULL)
	{
		log_line(error_msg);
		free(error_msg);
		log_line(cmdline_get_usage_message());
		exit(EXIT_FAILURE);
	}

	TRACE(3, "Configuring profile");
	if (initial_profile != NULL)
	{
		controller_push_profile(initial_profile, &encoder_settings, &decoder_settings);
		free(initial_profile);
		initial_profile = NULL;
	}

	TRACE(3, "Processing command line encoder-settings");
	struct setting_list_item *encoder_setting_list_item = encoder_explicit_settings;
	while (encoder_setting_list_item != NULL)
	{
		plc_setting_set_setting(&encoder_settings, &encoder_setting_list_item->setting);
		encoder_setting_list_item = encoder_setting_list_item->next;
	}

	TRACE(3, "Processing command line decoder-settings");
	struct setting_list_item *decoder_setting_list_item = decoder_explicit_settings;
	while (decoder_setting_list_item != NULL)
	{
		plc_setting_set_setting(&decoder_settings, &decoder_setting_list_item->setting);
		decoder_setting_list_item = decoder_setting_list_item->next;
	}

	TRACE(3, "Initializing UI");
	ui = ui_create(ui_plugin_name, argc, argv, settings);
	assert(ui);
	if (ui_plugin_name)
	{
		free(ui_plugin_name);
		ui_plugin_name = NULL;
	}

	TRACE(3, "Creating logger");
	logger = logger_create_log_text((void*) ui_log_text, ui);
	singletons_provider_initialize();

	// Initialize device
	TRACE(3, "Creating 'plc_cape'");
	plc_cape = plc_cape_create(!settings->std_driver);
	if (plc_cape == NULL)
		log_line_and_exit(get_last_error());
	in_emulation_mode = plc_cape_in_emulation(plc_cape);
	plc_afe = plc_cape_get_afe(plc_cape);
	plc_leds = plc_cape_get_leds(plc_cape);

	TRACE(3, "Creating 'monitor'");
	monitor = monitor_create(ui);

	TRACE(3, "Starting controller");
	controller_initialize(1, encoder_settings, decoder_settings);
	plc_setting_clear_settings(&decoder_explicit_settings);
	plc_setting_clear_settings(&encoder_explicit_settings);
	plc_setting_clear_settings(&decoder_settings);
	plc_setting_clear_settings(&encoder_settings);

	TRACE(3, "Controller started");
	if (settings->autostart)
	{
		controller_activate_async();
		// TODO: Revise autoclosing strategy
		// If autostart + max_duration -> close the application after the test
		// if (settings->communication_timeout_ms > 0)
		// 	return 0;
	}

	log_line("Ready. Select an option from the menu");
	// NOTE: 'pause()' is interrupted by signals -> while(1)
	if (settings->quietmode)
		while (1)
			pause();
	else
		ui_do_menu_loop(ui);
}

void controller_release(void)
{
	TRACE(3, "Terminating controller");
	controller_terminate(1);

	TRACE(3, "Releasing objects");
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
	if (profiles)
	{
		profiles_release(profiles);
		profiles = NULL;
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
	if (settings)
	{
		settings_release(settings);
		settings = NULL;
	}
	TRACE(3, "Releasing UI");
#ifdef DEBUG
	log_line("Press a key to close UI...");
	getchar();
#endif
	// As the logger depends on the 'ui' released it prior to 'ui_release'
	if (logger)
	{
		logger_release(logger);
		logger = NULL;
	}
	if (ui)
	{
		ui_release(ui);
		ui = NULL;
	}
	TRACE(3, "UI released");
}

static int controller_start_communication(void)
{
	assert(!controller_communication_in_progress);
	log_line("AFE active");
	if (plc_afe)
	{
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
			break;
		case operating_mode_calib_dac_txpga_txfilter:
			afe_blocks = afe_block_dac | afe_block_tx | afe_block_ref2;
			break;
		case operating_mode_calib_dac_txpga_rxpga1_rxfilter_rxpag2:
			afe_blocks = afe_block_dac | afe_block_tx | afe_block_ref2 | afe_block_rx;
			break;
		case operating_mode_rx:
			afe_blocks = afe_block_rx | afe_block_ref2;
			break;
		case operating_mode_COUNT:
		default:
			assert(0);
		}
		plc_afe_activate_blocks(plc_afe, afe_blocks);
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
		if (plc_tx_start_transmission(plc_tx) < 0)
			goto error_on_plc_tx_start_transmission;
		plc_leds_set_tx_activity(plc_leds, 1);
	}
	if (settings->rx.rx_mode != rx_mode_none)
	{
		if (rx_start_capture(rx) < 0)
			goto error_on_rx_start_capture;
	}
	monitor_start(monitor);
	controller_communication_in_progress = 1;
	return 0;

	// TODO: Refactor removing 'goto'
	error_on_rx_start_capture: // Error at stage 2
	if (settings->tx.tx_mode != spi_tx_mode_none)
	{
		plc_tx_stop_transmission(plc_tx);
		plc_leds_set_tx_activity(plc_leds, 0);
	}

	error_on_plc_tx_start_transmission: // Error at stage 1
	if (tx_preload_buffer)
	{
		encoder_set_copy_mode(encoder, NULL, 0, 0);
		free(tx_preload_buffer);
		tx_preload_buffer = NULL;
	}
	if (plc_afe)
	{
		plc_afe_set_dac_mode(plc_afe, 0);
		plc_afe_disable_all(plc_afe);
	}
	return -1;
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
		if (controller_start_communication() >= 0)
		{
			int ret = timer_settime(communication_timer_id, 0, &its_next_stop, NULL);
			assert(ret == 0);
		}
		else
		{
			log_line(get_last_error());
			controller_deactivate();
		}
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
	long long freq_nanosecs = (long long) 1000000 * SUPERVISOR_PERIOD_MS;
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

// Marked as 'async' because for periodic interval the controller will be activated some time later
void controller_activate_async(void)
{
	if ((encoder && !encoder_is_ready(encoder)) || (decoder && !decoder_is_ready(decoder)))
	{
		log_line("The 'encoder' plugin is not ready");
		return;
	}
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
		long long freq_nanosecs = (long long) 1000000 * settings->communication_timeout_ms;
		its_next_stop.it_value.tv_sec = freq_nanosecs / 1000000000;
		its_next_stop.it_value.tv_nsec = freq_nanosecs % 1000000000;
		its_next_stop.it_interval.tv_sec = 0;
		its_next_stop.it_interval.tv_nsec = 0;
		use_communication_timer = 1;
	}
	if (settings->communication_interval_ms > 0)
	{
		long long freq_nanosecs = (long long) 1000000
				* (settings->communication_interval_ms - settings->communication_timeout_ms);
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
		// To avoid timer overlapping when using two timers, only one timer is used switching between
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
			TRACE(1, "Init timer %d", its_next_stop.it_value.tv_sec);
			stop_communication_on_timer = 1;
			int ret = timer_settime(communication_timer_id, 0, &its_next_stop, NULL);
			assert(ret == 0);
		}
		if (controller_start_communication() < 0)
		{
			log_line(get_last_error());
			controller_deactivate();
		}
	}
}

int controlles_is_active(void)
{
	return controller_activated;
}

void controller_set_app_settings(void)
{
	controller_terminate(0);
	controller_initialize(0, NULL, NULL);
}

void controller_reload_configuration_profile(const char *profile)
{
	controller_terminate(1);
	struct setting_list_item *encoder_settings = NULL;
	struct setting_list_item *decoder_settings = NULL;
	controller_push_profile(profile, &encoder_settings, &decoder_settings);
	controller_initialize(1, encoder_settings, decoder_settings);
	plc_setting_clear_settings(&decoder_settings);
	plc_setting_clear_settings(&encoder_settings);
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
	char *info_settings = settings_get_info(settings, encoder, decoder);
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
	float us_per_sample = buffer_preparation_us / (float) buffer_samples;
	float freq_sps = 1000000.0 / us_per_sample;
	// TODO: Put conversion formula from sps to bps in a common public location
	//	See 'libplc-cape/spi.c#spi_bps_to_sps(...)'
	log_format("Preparation time = %.2f us per sample\n", us_per_sample);
	if (freq_sps * 165e-9 < 1.0)
	{
		float freq_bps = freq_sps * 10.5 / (1.0 - freq_sps * 165e-9);
		log_format("Max. fs = %.0f ksps (= %.1f Mbps)\n", freq_sps / 1000.0, freq_bps / 1000000.0);
	}
	else
	{
		log_format("Max. fs = %.0f ksps (> 12Mbps)\n", freq_sps / 1000.0);
	}
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

struct profiles *controller_get_profiles(void)
{
	return profiles;
}

struct setting_linked_list_item *controller_encoder_get_settings(void)
{
	return encoder ? encoder_get_settings(encoder) : NULL;
}

struct setting_linked_list_item *controller_decoder_get_settings(void)
{
	return decoder ? decoder_get_settings(decoder) : NULL;
}

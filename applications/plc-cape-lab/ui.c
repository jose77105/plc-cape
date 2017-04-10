/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include <unistd.h>			// access
#include "+common/api/+base.h"
#include "+common/api/ui.h"
#include "common.h"
#include "controller.h"
#include "encoder.h"		// stream_type_enum_text
#include "libraries/libplc-tools/api/plugin.h"
#include "monitor.h"
#include "plugins.h"
#include "plugins/ui/api/ui.h"
#include "plugins/encoder/api/encoder.h"		// stream_COUNT
#include "settings.h"
#include "ui.h"

#define UI_PLUGIN_NAME_DEFAULT1 "ui-ncurses"
#define UI_PLUGIN_NAME_DEFAULT2 "ui-console"

enum profile_category_enum
{
	profile_none = 0, profile_tx_rx_cal, profile_tx, profile_tx_pa, profile_tx_pa_rx, profile_rx
};

struct ui
{
	struct plugin *plugin;
	struct ui_api *api;
	ui_api_h api_handle;
	struct settings *settings;
};

static void ui_active_panel_close(struct ui *ui)
{
	ui->api->active_panel_close(ui->api_handle);
}

static void ui_update_settings_window(struct ui *ui)
{
	char *settings_text = controller_get_settings_text();
	ui->api->set_info(ui->api_handle, ui_info_settings_text, settings_text);
	free(settings_text);
}

static void ui_start_test(struct ui *ui)
{
	controller_activate_async();
}

static void ui_stop_test(struct ui *ui)
{
	controller_deactivate();
}

static void ui_active_settings_dialog_on_ok(struct ui *ui)
{
	controller_update_configuration();
	ui_update_settings_window(ui);
	ui_active_panel_close(ui);
}

static void ui_set_configuration_profile(struct ui *ui,
		enum configuration_profile_enum configuration_profile)
{
	controller_set_configuration_profile(configuration_profile);
	ui_update_settings_window(ui);
	// Auto-close popup on selection
	ui_active_panel_close(ui);
	ui_active_panel_close(ui);
}

static struct ui_menu_item menu_profiles_tx_rx_cal_item_array[] =
{
{ "1. TX+RX CAL 2kHz DEFAULTS", '1', ui_set_configuration_profile, cfg_txrx_cal_sin_2kHz_defaults },
{ "2. TX+RX CAL 2kHz", '2', ui_set_configuration_profile, cfg_txrx_cal_sin_2kHz },
{ "3. TX+RX CAL Ramp", '3', ui_set_configuration_profile, cfg_txrx_cal_ramp },
{ "4. TX+RX CAL WAVE OOK WAVE 10kHz", '4',
		ui_set_configuration_profile, cfg_txrx_cal_wave_ook_10kHz },
{ "5. TX+RX CAL WAVE OOK WAVE-HI 10kHz", '5',
		ui_set_configuration_profile, cfg_txrx_cal_wave_ook_Hi_10kHz },
{ "6. TX+RX CAL OOK MSG 10kHz Preload", '6',
		ui_set_configuration_profile, cfg_txrx_cal_msg_ook_10kHz_preload },
{ "7. TX+RX CAL PWM MSG 10kHz Preload", '7',
		ui_set_configuration_profile, cfg_txrx_cal_msg_pwm_10kHz_preload },
{ "8. TX+FILTER+RX CAL PWM MSG 110kHz Preload", '8',
		ui_set_configuration_profile, cfg_txrx_cal_filter_msg_pwm_110kHz_preload },
{ "9. TX+FILTER+RX CAL PWM MSG 110kHz Preload Timed", '9',
		ui_set_configuration_profile, cfg_txrx_cal_filter_msg_pwm_110kHz_preload_timed },
{ "Back [ESC]", 0, (void*) ui_active_panel_close }
};

static struct ui_menu_item menu_profiles_tx_item_array[] =
{
{ "1. TX 2kHz", '1', ui_set_configuration_profile, cfg_tx_sin_2kHz },
{ "2. TX 100kHz 3Mbps", '2', ui_set_configuration_profile, cfg_tx_sin_100kHz_3Mbps },
{ "3. TX File 93750 bps", '3', ui_set_configuration_profile, cfg_tx_file_93750bps },
{ "4. TX freq sweep", '4', ui_set_configuration_profile, cfg_tx_freq_sweep },
{ "Back [ESC]", 0, (void*) ui_active_panel_close }
};

static struct ui_menu_item menu_profiles_tx_pa_item_array[] =
{
{ "1. TX PA 10kHz", '1', ui_set_configuration_profile, cfg_tx_pa_sin_10kHz },
{ "2. TX PA 110kHz 6Mbps Preload", '2', ui_set_configuration_profile,
		cfg_tx_pa_sin_110kHz_6Mbps_preload },
{ "3. TX PA 1/4 Baud", '3', ui_set_configuration_profile, cfg_tx_pa_sin_quarter },
{ "4. TX PA Freq Sweep", '4', ui_set_configuration_profile, cfg_tx_pa_freq_sweep },
{ "5. TX PA Freq Sweep Preload", '5', ui_set_configuration_profile, cfg_tx_pa_freq_sweep_preload },
{ "6. TX PA OOK 20kHz", '6', ui_set_configuration_profile, cfg_tx_pa_sin_ook_20kHz },
{ "7. TX PA MSG PWM 110kHz Preload", '7',
		ui_set_configuration_profile, cfg_tx_pa_msg_pwm_110kHz_preload },
{ "8. TX PA Constant", '8', ui_set_configuration_profile, cfg_tx_pa_constant },
{ "Back [ESC]", '0', (void*) ui_active_panel_close }
};

static struct ui_menu_item menu_profiles_tx_pa_rx_item_array[] =
{
{ "1. TX+PA+RX 2kHz", '1', ui_set_configuration_profile, cfg_tx_pa_rx_sin_2kHz },
{ "2. TX+PA+RX OOK 9kHz", '2', ui_set_configuration_profile, cfg_tx_pa_rx_sin_ook_9kHz },
{ "Back [ESC]", 0, (void*) ui_active_panel_close }
};

static struct ui_menu_item menu_profiles_rx_item_array[] =
{
{ "1. RX", '1', ui_set_configuration_profile, cfg_rx },
{ "2. RX MSG PWM 110kHz", '2', ui_set_configuration_profile, cfg_rx_msg_pwm_110kHz },
{ "Back [ESC]", 0, (void*) ui_active_panel_close }
};

static void ui_open_menu_profile_category(struct ui *ui,
		enum profile_category_enum profile_category)
{
	const struct ui_menu_item *menu_items = NULL;
	uint32_t menu_items_count = 0;
	switch (profile_category)
	{
	case profile_tx_rx_cal:
		menu_items = menu_profiles_tx_rx_cal_item_array;
		menu_items_count = ARRAY_SIZE(menu_profiles_tx_rx_cal_item_array);
		break;
	case profile_tx:
		menu_items = menu_profiles_tx_item_array;
		menu_items_count = ARRAY_SIZE(menu_profiles_tx_item_array);
		break;
	case profile_tx_pa:
		menu_items = menu_profiles_tx_pa_item_array;
		menu_items_count = ARRAY_SIZE(menu_profiles_tx_pa_item_array);
		break;
	case profile_tx_pa_rx:
		menu_items = menu_profiles_tx_pa_rx_item_array;
		menu_items_count = ARRAY_SIZE(menu_profiles_tx_pa_rx_item_array);
		break;
	case profile_rx:
		menu_items = menu_profiles_rx_item_array;
		menu_items_count = ARRAY_SIZE(menu_profiles_rx_item_array);
		break;
	default:
		assert(0);
	}
	ui->api->open_menu(ui->api_handle, menu_items, menu_items_count, NULL, ui_active_panel_close);
}

static struct ui_menu_item menu_profile_categories_item_array[] =
{
	{ "1. TX+RX CAL", '1', ui_open_menu_profile_category, profile_tx_rx_cal },
	{ "2. TX", '2', ui_open_menu_profile_category, profile_tx },
	{ "3. TX+PA", '3', ui_open_menu_profile_category, profile_tx_pa },
	{ "4. TX+PA+RX", '4', ui_open_menu_profile_category, profile_tx_pa_rx },
	{ "5. RX", '5', ui_open_menu_profile_category, profile_rx },
	{ "Back [ESC]", 0, (void*) ui_active_panel_close }
};

int ui_check_controller_idle(void)
{
	if (controlles_is_active())
	{
		log_line("There is a test in progress. Stop it first");
		return 0;
	}
	else
	{
		return 1;
	}
}

static void ui_open_menu_profile_categories(struct ui *ui)
{
	if (!ui_check_controller_idle()) return;
	ui->api->open_menu(ui->api_handle, menu_profile_categories_item_array,
			ARRAY_SIZE(menu_profile_categories_item_array), NULL, ui_active_panel_close);
}

static void ui_open_dialog_settings_tx(struct ui *ui)
{
	if (!ui_check_controller_idle()) return;
	struct plc_plugin_list *encoder_plugins = controller_get_encoder_plugins();
	struct ui_dialog_item settings_dialog_item_array[] =
	{
		{ "Encoders", data_type_list,
		{ .list.index = &encoder_plugins->active_index, .list.items =
				(const char**) encoder_plugins->list, .list.items_count =
				encoder_plugins->list_count } },
		{ "Stream type:", data_type_list,
		{ .list.index = &ui->settings->tx.stream_type, .list.items = stream_type_enum_text,
				.list.items_count = stream_COUNT } },
		{ "Freq (Hz):", data_type_i32,
		{ .i32 = &ui->settings->tx.samples_freq }, 6 },
		{ "SPI Bauds:", data_type_u32,
		{ .u32 = &ui->settings->tx.freq_bps }, 8 },
		{ "Offset:", data_type_i32,
		{ .i32 = &ui->settings->tx.samples_offset }, 4 },
		{ "Range:", data_type_i32,
		{ .i32 = &ui->settings->tx.samples_range }, 4 },
		{ "AFE Gain TX PGA:", data_type_u32,
		{ .u32 = &ui->settings->tx.gain_tx_pga }, 1 },
		{ "Preload buffer len:", data_type_u32,
		{ .u32 = &ui->settings->tx.preload_buffer_len } },
		{ "TX mode:", data_type_list,
		{ .list.index = &ui->settings->tx.tx_mode, .list.items = spi_tx_mode_enum_text,
				.list.items_count = spi_tx_mode_COUNT } },
		{ "TX buffer len:", data_type_u32,
		{ .u32 = &ui->settings->tx.tx_buffers_len } },
		{ "Filename:", data_type_str,
		{ .str = &ui->settings->tx.dac_filename }, 20 },
		{ "Message to send:", data_type_str,
		{ .str = &ui->settings->tx.message_to_send }, 15 }
	};
	ui->api->open_dialog(ui->api_handle, settings_dialog_item_array,
			ARRAY_SIZE(settings_dialog_item_array), "Settings TX", ui_active_panel_close,
			ui_active_settings_dialog_on_ok);
}

static void ui_open_dialog_settings_rx(struct ui *ui)
{
	if (!ui_check_controller_idle()) return;
	struct plc_plugin_list *decoder_plugins = controller_get_decoder_plugins();
	struct ui_dialog_item settings_dialog_item_array[] =
	{
		{ "Decoders", data_type_list,
		{ .list.index = &decoder_plugins->active_index, .list.items =
				(const char**) decoder_plugins->list, .list.items_count =
				decoder_plugins->list_count } },
		{ "RX mode:", data_type_list,
		{ .list.index = &ui->settings->rx.rx_mode, .list.items = rx_mode_enum_text,
				.list.items_count = rx_mode_COUNT } },
		{ "Samples to file:", data_type_u32,
		{ .u32 = &ui->settings->rx.samples_to_file } },
		{ "Demod mode:", data_type_list,
		{ .list.index = &ui->settings->rx.demod_mode, .list.items = demodulation_mode_enum_text,
				.list.items_count = demod_mode_COUNT } },
		{ "Data offset:", data_type_u16,
		{ .u16 = &ui->settings->rx.data_offset } },
		{ "ADC Data HI threshold detection:", data_type_u16,
		{ .u16 = &ui->settings->rx.data_hi_threshold_detection } },
		{ "Demod Data HI threshold:", data_type_u16,
		{ .u16 = &ui->settings->rx.demod_data_hi_threshold } },
		{ "AFE Gain RX PGA1:", data_type_u32,
		{ .u32 = &ui->settings->rx.gain_rx_pga1 } },
		{ "AFE Gain RX PGA2:", data_type_u32,
		{ .u32 = &ui->settings->rx.gain_rx_pga2 } }
	};
	ui->api->open_dialog(ui->api_handle, settings_dialog_item_array,
			ARRAY_SIZE(settings_dialog_item_array), "Settings RX", ui_active_panel_close,
			ui_active_settings_dialog_on_ok);
}

static void ui_open_dialog_settings_global(struct ui *ui)
{
	if (!ui_check_controller_idle()) return;
	struct ui_dialog_item settings_dialog_item_array[] =
	{
		{ "AFE Mode:", data_type_list,
		{ .list.index = &ui->settings->operating_mode, .list.items = operating_mode_enum_text,
				.list.items_count = operating_mode_COUNT } },
		{ "AFE CENELEC A:", data_type_i32,
		{ .i32 = &ui->settings->cenelec_a } },
		{ "Data bit width (us):", data_type_u32,
		{ .u32 = &ui->settings->data_bit_us } },
		{ "Max communication duration (ms):", data_type_u32,
		{ .u32 = &ui->settings->communication_timeout_ms } },
		{ "Communication periodic interval (ms):", data_type_u32,
		{ .u32 = &ui->settings->communication_interval_ms } }
	};
	ui->api->open_dialog(ui->api_handle, settings_dialog_item_array,
			ARRAY_SIZE(settings_dialog_item_array), "Setting AFE", ui_active_panel_close,
			ui_active_settings_dialog_on_ok);
}

static void ui_set_monitoring_profile(struct ui *ui, enum monitor_profile_enum profile)
{
	controller_set_monitoring_profile(profile);
	ui_update_settings_window(ui);
	// Auto-close popup on selection
	ui_active_panel_close(ui);
}

static struct ui_menu_item menu_monitoring_profiles_item_array[] =
{
{ "1. None (for minimum impact)", '1', ui_set_monitoring_profile, monitor_profile_none },
{ "2. Buffers processed", '2', ui_set_monitoring_profile, monitor_profile_buffers_processed },
{ "3. TX values", '3', ui_set_monitoring_profile, monitor_profile_tx_values },
{ "4. TX time", '4', ui_set_monitoring_profile, monitor_profile_tx_time },
{ "5. RX values", '5', ui_set_monitoring_profile, monitor_profile_rx_values },
{ "6. RX time", '6', ui_set_monitoring_profile, monitor_profile_rx_time },
{ "Back [ESC]", 0, (void*) ui_active_panel_close }
};

static void ui_open_menu_monitoring_profiles(struct ui *ui)
{
	ui->api->open_menu(ui->api_handle, menu_monitoring_profiles_item_array,
			ARRAY_SIZE(menu_monitoring_profiles_item_array), NULL, ui_active_panel_close);
}

static struct ui_menu_item menu_tools_item_array[] =
{
{ "(L)og overoads", 'l', (void*) controller_log_overloads },
{ "Clear ove(r)loads", 'r', (void*) controller_clear_overloads },
{ "(V)iew current settings", 'v', (void*) controller_view_current_settings },
{ "(M)easure processing time on modulation", 'm', (void*) controller_measure_encoder_time },
{ "Back [ESC]", 0, (void*) ui_active_panel_close }
};

static void ui_open_menu_tools(struct ui *ui)
{
	if (!ui_check_controller_idle()) return;
	ui->api->open_menu(ui->api_handle, menu_tools_item_array, ARRAY_SIZE(menu_tools_item_array),
	NULL, ui_active_panel_close);
}

static struct ui_menu_item main_menu_item_array[] =
{
{ "ON [N]", 'n', (void*) ui_start_test },
{ "OFF [F]", 'f', (void*) ui_stop_test },
{ "Profiles [P]", 'p', (void*) ui_open_menu_profile_categories },
{ "Settings Global [G]", 'g', (void*) ui_open_dialog_settings_global },
{ "Settings TX [T]", 't', (void*) ui_open_dialog_settings_tx },
{ "Settings RX [R]", 'r', (void*) ui_open_dialog_settings_rx },
{ "Monitoring profiles [M]", 'm', (void*) ui_open_menu_monitoring_profiles },
{ "Tools [O]", 'o', (void*) ui_open_menu_tools },
{ "Quit [Q]", 'q', (void*) ui_quit }
};

static int ui_check_plugin_exists(const char *plugin_name)
{
	char *plugin_abs_path = plc_plugin_get_abs_path(plc_plugin_category_ui, plugin_name);
	int ret = access(plugin_abs_path, F_OK);
	free(plugin_abs_path);
	return ret;
}

struct ui *ui_create(const char *ui_plugin_name, int argc, char *argv[], struct settings *settings)
{
	const char *ui_plugin_name_ok = NULL;
	ui_plugin_name_ok = ui_plugin_name;
	if (ui_check_plugin_exists(ui_plugin_name_ok) != 0)
	{	
		ui_plugin_name_ok = UI_PLUGIN_NAME_DEFAULT1;
		if (ui_check_plugin_exists(ui_plugin_name_ok) != 0)
		{
			ui_plugin_name_ok = UI_PLUGIN_NAME_DEFAULT2;
			if (ui_check_plugin_exists(ui_plugin_name_ok) != 0)
				log_errno_and_exit("No valid ui-plugin has been found to start the application");
		}
	}
	struct ui *ui = calloc(1, sizeof(*ui));
	ui->settings = settings;
	uint32_t api_version, api_size;
	char *plugin_abs_path = plc_plugin_get_abs_path(plc_plugin_category_ui, ui_plugin_name_ok);
	ui->plugin = load_plugin(plugin_abs_path, (void**) &ui->api, &api_version, &api_size);
	free(plugin_abs_path);
	assert((api_version >= 1) && (api_size >= sizeof(struct ui_api)));
	// Initialize
	if (ui->api->init(argc, argv))
	{
		ui->api_handle = ui->api->create("Main menu", main_menu_item_array,
				ARRAY_SIZE(main_menu_item_array), ui);
		return ui;
	}
	else
	{
		unload_plugin(ui->plugin);
		free(ui);
		return NULL;
	}
}

void ui_release(struct ui *ui)
{
	ui->api->release(ui->api_handle);
	unload_plugin(ui->plugin);
	free(ui);
}

inline void ui_set_event(struct ui *ui, uint32_t id, uint32_t data)
{
	ui->api->set_event(ui->api_handle, id, data);
}

inline void ui_log_text(struct ui *ui, const char *text)
{
	ui->api->log_text(ui->api_handle, text);
}

inline void ui_set_status_bar(struct ui *ui, const char *text)
{
	ui->api->set_info(ui->api_handle, ui_info_status_bar_text, text);
}

void ui_do_menu_loop(struct ui *ui)
{
	ui_update_settings_window(ui);
	ui->api->do_menu_loop(ui->api_handle);
}

inline void ui_quit(struct ui *ui)
{
	ui->api->quit(ui->api_handle);
}

inline void ui_refresh(struct ui *ui)
{
	ui->api->refresh(ui->api_handle);
}
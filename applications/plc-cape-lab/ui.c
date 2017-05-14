/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#define _GNU_SOURCE			// asprintf
#include <unistd.h>			// access
#include "+common/api/+base.h"
#include "+common/api/setting.h"
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
#include "profiles.h"
#include "ui.h"

/* Example of basic usage of different data types on dialogs
 *
 * struct ui_dialog_item settings_dialog_item_array[] = {
 *	{
 *		"Encoders", data_type_list, {
 *			.list.index = &encoder_plugins->active_index, .list.items =
 *					(const char**) encoder_plugins->list, .list.items_count =
 *					encoder_plugins->list_count } }, {
 *		"SPI Bauds:", data_type_u32, {
 *			.u32 = &ui->settings->tx.freq_bps }, 8 }, {
 *		"TX mode:", data_type_list, {
 *			.list.index = &ui->settings->tx.tx_mode, .list.items = spi_tx_mode_enum_text,
 *			.list.items_count = spi_tx_mode_COUNT } }, {
 *		"Message to send:", data_type_str, {
 *			.str = &ui->settings->tx.message_to_send }, 15 } };
 */

#define UI_PLUGIN_NAME_DEFAULT1 "ui-ncurses"
#define UI_PLUGIN_NAME_DEFAULT2 "ui-console"

struct ui
{
	struct plugin *plugin;
	struct ui_api *api;
	ui_api_h api_handle;
	struct settings *settings;
	struct ui_menu_item *menu_items_all_profiles;
	uint32_t menu_items_all_profiles_count;
	uint32_t menu_panel_depth;
};

inline static void ui_open_menu(struct ui *ui, const struct ui_menu_item *menu_items,
		uint32_t menu_items_count, const char *title, void (*on_cancel)(ui_callbacks_h))
{
	ui->menu_panel_depth++;
	ui->api->open_menu(ui->api_handle, menu_items, menu_items_count, title, on_cancel);
}

inline static void ui_open_dialog(struct ui *ui, const struct ui_dialog_item *dialog_items,
		uint32_t dialog_items_count, const char *title, void (*on_cancel)(ui_callbacks_h),
		void (*on_ok)(ui_callbacks_h))
{
	ui->menu_panel_depth++;
	ui->api->open_dialog(ui->api_handle, dialog_items, dialog_items_count, title, on_cancel, on_ok);
}

static void ui_active_panel_close(struct ui *ui)
{
	ui->menu_panel_depth--;
	ui->api->active_panel_close(ui->api_handle);
}

static void ui_close_all_panels(struct ui *ui)
{
	for (; ui->menu_panel_depth > 0; ui->menu_panel_depth--)
		ui->api->active_panel_close(ui->api_handle);
}

static void setting_to_dialog(struct ui_dialog_item *item,
		const struct plc_setting_definition *setting_definition,
		union plc_setting_data *setting_data)
{
	item->caption = strdup(setting_definition->caption);
	item->max_len = 20;
	switch (setting_definition->type)
	{
	case plc_setting_u16:
		item->data_type = data_type_u16;
		item->data_ptr.u16 = &setting_data->u16;
		break;
	case plc_setting_u32:
		item->data_type = data_type_u32;
		item->data_ptr.u32 = &setting_data->u32;
		break;
	case plc_setting_i32:
		item->data_type = data_type_i32;
		item->data_ptr.i32 = &setting_data->i32;
		break;
	case plc_setting_float:
		item->data_type = data_type_float;
		item->data_ptr.f = &setting_data->f;
		break;
	case plc_setting_string:
		item->data_type = data_type_str;
		item->data_ptr.str = &setting_data->s;
		break;
	case plc_setting_enum:
	{
		item->data_type = data_type_list;
		item->data_ptr.list.index = &setting_data->u32;
		plc_setting_get_enum_captions(setting_definition, &item->data_ptr.list.items,
				&item->data_ptr.list.items_count);
		break;
	}
	case plc_setting_callback:
		item->data_type = data_type_callbacks;
		item->data_ptr.callbacks.data = &setting_data->p;
		break;
	default:
		assert(0);
		item->data_type = data_type_u32;
	}
}

static void ui_refresh_settings_window(struct ui *ui)
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

static void ui_app_settings_dialog_on_ok(struct ui *ui)
{
	controller_set_app_settings();
	ui_refresh_settings_window(ui);
	ui_active_panel_close(ui);
}

static void ui_reload_configuration_profile(struct ui *ui, const char *profile)
{
	controller_reload_configuration_profile(profile);
	ui_refresh_settings_window(ui);
	ui_close_all_panels(ui);
}

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

static void ui_open_menu_all_profiles(struct ui *ui)
{
	ui_open_menu(ui, ui->menu_items_all_profiles, ui->menu_items_all_profiles_count, NULL,
			ui_active_panel_close);
}

static void ui_open_menu_profile_categories(struct ui *ui, const struct tree_node_vector *tree)
{
	if (!ui_check_controller_idle())
		return;
	struct profiles *profiles = controller_get_profiles();
	int all_profiles = 0;
	if (tree == NULL)
	{
		tree = profiles_get_tree(profiles);
		all_profiles = 1;
	}
	uint32_t n;
	uint32_t menu_items_count = tree->nodes_count + tree->profile_identifiers.count + all_profiles;
	struct ui_menu_item *menu_items = malloc(menu_items_count * sizeof(struct ui_menu_item));
	struct ui_menu_item *menu_item = menu_items;
	uint32_t menu_index = 0;
	const struct tree_node_vector *node = tree->nodes;
	for (n = tree->nodes_count; n > 0; n--, menu_item++, node++)
	{
		menu_item->shortcut = '1' + menu_index;
		asprintf(&menu_item->caption, "%c. %s", menu_item->shortcut, node->title);
		menu_item->action = (void*) ui_open_menu_profile_categories;
		menu_item->data = (uint32_t) node;
		if (menu_index++ == '9')
			menu_index = 'A';
	}
	char **profile_identifier = tree->profile_identifiers.identifiers;
	for (n = tree->profile_identifiers.count; n > 0; n--, menu_item++, profile_identifier++)
	{
		menu_item->shortcut = '1' + menu_index;
		asprintf(&menu_item->caption, "%c. %s", menu_item->shortcut, *profile_identifier);
		menu_item->action = (void *) ui_reload_configuration_profile;
		menu_item->data = (uint32_t) *profile_identifier;
		if (menu_index++ == '9')
			menu_index = 'A';
	}
	// Add a [All] menu with all the available profiles in flat format
	if (all_profiles)
	{
		menu_item->shortcut = '1' + menu_index;
		asprintf(&menu_item->caption, "%c. [ALL]", menu_item->shortcut);
		menu_item->action = (void*) ui_open_menu_all_profiles;
	}
	ui_open_menu(ui, menu_items, menu_items_count, NULL, ui_active_panel_close);
	// TODO: Implement copy items on 'open_menu' and then restore the below code for releasing
	//	For the moment and for simplicity, accept the associated memory leak
	/*
	 menu_item = menu_items;
	 for (n = menu_items_count; n > 0; n--, menu_item++)
	 free(menu_item->caption);
	 free(menu_items);
	 */
}

static void ui_on_encoder_selected(struct ui *ui)
{
	controller_reload_encoder_plugin();
	controller_encoder_set_default_configuration();
	controller_encoder_apply_configuration();
	ui_refresh_settings_window(ui);
	ui_active_panel_close(ui);
}

static void ui_on_decoder_selected(struct ui *ui)
{
	controller_reload_decoder_plugin();
	controller_decoder_set_default_configuration();
	controller_decoder_apply_configuration();
	ui_refresh_settings_window(ui);
	ui_active_panel_close(ui);
}

static void ui_on_encoder_reconfigured(struct ui *ui)
{
	controller_encoder_apply_configuration();
	ui_refresh_settings_window(ui);
	ui_active_panel_close(ui);
}

static void ui_on_decoder_reconfigured(struct ui *ui)
{
	controller_decoder_apply_configuration();
	ui_refresh_settings_window(ui);
	ui_active_panel_close(ui);
}

static void ui_open_dialog_encoder(struct ui *ui)
{
	if (!ui_check_controller_idle())
		return;
	struct plc_plugin_list *encoder_plugins = controller_get_encoder_plugins();
	struct ui_dialog_item settings_dialog_item_array[] = {
		{
			"Encoders", data_type_list, {
				.list.index = &encoder_plugins->active_index, .list.items =
						(const char**) encoder_plugins->list, .list.items_count =
						encoder_plugins->list_count } } };
	ui_open_dialog(ui, settings_dialog_item_array, ARRAY_SIZE(settings_dialog_item_array),
			"Encoder", ui_active_panel_close, ui_on_encoder_selected);
}

static void ui_open_dialog_decoder(struct ui *ui)
{
	if (!ui_check_controller_idle())
		return;
	if (!ui_check_controller_idle())
		return;
	struct plc_plugin_list *decoder_plugins = controller_get_decoder_plugins();
	struct ui_dialog_item settings_dialog_item_array[] = {
		{
			"Decoders", data_type_list, {
				.list.index = &decoder_plugins->active_index, .list.items =
						(const char**) decoder_plugins->list, .list.items_count =
						decoder_plugins->list_count } } };
	ui_open_dialog(ui, settings_dialog_item_array, ARRAY_SIZE(settings_dialog_item_array),
			"Decoder", ui_active_panel_close, ui_on_decoder_selected);
}

static void ui_open_dialog_configure_plugin(struct ui *ui, const char *title,
		struct setting_linked_list_item *settings, void (*ui_on_plugin_reconfigured)(struct ui *))
{
	if (!ui_check_controller_idle())
		return;
	uint32_t settings_count = plc_setting_get_settings_linked_count(settings);
	if (settings_count == 0)
	{
		log_line("No encoder-settings to configure");
		return;
	}
	struct ui_dialog_item *dialog_items = malloc(settings_count * sizeof(struct ui_dialog_item));
	struct setting_linked_list_item *item = settings;
	uint32_t i;
	for (i = 0; i < settings_count; i++, item = item->next)
		setting_to_dialog(&dialog_items[i], item->setting.definition, &item->setting.data);
	ui_open_dialog(ui, dialog_items, settings_count, title, ui_active_panel_close,
			ui_on_plugin_reconfigured);
	free(dialog_items);
};

static void ui_open_dialog_configure_encoder(struct ui *ui)
{
	ui_open_dialog_configure_plugin(ui, "Encoder settings", controller_encoder_get_settings(),
			ui_on_encoder_reconfigured);
}

static void ui_open_dialog_configure_decoder(struct ui *ui)
{
	ui_open_dialog_configure_plugin(ui, "Decoder settings", controller_decoder_get_settings(),
			ui_on_decoder_reconfigured);
}

static struct ui_menu_item menu_plugins_item_array[] = {
	{
		"1. Select Encoder", '1', (void*) ui_open_dialog_encoder }, {
		"2. Select Decoder", '2', (void*) ui_open_dialog_decoder }, {
		"(B)ack", 'b', (void*) ui_active_panel_close } };

static void ui_open_menu_plugins(struct ui *ui)
{
	if (!ui_check_controller_idle())
		return;
	ui_open_menu(ui, menu_plugins_item_array, ARRAY_SIZE(menu_plugins_item_array),
	NULL, ui_active_panel_close);
}

static void ui_open_dialog_settings_tx(struct ui *ui)
{
	if (!ui_check_controller_idle())
		return;
	struct ui_dialog_item settings_dialog_item_array[] = {
		{
			"Freq sampling [sps]:", data_type_float, {
				.f = &ui->settings->tx.sampling_rate_sps }, 8 }, {
			"AFE Gain TX PGA:", data_type_list, {
				.list.index = &ui->settings->tx.gain_tx_pga, .list.items = afe_gain_tx_pga_enum_text,
				.list.items_count = afe_gain_tx_pga_COUNT } }, {
			"Preload buffer len:", data_type_u32, {
				.u32 = &ui->settings->tx.preload_buffer_len } }, {
			"TX mode:", data_type_list, {
				.list.index = &ui->settings->tx.tx_mode, .list.items = spi_tx_mode_enum_text,
				.list.items_count = spi_tx_mode_COUNT } }, {
			"TX buffer len:", data_type_u32, {
				.u32 = &ui->settings->tx.tx_buffers_len } } };
	ui_open_dialog(ui, settings_dialog_item_array, ARRAY_SIZE(settings_dialog_item_array),
			"Settings TX", ui_active_panel_close, ui_app_settings_dialog_on_ok);
}

static void ui_open_dialog_settings_rx(struct ui *ui)
{
	if (!ui_check_controller_idle())
		return;
	struct ui_dialog_item settings_dialog_item_array[] = {
		{
			"RX mode:", data_type_list, {
				.list.index = &ui->settings->rx.rx_mode, .list.items = rx_mode_enum_text,
				.list.items_count = rx_mode_COUNT } }, {
			"Freq capture [sps]:", data_type_float, {
				.f = &ui->settings->rx.sampling_rate_sps } }, {
			"Samples to file:", data_type_u32, {
				.u32 = &ui->settings->rx.samples_to_file } }, {
			"Demod mode:", data_type_list, {
				.list.index = &ui->settings->rx.demod_mode, .list.items = demod_mode_enum_text,
				.list.items_count = demod_mode_COUNT } }, {
			"Data offset:", data_type_u16, {
				.u16 = &ui->settings->rx.data_offset } }, {
			"ADC Data HI threshold detection:", data_type_u16, {
				.u16 = &ui->settings->rx.data_hi_threshold_detection } }, {
			"AFE Gain RX PGA1:", data_type_list, {
				.list.index = &ui->settings->rx.gain_rx_pga1, .list.items = afe_gain_rx_pga1_enum_text,
				.list.items_count = afe_gain_rx_pga1_COUNT } }, {
			"AFE Gain RX PGA2:", data_type_list, {
				.list.index = &ui->settings->rx.gain_rx_pga2, .list.items = afe_gain_rx_pga2_enum_text,
				.list.items_count = afe_gain_rx_pga2_COUNT } } };
	ui_open_dialog(ui, settings_dialog_item_array, ARRAY_SIZE(settings_dialog_item_array),
			"Settings RX", ui_active_panel_close, ui_app_settings_dialog_on_ok);
}

static void ui_open_dialog_settings_global(struct ui *ui)
{
	if (!ui_check_controller_idle())
		return;
	struct ui_dialog_item settings_dialog_item_array[] = {
		{
			"AFE Mode:", data_type_list, {
				.list.index = &ui->settings->operating_mode, .list.items = operating_mode_enum_text,
				.list.items_count = operating_mode_COUNT } }, {
			"AFE CENELEC A:", data_type_i32, {
				.u32 = &ui->settings->cenelec_a } }, {
			"Data bit width (us):", data_type_u32, {
				.u32 = &ui->settings->bit_width_us } }, {
			"Max communication duration (ms):", data_type_u32, {
				.u32 = &ui->settings->communication_timeout_ms } }, {
			"Communication periodic interval (ms):", data_type_u32, {
				.u32 = &ui->settings->communication_interval_ms } } };
	ui_open_dialog(ui, settings_dialog_item_array, ARRAY_SIZE(settings_dialog_item_array),
			"Setting AFE", ui_active_panel_close, ui_app_settings_dialog_on_ok);
}

static void ui_set_monitoring_profile(struct ui *ui, enum monitor_profile_enum profile)
{
	controller_set_monitoring_profile(profile);
	ui_refresh_settings_window(ui);
	// Auto-close popup on selection
	ui_active_panel_close(ui);
}

static struct ui_menu_item menu_monitoring_profiles_item_array[] = {
	{
		"1. None (for minimum impact)", '1', ui_set_monitoring_profile, monitor_profile_none }, {
		"2. Buffers processed", '2', ui_set_monitoring_profile, monitor_profile_buffers_processed },
	{
		"3. TX values", '3', ui_set_monitoring_profile, monitor_profile_tx_values }, {
		"4. TX time", '4', ui_set_monitoring_profile, monitor_profile_tx_time }, {
		"5. RX values", '5', ui_set_monitoring_profile, monitor_profile_rx_values }, {
		"6. RX time", '6', ui_set_monitoring_profile, monitor_profile_rx_time }, {
		"(B)ack", 'b', (void*) ui_active_panel_close } };

static void ui_open_menu_monitoring_profiles(struct ui *ui)
{
	ui_open_menu(ui, menu_monitoring_profiles_item_array,
			ARRAY_SIZE(menu_monitoring_profiles_item_array), NULL, ui_active_panel_close);
}

static struct ui_menu_item menu_tools_item_array[] = {
	{
		"(L)og overoads", 'l', (void*) controller_log_overloads }, {
		"Clear ove(r)loads", 'r', (void*) controller_clear_overloads }, {
		"(V)iew current settings", 'v', (void*) controller_view_current_settings }, {
		"(M)easure processing time on modulation", 'm', (void*) controller_measure_encoder_time }, {
		"(B)ack", 'b', (void*) ui_active_panel_close } };

static void ui_open_menu_tools(struct ui *ui)
{
	if (!ui_check_controller_idle())
		return;
	ui_open_menu(ui, menu_tools_item_array, ARRAY_SIZE(menu_tools_item_array),
	NULL, ui_active_panel_close);
}

static struct ui_menu_item main_menu_item_array[] = {
	{
		"ON [N]", 'n', (void*) ui_start_test }, {
		"OFF [F]", 'f', (void*) ui_stop_test }, {
		"Profiles [P]", 'p', (void*) ui_open_menu_profile_categories, (uint32_t) NULL }, {
		"Plugins [L]", 'l', (void*) ui_open_menu_plugins }, {
		"Settings Global [G]", 'g', (void*) ui_open_dialog_settings_global }, {
		"Settings TX [T]", 't', (void*) ui_open_dialog_settings_tx }, {
		"Settings RX [R]", 'r', (void*) ui_open_dialog_settings_rx }, {
		"Settings Encoder [E]", 'e', (void*) ui_open_dialog_configure_encoder }, {
		"Settings Decoder [D]", 'd', (void*) ui_open_dialog_configure_decoder }, {
		"Monitoring profiles [M]", 'm', (void*) ui_open_menu_monitoring_profiles }, {
		"Tools [O]", 'o', (void*) ui_open_menu_tools }, {
		"Quit [Q]", 'q', (void*) ui_quit } };

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
	if (ui->menu_items_all_profiles)
	{
		uint32_t i;
		for (i = 0; i < ui->menu_items_all_profiles_count; i++)
			free(ui->menu_items_all_profiles[i].caption);
		free(ui->menu_items_all_profiles);
	}
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

inline void ui_quit(struct ui *ui)
{
	ui->api->quit(ui->api_handle);
}

inline void ui_refresh(struct ui *ui)
{
	ui->api->refresh(ui->api_handle);
}

void ui_do_menu_loop(struct ui *ui)
{
	// Prepare 'all-profiles' menu
	struct profiles *profiles = controller_get_profiles();
	uint32_t profiles_count = profiles_get_count(profiles);
	ui->menu_items_all_profiles = calloc(profiles_count, sizeof(*ui->menu_items_all_profiles));
	uint32_t i;
	profiles_iterator *iterator = profiles_move_to_first_profile(profiles);
	struct ui_menu_item *menu_item = ui->menu_items_all_profiles;
	uint32_t menu_char = '1';
	for (i = profiles_count; i > 0; i--)
	{
		if (!profiles_current_profile_is_hidden(iterator))
		{
			asprintf(&menu_item->caption, "%c. %s", menu_char,
					profiles_current_profile_get_title(iterator));
			// TODO: Do copy (& release) instead of using references
			menu_item->data = (uint32_t) profiles_current_profile_get_identifier(iterator);
			menu_item->shortcut = menu_char;
			menu_item->action = (void *) ui_reload_configuration_profile;
			menu_item++;
			if (menu_char++ == '9')
				menu_char = 'A';
		}
		iterator = profiles_move_to_next_profile(iterator);
	}
	ui->menu_items_all_profiles_count = menu_item - ui->menu_items_all_profiles;
	if (ui->menu_items_all_profiles_count < profiles_count)
		ui->menu_items_all_profiles = realloc(ui->menu_items_all_profiles,
				ui->menu_items_all_profiles_count * sizeof(*ui->menu_items_all_profiles));

	ui_refresh_settings_window(ui);
	ui->api->do_menu_loop(ui->api_handle);
}

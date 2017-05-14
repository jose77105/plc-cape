/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include <cairo.h>
#include <dirent.h>		// DIR
#include <errno.h>		// errno
#include <fcntl.h>		// O_RDONLY
#include <glib.h>		// TRUE, FALSE
#include <math.h>		// cos
#include <pthread.h>	// pthread
#include <stdarg.h>		// va_list
#include "+common/api/+base.h"
#include "application.h"
#include "libraries/libplc-tools/api/application.h"
#include "libraries/libplc-tools/api/time.h"
#include "plot_area.h"
#include "plot_widget.h"
#include "recorder_interface.h"
#include "recorder_plc.h"
#include "tools.h"

#define CONFIGURATION_NAME_MAX_LEN 63
#define DEFAULT_YUNIT_PER_SAMPLE 1.0
#define DEFAULT_OSC_BUFFER_INTERVAL_MS 20.0
#define SAMPLES_FILENAME "oscilloscope.csv"
#define PNG_FILENAME "oscilloscope.png"
#define SVG_FILENAME "oscilloscope.svg"
#define GRID_COLUMNS 2

#define OFFSET(member) offsetof(struct application_configuration, member)

// Avoid the warning produced because using string literals on 'char *' fields
// This is not a recommended practice but required here for simplicity in declarations
// More info:
//	http://stackoverflow.com/questions/59670/
//		how-to-get-rid-of-deprecated-conversion-from-string-constant-to-char-warnin
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"

// NOTE: To initialize unions in C++ it seems that the dialect '-std=c++11' is required which is
//	implemented since GCC 4.8.1.
// In the BBB if we execute 'gcc -v' we get 'gcc version 4.6.3 (Debian 4.6.3-14)'.
// This way, if we use 'designated initializers' in a C++ project it compiles in Ubuntu 14.04 but
// fails in the BBB. Example code used:
//	const struct plc_setting_definition Application_accepted_settings[] =
//		{{ "info_panel_visible", plc_setting_u32, "Information panel visible",
//		{	.u32 = 0}, 0, NULL, OFFSET(info_panel_visible)}};
// Additional info in: https://gcc.gnu.org/projects/cxx-status.html#cxx11
//
// The std C++ allows to initialize a union through the first member
// As we are not using the 'union-field' in this project, just set it to NULL

const struct plc_setting_definition Application::accepted_settings[] = {
	{
		"info_panel_visible", plc_setting_u32, "Information panel visible", {
			NULL }, 0, NULL, OFFSET(info_panel_visible) }, {
		"buffer_interval_ms", plc_setting_u32, "Buffer interval [ms]", {
			NULL }, 0, NULL, OFFSET(buffer_interval_ms) }, {
		"yunit_symbol", plc_setting_string, "Y-unit symbol", {
			NULL }, 0, NULL, OFFSET(yunit_symbol) }, {
		"yunit_per_sample", plc_setting_float, "Y-unit-per-sample conversion factor", {
			NULL }, 0, NULL, OFFSET(yunit_per_sample) }, {
		"load_afe_plugin", plc_setting_u32, "Load AFE plugin", {
			NULL }, 0, NULL, OFFSET(load_afe_plugin) }, {
		"default_width", plc_setting_u32, "Main window default width", {
			NULL }, 0, NULL, OFFSET(default_width) }, {
		"default_height", plc_setting_u32, "Main window default height", {
			NULL }, 0, NULL, OFFSET(default_height) }, };

#pragma GCC diagnostic pop

#undef OFFSET

const char *Application::statistics_mode_text[] = {
	"None", "Adc", "Osc" };

GActionEntry Application::app_entries[] = {
	{
		"start", on_menu_start, NULL, NULL, NULL }, {
		"stop", on_menu_stop, NULL, NULL, NULL }, {
		"save-samples", on_menu_save_samples, NULL, NULL, NULL }, {
		"save-png", on_menu_save_png, NULL, NULL, NULL }, {
		"save-svg", on_menu_save_svg, NULL, NULL, NULL }, {
		"load-configuration", on_menu_load_configuration, "s", NULL, NULL }, {
		"reload-configuration", on_menu_reload_configuration, NULL, NULL, NULL }, {
		"toggle-grid", on_menu_toggle_grid, NULL, "false", on_menu_toggle_grid_change_state }, {
		"toggle-samples", on_menu_toggle_samples, NULL, "true", NULL }, {
		"toggle-fft", on_menu_toggle_fft, NULL, "false", NULL }, {
		"preferences", on_menu_preferences, NULL, NULL, NULL }, {
		"plot_widget_settings", on_menu_plot_widget_settings, NULL, NULL, NULL }, {
		"about", on_menu_about, NULL, NULL, NULL }, {
		"quit", on_menu_quit, NULL, NULL, NULL }, {
		"auto-scale-y", on_menu_auto_scale_y, NULL, NULL, NULL }, {
		"auto-scale-x", on_menu_auto_scale_x, NULL, NULL, NULL }, {
		"auto-offset-y", on_menu_auto_offset_y, NULL, NULL, NULL }, {
		"set-dragging", on_menu_set_dragging, "i", "0", on_menu_set_dragging_change_state }, {
		"set-statistics", on_menu_set_statistics, "i", "0", on_menu_set_statistics_change_state }, {
		"toggle-live-info-panel", on_menu_toggle_live_info_panel, NULL, "false", NULL }, {
		"toggle-static-grid", on_menu_toggle_static_grid, NULL, "false", NULL }, {
		"select-script", on_menu_select_script, "s", NULL, NULL } };

Application::Application(void)
{
	// NOTE: To reset the members of the class we could use a generic zero-padding like:
	// 	memset(this, 0, sizeof(class Application));
	// 	memset(this, 0, sizeof(*this));
	// Note however that this is only valid if the class hasn't virtual functions
	// For maintenance simplicity initialize all the members
	main_window = NULL;
	main_window_grid = NULL;
	popup_menu_darea = NULL;
	preferences_dialog = NULL;
	label_info = NULL;
	osc_widget = NULL;
	// NOTE: 'gtk_builder_new_from_file' is not included in the GTK version of the BBB
	// 	builder = gtk_builder_new_from_file("app-menu.ui");
	builder = gtk_builder_new();
	guint ok = gtk_builder_add_from_file(builder, "app-menu.ui", NULL);
	assert(ok);
	app = gtk_application_new("com.gmail.jose77105.plc-cape-oscilloscope",
			G_APPLICATION_FLAGS_NONE);
	g_signal_connect(app, "activate", G_CALLBACK (on_app_activate), this);
	recording_started = 0;
	end_thread_recorder = 0;
	trigger_mode = trigger_auto;
	trigger_interval_us = 500000;
	trigger_threshold_yunit = 0.0;
	trigger_threshold_samples = 0;
	trigger_freq_threshold = 0.0;
	trigger_freq_hz = 0;
	statistics_mode = statistics_none;
	recorder = new Recorder_plc();
	recorder->initialize();
	plugin_afe = NULL;
	active_configuration_filename = NULL;
	set_configuration_defaults();
}

Application::~Application()
{
	TRACE(3, "ini");
	recorder->terminate();
	recorder->release();
	release_configuration();
	// TODO: Improve the system to detect memory leaks, here based on G_IS_OBJECT asserts.
	//	That's not nice but practical. Look for a way to get the reference counter
	assert(G_IS_OBJECT(app));
	// Decrease the reference counter (to 0) and then release the 'app' object
	g_object_unref(app);
	// TODO: Remove extra 'unref'. Seems to be due to BBB patched menu management
#ifdef OLD_GDK
	g_object_unref(app);
#endif
	assert(!G_IS_OBJECT(app));
	// gtk_widget_destroy(GTK_WIDGET(builder));
	assert(G_IS_OBJECT(builder));
	g_object_unref(builder);
	assert(!G_IS_OBJECT(builder)); TRACE(3, "end");
	if (active_configuration_filename)
		free(active_configuration_filename);
}

void Application::release_configuration(void)
{
	if (plugin_afe)
	{
		delete plugin_afe;
		plugin_afe = NULL;
	}
	if (yunit_symbol)
	{
		free(yunit_symbol);
		yunit_symbol = NULL;
	}
}

void Application::set_configuration_defaults(void)
{
	memset((application_configuration*) this, 0, sizeof(struct application_configuration));
	buffer_interval_ms = DEFAULT_OSC_BUFFER_INTERVAL_MS;
	yunit_per_sample = 1.0;
	default_width = 800;
	default_height = 400;
}

int Application::begin_configuration(void)
{
	release_configuration();
	set_configuration_defaults();
	return 0;
}

int Application::set_configuration_setting(const char *identifier, const char *data)
{
	const struct plc_setting_definition *setting_definition = plc_setting_find_definition(
			accepted_settings, ARRAY_SIZE(accepted_settings), identifier);
	if (setting_definition != NULL)
	{
		union plc_setting_data *dst_data =
				(union plc_setting_data *) (((char *) (application_configuration*) this)
						+ setting_definition->user_data);
		plc_setting_data_release(setting_definition->type, dst_data);
		plc_setting_text_to_data(data, setting_definition->type, dst_data);
	}
	else
	{
		return -1;
	}
	return 0;
}

int Application::end_configuration(void)
{
	return 0;
}

int Application::do_menu_loop(int argc, char **argv)
{
	return g_application_run(G_APPLICATION(app), argc, argv);
}

gint Application::show_dialog(GtkMessageType type, GtkButtonsType buttons, const gchar *title,
		const gchar *message_format, ...)
{
	va_list args;
	va_start(args, message_format);
	char *text_buffer;
	int chars = vasprintf(&text_buffer, message_format, args);
	assert(chars != -1);
	GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(main_window), GTK_DIALOG_MODAL, type,
			buttons, "%s", text_buffer);
	va_end(args);
	free(text_buffer);
	gtk_window_set_title(GTK_WINDOW(dialog), title);
	gint result = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	return result;
}

void Application::quit(void)
{
	TRACE(3, "ini");
	if (recording_started)
		stop();
	gtk_widget_destroy(GTK_WIDGET(popup_menu_darea));
	gtk_widget_destroy(GTK_WIDGET(main_window));
	TRACE(3, "end");
}

void Application::main_window_release(void)
{
	TRACE(3, "ini");
	if (osc_widget)
	{
		delete osc_widget;
		osc_widget = NULL;
	}
	TRACE(3, "end");
}

void Application::update_recording_actions(void)
{
	GSimpleAction *action;
	action = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "start"));
	assert(action);
	g_simple_action_set_enabled(action, !recording_started);
	action = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "stop"));
	assert(action);
	g_simple_action_set_enabled(action, recording_started);
}

void Application::start(void)
{
	if (recording_started)
	{
		g_print("Recording already started\n");
		return;
	}
	recording_started = 1;
	end_thread_recorder = 0;
	int ret = pthread_create(&recorder_thread, NULL, thread_recorder_arg, this);
	assert(ret == 0);
	update_recording_actions();
}

void Application::stop(void)
{
	if (end_thread_recorder || !recording_started)
	{
		g_print("Recording already stopped\n");
		return;
	}
	end_thread_recorder = 1;
	int ret = pthread_join(recorder_thread, NULL);
	assert(ret == 0);
	recording_started = 0;
	update_recording_actions();
}

void Application::save_samples(void)
{
	save_to_file(save_to_file_samples);
}

void Application::save_png(void)
{
	save_to_file(save_to_file_png);
}

void Application::save_svg(void)
{
	save_to_file(save_to_file_svg);
}

void Application::save_to_file(enum save_to_file_options option)
{
	const char *option_caption = NULL;
	const char *filename = NULL;
	switch (option)
	{
	case save_to_file_samples:
		option_caption = "SAMPLES";
		filename = SAMPLES_FILENAME;
		break;
	case save_to_file_png:
		option_caption = "PNG";
		filename = PNG_FILENAME;
		break;
	case save_to_file_svg:
		option_caption = "SVG";
		filename = SVG_FILENAME;
		break;
	}
	gint result = show_dialog(GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "Confirmation",
			"Save %s to '%s'?", option_caption, filename);
	if (result == GTK_RESPONSE_YES)
	{
		int saving_result;
		switch (option)
		{
		case save_to_file_samples:
			saving_result = osc_widget->save_samples(filename, 0);
			break;
		case save_to_file_png:
			saving_result = osc_widget->save_png(filename);
			break;
		case save_to_file_svg:
			saving_result = osc_widget->save_svg(filename);
			break;
		}
		if (saving_result == 0)
			show_dialog(GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "Notification", "%s saved to '%s'",
					option_caption, filename);
		else
			show_dialog(GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Error",
					"Error when saving %s in '%s': %s", option_caption, filename,
					g_strerror(errno));
	}
}

void Application::get_last_configuration_name(void)
{
	int fd = open("plc-cape-oscilloscope-configuration", O_RDONLY);
	if (fd >= 0)
	{
		char configuration_name[CONFIGURATION_NAME_MAX_LEN + 1];
		size_t bytes_read = read(fd, configuration_name, CONFIGURATION_NAME_MAX_LEN);
		if (bytes_read > 0)
		{
			configuration_name[bytes_read] = '\0';
			if (active_configuration_filename)
				free(active_configuration_filename);
			active_configuration_filename = strdup(configuration_name);
		}
		close(fd);
	}
}

void Application::reload_configuration(void)
{
	if (active_configuration_filename)
		load_configuration(active_configuration_filename);
}

void Application::load_configuration(const char *filename)
{
	// afe_cfg
	int ret = app_cfg->load_configuration(filename);
	assert(ret == 0);
	app_cfg->set_configuration("app-settings", this);
	app_cfg->set_configuration("recorder-settings", recorder);
	app_cfg->set_configuration("plot-widget-settings", osc_widget);
	app_cfg->set_configuration("plot-area-settings",
			osc_widget->get_plot_area_configuration_interface());
	// osc_widget
	osc_widget->set_yunit_symbol(yunit_symbol);
	osc_widget->set_yunit_per_sample(yunit_per_sample);
	osc_widget->set_osc_buffer(buffer_interval_ms, 1000000.0 / recorder->get_real_capturing_rate());
	set_trigger_threshold_yunit(0.1);
	osc_widget->show_info_widget(info_panel_visible);
	// plugin_afe
	assert(plugin_afe == NULL);
	plugin_afe = load_afe_plugin ? new Plugin_afe() : NULL;
	if (plugin_afe)
	{
		// Initialize plugin to a known state
		plugin_afe_set_gain_rx_pga1(afe_gain_rx_pga1_025);
		plugin_afe_set_gain_rx_pga2(afe_gain_rx_pga2_1);
		plugin_afe_set_filtering(afe_filtering_cenelec_bcd);
	}
	// Update actions states to synchronize buttons
	GAction *action = g_action_map_lookup_action(G_ACTION_MAP(app), "toggle-grid");
	assert(action);
	g_action_change_state(action, g_variant_new_boolean(osc_widget->get_grid()));
	// Store in a file the last configuration selected
	if ((active_configuration_filename == NULL)
			|| (strcmp(active_configuration_filename, filename) != 0))
	{
		int fd = open("plc-cape-oscilloscope-configuration", O_CREAT | O_WRONLY | O_TRUNC,
		S_IWUSR | S_IRUSR);
		assert(fd >= 0);
		size_t bytes_to_write = strlen(filename);
		size_t bytes_written = write(fd, filename, bytes_to_write);
		assert(bytes_written == bytes_to_write);
		close(fd);
		// Save the active configuration filename for auto-reloading if necessary
		if (active_configuration_filename)
			free(active_configuration_filename);
		active_configuration_filename = strdup(filename);
	}
}

void Application::dialog_trigger_mode_changed(void)
{
	GObject *widget = gtk_builder_get_object(builder, "triggerComboBox");
	gint index = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	GtkWidget *triggerThresholdFrame = GTK_WIDGET(
			gtk_builder_get_object(builder, "triggerThresholdFrame"));
	GtkWidget *triggerTimedFrame = GTK_WIDGET(gtk_builder_get_object(builder, "triggerTimedFrame"));
	GtkWidget *triggerFreqThresholdFrame = GTK_WIDGET(
			gtk_builder_get_object(builder, "triggerFreqThresholdFrame"));
	gtk_widget_set_visible(triggerThresholdFrame, false);
	gtk_widget_set_visible(triggerTimedFrame, false);
	gtk_widget_set_visible(triggerFreqThresholdFrame, false);
	switch (index)
	{
	case trigger_auto:
		break;
	case trigger_threshold_repetitive:
	case trigger_threshold_single:
		gtk_widget_set_visible(triggerThresholdFrame, true);
		break;
	case trigger_timed:
		gtk_widget_set_visible(triggerTimedFrame, true);
		break;
	case trigger_freq_threshold_repetitive:
	case trigger_freq_threshold_single:
		gtk_widget_set_visible(triggerFreqThresholdFrame, true);
		break;
	}
}

void Application::dialog_show(void)
{
	settings_to_dialog();
}

void Application::dialog_destroy(void)
{
	preferences_dialog = NULL;
}

void Application::dialog_apply(void)
{
	dialog_to_settings();
}

void Application::on_dialog_ok(GtkWidget *widget, gpointer data)
{
	((Application *) data)->dialog_apply();
	GtkWidget *toplevel = gtk_widget_get_toplevel(widget);
	gtk_widget_destroy(toplevel);
}

void Application::settings_to_dialog(void)
{
	char text_buffer[32];
	GObject *widget;
	settings_to_dialog_format("capturingRateSpsEntry", "%.2f", recorder->get_real_capturing_rate());
	settings_to_dialog_format("oscBufferIntervalEntry", "%.2f",
			osc_widget->get_osc_buffer_interval());
	// Trigger mode
	widget = gtk_builder_get_object(builder, "triggerComboBox");
	gtk_combo_box_set_active(GTK_COMBO_BOX(widget), trigger_mode);
	// Trigger interval
	widget = gtk_builder_get_object(builder, "triggerIntervalEntry");
	sprintf(text_buffer, "%d", trigger_interval_us);
	gtk_entry_set_text(GTK_ENTRY(widget), text_buffer);
	settings_to_dialog_format("triggerThresholdEntry", "%.2f", trigger_threshold_yunit);
	settings_to_dialog_format("triggerFreqThresholdEntry", "%.2f", trigger_freq_threshold);
	settings_to_dialog_format("triggerFreqEntry", "%.2f", trigger_freq_hz);
}

void Application::settings_to_dialog_format(const char *widget_id, const char *format, ...)
{
	va_list args;
	char text_buffer[32];
	GObject *widget = gtk_builder_get_object(builder, widget_id);
	va_start(args, format);
	vsprintf(text_buffer, format, args);
	va_end(args);
	gtk_entry_set_text(GTK_ENTRY(widget), text_buffer);
}

void Application::dialog_to_settings(void)
{
	GObject *widget;
	recorder->set_preferred_capturing_rate(dialog_to_settings_float("capturingRateSpsEntry"));
	// If capturing_rate has changed the osc_buffer will be reallocated
	osc_widget->set_osc_buffer(dialog_to_settings_float("oscBufferIntervalEntry"),
			1000000.0 / recorder->get_real_capturing_rate());
	// Trigger mode
	widget = gtk_builder_get_object(builder, "triggerComboBox");
	gint index = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
	trigger_mode = (enum trigger_mode) index;
	// Trigger interval
	widget = gtk_builder_get_object(builder, "triggerIntervalEntry");
	trigger_interval_us = atoi(gtk_entry_get_text(GTK_ENTRY(widget)));
	set_trigger_threshold_yunit(dialog_to_settings_float("triggerThresholdEntry"));
	trigger_freq_threshold = dialog_to_settings_float("triggerFreqThresholdEntry");
	trigger_freq_hz = dialog_to_settings_float("triggerFreqEntry");
}

float Application::dialog_to_settings_float(const char *widget_id)
{
	GObject *widget = gtk_builder_get_object(builder, widget_id);
	return strtof(gtk_entry_get_text(GTK_ENTRY(widget)), NULL);
}

void Application::show_settings(void)
{
	if (preferences_dialog == NULL)
	{
		// TODO: Refactor. Encapsulate in a class
		guint ok = gtk_builder_add_from_file(builder, "preferences.ui", NULL);
		assert(ok);
		gint x, y;
		gint width, height;
		gtk_window_get_position(GTK_WINDOW(main_window), &x, &y);
		gtk_window_get_size(GTK_WINDOW(main_window), &width, &height);
		preferences_dialog = gtk_builder_get_object(builder, "preferences_dialog");
		// By default move preferences window to 'main_window' right border and some margin
		gtk_window_move(GTK_WINDOW(preferences_dialog), x + width + 5, y);
		g_signal_connect(preferences_dialog, "show", G_CALLBACK(on_dialog_show), this);
		g_signal_connect(preferences_dialog, "destroy", G_CALLBACK(on_dialog_destroy), this);
		GtkWidget *trigger_mode = GTK_WIDGET(gtk_builder_get_object(builder, "triggerComboBox"));
		g_signal_connect(trigger_mode, "changed", G_CALLBACK(on_dialog_trigger_mode_changed), this);
		GObject *apply_button = gtk_builder_get_object(builder, "applyButton");
		g_signal_connect(apply_button, "clicked", G_CALLBACK(on_dialog_apply), this);
		GtkWidget *ok_button = GTK_WIDGET(gtk_builder_get_object(builder, "okButton"));
		g_signal_connect(ok_button, "clicked", G_CALLBACK(on_dialog_ok), this);
		// Set the default button
		// NOTE: 'activate-default' is not called if a default 'GtkEntry' widget has the focus
		//	To allow the ENTER in a 'GtkEntry' to call the default action, it must be explicitly
		//	declared with 'activates-default':
		//		<object class="GtkEntry" id="yUnitRangeEntry">
		//			<property name="activates-default">True</property>
		//		</object>
		gtk_widget_set_can_default(ok_button, TRUE);
		gtk_window_set_default(GTK_WINDOW(preferences_dialog), ok_button);
		g_signal_connect(preferences_dialog, "activate-default", G_CALLBACK(on_dialog_ok), this);
		gtk_widget_show_all(GTK_WIDGET(preferences_dialog));
	}
	else
	{
		gtk_window_present(GTK_WINDOW(preferences_dialog));
	}
}

void Application::show_plot_widget_settings(void)
{
	osc_widget->show_settings(GTK_WINDOW(main_window));
}

void Application::show_about(void)
{
	// The ending 'slashes' are to prevent "Automatic Code Formatters" to join the lines
	gtk_show_about_dialog(NULL, //
			"copyright", "Copyright (C) 2017 Jose Maria Ortega", //
			"license-type", GTK_LICENSE_GPL_3_0, //
			"version", "0.1", //
			NULL);
}

void Application::add_toolbar_button(GtkWidget *toolbar, const char *icon_id, const char *icon_text,
		GCallback on_click)
{
	// In GTK < 3.1 use gtk_tool_button_new_from_stock(GTK_STOCK_NEW)
	GtkToolItem *button = gtk_tool_button_new(
			gtk_image_new_from_icon_name(icon_id, GTK_ICON_SIZE_SMALL_TOOLBAR), icon_text);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), button, -1);
	g_signal_connect(G_OBJECT(button), "clicked", on_click, this);
}

void Application::add_toolbar_button_action(GtkWidget *toolbar, const char *icon_id,
		const char *icon_text, const char *action_name)
{
	GtkToolItem *button = gtk_tool_button_new(
			gtk_image_new_from_icon_name(icon_id, GTK_ICON_SIZE_SMALL_TOOLBAR), icon_text);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), button, -1);
	gtk_actionable_set_action_name(GTK_ACTIONABLE(button), action_name);
}

void Application::add_toolbar_toggle_button(GtkWidget *toolbar, const char *icon_id,
		const char *icon_text, GCallback on_toggle)
{
	GtkToolItem *toggle_button = gtk_toggle_tool_button_new();
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(toggle_button), icon_text);
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(toggle_button), icon_id);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toggle_button, -1);
	g_signal_connect(G_OBJECT(toggle_button), "toggled", on_toggle, this);
}

void Application::add_toolbar_toggle_button_action(GtkWidget *toolbar, const char *icon_id,
		const char *icon_text, const char *action_name)
{
	GtkToolItem *toggle_button = gtk_toggle_tool_button_new();
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(toggle_button), icon_text);
	gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(toggle_button), icon_id);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toggle_button, -1);
	gtk_actionable_set_action_name(GTK_ACTIONABLE(toggle_button), action_name);
}

void Application::add_toolbar_menu_button(GtkWidget *toolbar, const char *icon_id,
		const char *icon_text, const char *menu_id, GCallback on_click)
{
	GMenuModel *menu_model = G_MENU_MODEL(gtk_builder_get_object(builder, menu_id));
	assert(menu_model);
	GtkWidget *menu = menu_new_from_model(menu_model);
	GtkToolItem *menu_button = gtk_menu_tool_button_new(
			gtk_image_new_from_icon_name(icon_id, GTK_ICON_SIZE_SMALL_TOOLBAR), icon_text);
	gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(menu_button), GTK_WIDGET(menu));
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), menu_button, -1);
	g_signal_connect(G_OBJECT(menu_button), "clicked", on_click, this);
}

void Application::add_toolbar_menu_button_action(GtkWidget *toolbar, const char *icon_id,
		const char *icon_text, const char *menu_id, const char *action_name,
		int button_action_index)
{
	GMenuModel *menu_model = G_MENU_MODEL(gtk_builder_get_object(builder, menu_id));
	assert(menu_model);
	GtkWidget *menu = menu_new_from_model(menu_model);
	GtkToolItem *menu_button = gtk_menu_tool_button_new(
			gtk_image_new_from_icon_name(icon_id, GTK_ICON_SIZE_SMALL_TOOLBAR), icon_text);
	gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(menu_button), GTK_WIDGET(menu));
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), menu_button, -1);
	gtk_actionable_set_action_name(GTK_ACTIONABLE(menu_button), action_name);
	gtk_actionable_set_action_target_value(GTK_ACTIONABLE(menu_button),
			g_variant_new_int32(button_action_index));
}

GtkWidget *Application::create_toolbar_box(void)
{
	// In my BBB I need to create the toolbar with GTK_ORIENTATION_VERTICAL to be properly shown
	// TODO: Analyze why this patch is required
#ifdef __arm__
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
#else
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#endif
	GtkWidget *toolbar = gtk_toolbar_new();
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH);
	gtk_container_set_border_width(GTK_CONTAINER(toolbar), 0);
	add_toolbar_button_action(toolbar, "media-playback-start", "Start", "app.start");
	add_toolbar_button_action(toolbar, "media-playback-stop", "Stop", "app.stop");
	add_toolbar_menu_button_action(toolbar, "zoom-fit-best", "Dragging", "dragging-mode-menu",
			"app.set-dragging", Plot_widget::dragging_free_cursor);
	add_toolbar_toggle_button_action(toolbar, "document-properties", "Grid", "app.toggle-grid");
	add_toolbar_toggle_button_action(toolbar, "document-properties", "Samples",
			"app.toggle-samples");
	add_toolbar_toggle_button_action(toolbar, "document-properties", "FFT", "app.toggle-fft");
	add_toolbar_button(toolbar, "preferences-other", "Preferences",
			G_CALLBACK(on_preferences_click));
	add_toolbar_button(toolbar, "preferences-other", "Graph Settings",
			G_CALLBACK(on_plot_widget_settings_click));
	GtkToolItem *separator = gtk_separator_tool_item_new();
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), separator, -1);
	add_toolbar_button(toolbar, "application-exit", "Quit", G_CALLBACK(on_quit));
	gtk_box_pack_start(GTK_BOX(box), toolbar, FALSE, FALSE, 0);
	return box;
}

void Application::auto_offset_y(void)
{
	osc_widget->auto_offset_y();
}

void Application::auto_scale_y(void)
{
	osc_widget->auto_scale_y();
}

void Application::auto_scale_x(void)
{
	osc_widget->auto_scale_x();
}

// TODO: Move 'plugin_afe'-functions to 'plugin_afe.c'
void Application::plugin_afe_set_gain_rx_pga1(enum afe_gain_rx_pga1_enum gain_rx_pga1)
{
	static const float rx_pga1_gains[afe_gain_rx_pga1_COUNT] = {
		0.25, 0.5, 1.0, 2.0 };
	assert(plugin_afe);
	plugin_afe->execute_command(command_plc_afe_set_rx_pga1_gain, (void*) gain_rx_pga1, NULL);
	if (yunit_symbol != NULL)
		osc_widget->set_yunit_per_sample(yunit_per_sample * rx_pga1_gains[gain_rx_pga1]);
}

void Application::plugin_afe_set_gain_rx_pga2(enum afe_gain_rx_pga2_enum gain_rx_pga2)
{
	static const float rx_pga2_gains[afe_gain_rx_pga2_COUNT] = {
		1, 4, 16, 64 };
	assert(plugin_afe);
	plugin_afe->execute_command(command_plc_afe_set_rx_pga2_gain, (void*) gain_rx_pga2, NULL);
	if (yunit_symbol != NULL)
		osc_widget->set_yunit_per_sample(yunit_per_sample * rx_pga2_gains[gain_rx_pga2]);
}

void Application::plugin_afe_set_filtering(enum afe_filtering_enum filtering)
{
	assert(plugin_afe);
	plugin_afe->execute_command(command_plc_afe_set_filtering_band, (void*) filtering, NULL);
}

void Application::on_plugin_afe_menu_set_pga1(GtkMenuItem *menuitem, gpointer data)
{
	// On Radio and Check buttons 'activate' signal is called twice, one for the deactivated item
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem)))
		((Application*) (data))->plugin_afe_set_gain_rx_pga1(
				(afe_gain_rx_pga1_enum) (uint32_t) g_object_get_data(G_OBJECT(menuitem),
						"plc:index"));
}

void Application::on_plugin_afe_menu_set_pga2(GtkMenuItem *menuitem, gpointer data)
{
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem)))
		((Application*) (data))->plugin_afe_set_gain_rx_pga2(
				(afe_gain_rx_pga2_enum) (uint32_t) g_object_get_data(G_OBJECT(menuitem),
						"plc:index"));
}

void Application::on_plugin_afe_menu_set_filtering(GtkMenuItem *menuitem, gpointer data)
{
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem)))
		((Application*) (data))->plugin_afe_set_filtering(
				(afe_filtering_enum) (uint32_t) g_object_get_data(G_OBJECT(menuitem), "plc:index"));
}

void Application::on_set_drawing_mode(GtkMenuItem *menuitem, gpointer data)
{
	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem)))
		((Application*) (data))->osc_widget->set_graph_drawing_mode(
				(enum graph_drawing_mode) (uint32_t) g_object_get_data(G_OBJECT(menuitem),
						"plc:index"));
}

void Application::on_set_ybase_to_center(GSimpleAction *action, GVariant* parameter,
		gpointer user_data)
{
	((Application*) user_data)->osc_widget->set_ybase_to_center();
}

void Application::on_set_xybase_to_cursor(GSimpleAction *action, GVariant* parameter,
		gpointer user_data)
{
	((Application*) user_data)->osc_widget->set_xybase_to_cursor();
}

void Application::on_test_item(GSimpleAction *action, GVariant* parameter, gpointer user_data)
{
	g_warning("%s called.\n", G_STRFUNC);
	((Application*) user_data)->show_about();
}

GActionEntry Application::action_entries[] = {
	{
		"set-ybase-to-center", on_set_ybase_to_center }, {
		"set-xybase-to-cursor", on_set_xybase_to_cursor }, {
		"test2", on_test_item } };

GtkWidget *Application::create_popup_menu_darea(void)
{
	GtkWidget *menuitem;
	GtkWidget *popup_menu = gtk_menu_new();
	// Shortcuts
	static const char* shortcuts_menu_definition = "<interface>"
			"  <menu id='shortcuts-menu'>"
			"    <section>"
			"      <item>"
			"        <attribute name='label' translatable='yes'>Start</attribute>"
			"        <attribute name='action'>app.start</attribute>"
			"      </item>"
			"      <item>"
			"        <attribute name='label'>Stop</attribute>"
			"        <attribute name='action'>app.stop</attribute>"
			"      </item>"
			"      <item>"
			"        <attribute name='label'>Set Y-Ref to center</attribute>"
			"        <attribute name='action'>app.set-ybase-to-center</attribute>"
			"      </item>"
			"      <item>"
			"        <attribute name='label'>Set X &amp; Y refs to cursor</attribute>"
			"        <attribute name='action'>app.set-xybase-to-cursor</attribute>"
			"      </item>"
			"    </section>"
			"  </menu>"
			"</interface>";
	GError* error = NULL;
	gtk_builder_add_from_string(builder, shortcuts_menu_definition, -1, &error);
	g_assert_no_error(error);
	GMenu *shortcuts_gmenu = G_MENU(gtk_builder_get_object(builder, "shortcuts-menu"));
	assert(shortcuts_gmenu);
	GMenuModel *shortcuts_menu_model = G_MENU_MODEL(shortcuts_gmenu);
	assert(shortcuts_menu_model);
	GtkWidget *shorcuts_menu_widget = menu_new_from_model(shortcuts_menu_model);
	assert(shorcuts_menu_widget && (g_menu_model_get_n_items(shortcuts_menu_model) == 1));
	// Drawing mode
	GtkWidget *drawing_mode_submenu = add_submenu_entries_enum("Drawing mode",
			graph_drawing_mode_text, graph_drawing_COUNT, (GCallback) on_set_drawing_mode);
	gtk_menu_shell_append(GTK_MENU_SHELL(shorcuts_menu_widget), drawing_mode_submenu);
	// Shortcuts
	menuitem = gtk_menu_item_new_with_label("Shortcuts");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), shorcuts_menu_widget);
	gtk_menu_shell_append(GTK_MENU_SHELL(popup_menu), menuitem);
	gtk_widget_show(menuitem);
	// Action map
	g_action_map_add_action_entries(G_ACTION_MAP(app), action_entries, G_N_ELEMENTS(action_entries),
			this);
	// Dragging
	GMenuModel *dragging_menu_model = G_MENU_MODEL(
			gtk_builder_get_object(builder, "dragging-mode-menu"));
	assert(dragging_menu_model);
	GtkWidget *dragging_submenu = menu_new_from_model(dragging_menu_model);
	menuitem = gtk_menu_item_new_with_label("Dragging");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), dragging_submenu);
	gtk_menu_shell_append(GTK_MENU_SHELL(popup_menu), menuitem);
	gtk_widget_show(menuitem);
	// Tools
	GMenuModel *tools_menu_model = G_MENU_MODEL(gtk_builder_get_object(builder, "tools-menu"));
	assert(tools_menu_model);
	GtkWidget *tools_submenu = menu_new_from_model(tools_menu_model);
	menuitem = gtk_menu_item_new_with_label("Tools");
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), tools_submenu);
	gtk_menu_shell_append(GTK_MENU_SHELL(popup_menu), menuitem);
	gtk_widget_show(menuitem);
	// Plugin_afe
	if (plugin_afe)
	{
		GtkWidget *afe_submenu = gtk_menu_new();
		GtkWidget *afe_submenu_pga1 = add_submenu_entries_enum("RX PGA1 GAIN",
				afe_gain_rx_pga1_enum_text, afe_gain_rx_pga1_COUNT,
				(GCallback) on_plugin_afe_menu_set_pga1);
		gtk_menu_shell_append(GTK_MENU_SHELL(afe_submenu), afe_submenu_pga1);
		GtkWidget *afe_submenu_pga2 = add_submenu_entries_enum("RX PGA2 GAIN",
				afe_gain_rx_pga2_enum_text, afe_gain_rx_pga2_COUNT,
				(GCallback) on_plugin_afe_menu_set_pga2);
		gtk_menu_shell_append(GTK_MENU_SHELL(afe_submenu), afe_submenu_pga2);
		GtkWidget *afe_submenu_filtering = add_submenu_entries_enum("RX Filtering",
				afe_filtering_enum_text, afe_filtering_COUNT,
				(GCallback) on_plugin_afe_menu_set_filtering);
		gtk_menu_shell_append(GTK_MENU_SHELL(afe_submenu), afe_submenu_filtering);
		GtkWidget *afe_menu = gtk_menu_item_new_with_label("AFE");
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(afe_menu), afe_submenu);
		gtk_menu_shell_append(GTK_MENU_SHELL(popup_menu), afe_menu);
		gtk_widget_show(afe_menu);
	}
	// gtk_widget_show_all(popup_menu);
	return popup_menu;
}

GtkWidget *Application::add_submenu_entries_enum(const char *submenu_label, const char *enum_text[],
		uint32_t enum_count, GCallback activate_callback)
{
	GtkWidget *submenu = gtk_menu_new();
	uint32_t n;
	GSList *group = NULL;
	for (n = 0; n < enum_count; n++)
	{
		GtkWidget *menuitem = gtk_radio_menu_item_new_with_label(group, enum_text[n]);
		g_object_set_data(G_OBJECT(menuitem), "plc:index", (gpointer) n);
		group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(menuitem));
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
		gtk_widget_show(menuitem);
		g_signal_connect(menuitem, "activate", activate_callback, this);
		if (n == 0)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), TRUE);
	}
	GtkWidget *main_submenu = gtk_menu_item_new_with_label(submenu_label);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(main_submenu), submenu);
	gtk_widget_show(main_submenu);
	return main_submenu;
}

void Application::activate(void)
{
	// Only initialize application window once
	if (main_window == NULL)
		initialize_main_window();
	// Show the main_window and all the actual child controls
	gtk_widget_show_all(GTK_WIDGET(main_window));
	// Add panel info initially visible or hidden according to settings
	// Do it after the grid is shown to properly update the grid-columns
	show_info_panel(info_panel_visible);
}

void Application::initialize_main_window(void)
{
	uint32_t n;
	main_window = GTK_WINDOW(gtk_application_window_new(app));
	g_action_map_add_action_entries(G_ACTION_MAP(app), app_entries, G_N_ELEMENTS(app_entries),
			this);
	assert(osc_widget == NULL);
	osc_widget = new Plot_widget(G_ACTION_MAP(app), builder);
	get_last_configuration_name();
	reload_configuration();
	// main_menu
	GMenuModel *app_menu = G_MENU_MODEL(gtk_builder_get_object(builder, "appmenu"));
	// Add "dynamic" items
	GMenu *statistics_menu = G_MENU(gtk_builder_get_object(builder, "statistics-menu"));
	assert(statistics_menu);
	for (n = 0; n < ARRAY_SIZE(statistics_mode_text); n++)
	{
		// For GTK 3.10 we can use the 'detailed_action' format for simpler implementation
		//		char detailed_action[32];
		//		sprintf(detailed_action, "app.set-statistics(%d)", n);
		//		GMenuItem *menu_item = g_menu_item_new(statistics_mode_text[n], detailed_action);
		// However I've faced some issue with the 3.4. So, for portability use the basic method
		GMenuItem *menu_item = g_menu_item_new(statistics_mode_text[n], NULL);
		g_menu_item_set_action_and_target_value(menu_item, "app.set-statistics",
				g_variant_new_int32(n));
		g_menu_append_item(statistics_menu, menu_item);
		g_object_unref(menu_item);
	}
	// Add configurations
	GMenu *configuration_menu = G_MENU(gtk_builder_get_object(builder, "load-configuration"));
	assert(configuration_menu);
	char *app_abs_dir = plc_application_get_abs_dir();
	DIR *dir = opendir(app_abs_dir);
	if (dir)
	{
		struct dirent *entry;
		while ((entry = readdir(dir)) != NULL)
		{
			if (entry->d_type == DT_REG)
			{
				const char *d_name_ext = strrchr(entry->d_name, '.');
				if (d_name_ext && (strcmp(d_name_ext, ".cfg") == 0))
				{
					GMenuItem *menu_item = g_menu_item_new(entry->d_name, NULL);
					g_menu_item_set_action_and_target_value(menu_item, "app.load-configuration",
							g_variant_new_string(entry->d_name));
					g_menu_append_item(configuration_menu, menu_item);
					g_object_unref(menu_item);
				}
			}
		}
		closedir(dir);
	}
	free(app_abs_dir);
	gtk_application_set_menubar(GTK_APPLICATION(app), app_menu);
	assert(popup_menu_darea == NULL);
	popup_menu_darea = create_popup_menu_darea();
	// Attach menu to 'main_window' to be able to use application actions like "app.start"
	gtk_menu_attach_to_widget(GTK_MENU(popup_menu_darea), GTK_WIDGET(main_window), NULL);
	// Update enabled status of menu items
	update_recording_actions();
	// Main window pos
	gtk_window_set_title(main_window, "plc-cape-oscilloscope");
	gtk_container_set_border_width(GTK_CONTAINER(main_window), 0);
	if ((default_width != 0) && (default_height != 0))
		gtk_window_set_default_size(main_window, default_width, default_height);
	gtk_window_set_position(main_window, GTK_WIN_POS_CENTER);
	// Hook the "destroy" signal for proper cleaning
	g_signal_connect(G_OBJECT(main_window), "destroy", G_CALLBACK(on_main_window_destroy), this);
	main_window_grid = gtk_grid_new();
	gtk_container_add(GTK_CONTAINER(main_window), main_window_grid);
	GtkWidget *box = create_toolbar_box();
	gtk_grid_attach(GTK_GRID(main_window_grid), box, 0, 0, GRID_COLUMNS, 1);
	// Label info
	label_info = GTK_LABEL(gtk_label_new("Status info"));
	gtk_grid_attach(GTK_GRID(main_window_grid), GTK_WIDGET(label_info), 0, 2, GRID_COLUMNS, 1);
	// Process requests to open a popup menu through the standard Shift-F10 way
	g_signal_connect(G_OBJECT(main_window), "popup-menu", G_CALLBACK(on_main_window_popup_menu),
			this);
	// Oscilloscope widget
	osc_widget->set_popup_menu(popup_menu_darea);
	gtk_grid_attach(GTK_GRID(main_window_grid), osc_widget->get_graph_widget(), 0, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(main_window_grid), osc_widget->get_info_widget(), GRID_COLUMNS - 1, 1,
			1, 1);
}

void Application::show_info_panel(int visible)
{
	info_panel_visible = visible;
	osc_widget->show_info_widget(visible);
}

void Application::set_trigger_threshold_yunit(float value)
{
	trigger_threshold_yunit = value;
	trigger_threshold_samples = SAMPLES_ZERO_REF
			+ trigger_threshold_yunit / osc_widget->get_yunit_per_sample();
}

void *Application::thread_recorder(void)
{
	// Long-enough buffer to accommodate statistics text
	static const size_t statistics_size = 1000;
	char *statistics = (char*) malloc(statistics_size);
	uint32_t samples_buffer_count = 1024;
	sample_rx_t *samples_buffer = (sample_rx_t*) calloc(1,
			samples_buffer_count * sizeof(sample_rx_t));
	float freq_adc_sps = recorder->get_real_capturing_rate();
	float time_per_sample_us = 1000000.0 / freq_adc_sps;
	int thread_not_requited = 0;
	recorder->start_recording();
	recorder->pause();
	// TODO: Change this contention delay by a better way. Used here to avoid race conditions
	sleep(1);
	uint32_t stamp_ini_ms = plc_time_get_tick_ms();
	struct timespec stamp_next;
	if (trigger_mode == trigger_timed)
		stamp_next = plc_time_get_hires_stamp();
	recorder->resume();
	// TODO: 'trigger_mode' (and others) are modified from the UI thread (dialog) -> Implement lock
	int trigger_freq_based = ((trigger_mode == trigger_freq_threshold_repetitive)
			|| (trigger_mode == trigger_freq_threshold_single));
	int trigger_threshold_based = ((trigger_mode == trigger_threshold_repetitive)
			|| (trigger_mode == trigger_threshold_single) || trigger_freq_based);
	int trigger_threshold_detected = 0;
	while (!end_thread_recorder && !thread_not_requited)
	{
		recorder->pop_recorded_buffer(samples_buffer, samples_buffer_count);
		if (trigger_freq_based && !trigger_threshold_detected)
		{
			float xr = 0.0, xc = 0.0;
			// PRECONDITION: threshold_freq <= freq_adc_sps/2
			float threshold_w = 2.0 * M_PI * trigger_freq_hz / freq_adc_sps;
			uint32_t n;
			for (n = 0; n < samples_buffer_count; n++)
			{
				xr += ((int16_t) (samples_buffer[n] - SAMPLES_ZERO_REF)) * cos(threshold_w * n);
				xc += ((int16_t) (samples_buffer[n] - SAMPLES_ZERO_REF)) * sin(threshold_w * n);
			}
			float threshold_abs = sqrt(xr * xr + xc * xc) / samples_buffer_count;
			if (threshold_abs > trigger_freq_threshold)
			{
				trigger_threshold_detected = 1;
				if (trigger_mode == trigger_threshold_single)
					thread_not_requited = 1;
			}
			else
			{
				continue;
			}
		}
		else if (trigger_threshold_based && !trigger_threshold_detected)
		{
			uint32_t n;
			for (n = 0; n < samples_buffer_count; n++)
				if (samples_buffer[n] > trigger_threshold_samples)
				{
					trigger_threshold_detected = 1;
					if (trigger_mode == trigger_threshold_single)
						thread_not_requited = 1;
					break;
				}
			if (!trigger_threshold_detected)
				continue;
		}
		if (osc_widget->push_buffer(samples_buffer, samples_buffer_count))
		{
			switch (statistics_mode)
			{
			case statistics_none:
				*statistics = '\0';
				break;
			case statistics_adc_buffer:
				recorder->get_statistics(statistics, statistics_size);
				break;
			case statistics_osc_buffer:
				uint32_t n;
				uint32_t ac_mean = 0;
				for (n = 0; n < samples_buffer_count; n++)
					ac_mean += abs(samples_buffer[n] - SAMPLES_ZERO_REF);
				snprintf(statistics, statistics_size, "Tick: %6d ms; AC Mean: %d; Tcycle: %.1f us",
						plc_time_get_tick_ms() - stamp_ini_ms, ac_mean / samples_buffer_count,
						time_per_sample_us * samples_buffer_count);
				break;
			}
			gtk_label_set_text(label_info, statistics);

			if (trigger_mode == trigger_timed)
			{
				struct timespec stamp = plc_time_get_hires_stamp();
				int32_t time_to_next_us = plc_time_hires_interval_to_usec(stamp, stamp_next);
				if (time_to_next_us > 0)
				{
					recorder->pause();
					usleep(time_to_next_us);
					recorder->resume();
				}
				plc_time_add_usec_to_hires_interval(&stamp_next, trigger_interval_us);
			}
			else
			{
				if (trigger_threshold_based)
					trigger_threshold_detected = 0;
				// TODO: Improve contention mechanism. At least make it configurable
				recorder->pause();
				usleep(100000);
				recorder->resume();
			}
		}
	}
	recorder->stop_recording();
	free(samples_buffer);
	free(statistics);
	return NULL;
}

#ifdef OLD_GDK
void on_menu_detached(GtkWidget *attach_widget, GtkMenu *menu)
{
	TRACE(3, "Menu detached Widget: %x, Menu: %x", (uint32_t)attach_widget, (uint32_t)menu);
}
#endif

GtkWidget* Application::menu_new_from_model(GMenuModel *model)
{
	GtkWidget *menu_widget = gtk_menu_new_from_model(model);
	assert(menu_widget);
#ifdef OLD_GDK
	// I have faced some issues with menus created with 'gtk_menu_new_from_model' in GTK 3.4 (BBB).
	// It would seem that the actions are not properly linked to the main window and then, the
	// corresponding items are hidden (in Ubuntu with GTK 3.10, not-linked actions are disabled)
	// TEMPORARY PATCH:
	//	* call 'gtk_menu_attach_to_widget' to associate the menu actions with the main window
	//	* add an extra reference to 'menu'
	//	* call 'gtk_menu_detach' to remove the attchment and allow subsequent 
	//	  'gtk_menu_item_set_submenu' doing the proper linking. The menu still holds the reference
	//	  to the main window because the extra reference
	gtk_menu_attach_to_widget(GTK_MENU(menu_widget), GTK_WIDGET(main_window), on_menu_detached);
	g_object_ref(GTK_MENU(menu_widget));
	gtk_menu_detach(GTK_MENU(menu_widget));
	assert(gtk_menu_get_attach_widget(GTK_MENU(menu_widget)) == NULL);
#endif
	return menu_widget;
}

/**
 * @file
 * @brief	Graphic UI entry point
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef APPLICATION_H
#define APPLICATION_H

#include <gtk/gtk.h>
#include "configuration.h"
#include "plugin_afe.h"

#ifndef GDK_VERSION_3_10
#define OLD_GDK
#endif

// TODO: Refactor for more readable and modular code

// The GTK version of the off-the-shelf distribution in the BBBs (year 2016) is 3.4.x
//	https://developer.gnome.org/gtk3/3.4

// NOTE ON GTK CALLBACKS:
//	Most GTK callbacks end with 'gpointer user_data' parameter
//	If a callback function only uses 'user_data', a function prototype having just that parameter is
//	valid. That is because the C passing parameter convention is from right-to-left

#define DECLARE_BASIC_WIDGET_REDIR(on_action, action) \
	static void on_action(GtkWidget *widget, gpointer data) { ((Application *) data)->action(); } \
	void action(void);

#define DECLARE_BASIC_MENU_REDIR(on_menu_selection, action) \
		static void on_menu_selection(GSimpleAction *action, GVariant *parameter, gpointer data) { \
				((Application *) data)->action(); } \
	void action(void);

#define DECLARE_BUTTON_AND_MENU_REDIR(on_click, on_menu_selection, action) \
	static void on_click(GtkWidget *widget, gpointer data) { ((Application *) data)->action(); } \
	static void on_menu_selection(GSimpleAction *action, GVariant *parameter, gpointer data) { \
				((Application *) data)->action(); } \
	void action(void);

class Configuration_interface;
class Recorder_interface;

struct application_configuration
{
	int info_panel_visible;
	uint32_t buffer_interval_ms;
	char *yunit_symbol;
	float yunit_per_sample;
	int load_afe_plugin;
	int default_width;
	int default_height;
};

class Application: public Configuration_interface, private application_configuration
{
public:
	Application(void);
	virtual ~Application();
	void release_configuration(void);
	void set_configuration_defaults(void);
	// Configuration_interface
	int begin_configuration(void);
	int set_configuration_setting(const char *identifier, const char *data);
	int end_configuration(void);
	int do_menu_loop(int argc, char **argv);

private:
	enum trigger_mode
	{
		trigger_auto = 0,
		trigger_threshold_repetitive,
		trigger_threshold_single,
		trigger_timed,
		trigger_freq_threshold_repetitive,
		trigger_freq_threshold_single,
	};

	static const char *dragging_mode_id[];

	enum statistics_mode
	{
		statistics_none = 0, statistics_adc_buffer, statistics_osc_buffer,
	};
	static const char *statistics_mode_text[];

	enum save_to_file_options
	{
		save_to_file_samples = 0, save_to_file_png, save_to_file_svg
	};

	gint show_dialog(GtkMessageType type, GtkButtonsType buttons, const gchar *title,
			const gchar *message_format, ...);DECLARE_BUTTON_AND_MENU_REDIR(on_quit, on_menu_quit, quit)
	DECLARE_BASIC_WIDGET_REDIR(on_main_window_destroy, main_window_release)
	DECLARE_BUTTON_AND_MENU_REDIR(on_preferences_click, on_menu_preferences, show_settings)
	DECLARE_BUTTON_AND_MENU_REDIR(on_plot_widget_settings_click, on_menu_plot_widget_settings,
			show_plot_widget_settings)
	DECLARE_BUTTON_AND_MENU_REDIR(on_about_click, on_menu_about, show_about)

	DECLARE_BASIC_MENU_REDIR(on_menu_auto_scale_y, auto_scale_y)
	DECLARE_BASIC_MENU_REDIR(on_menu_auto_scale_x, auto_scale_x)
	DECLARE_BASIC_MENU_REDIR(on_menu_auto_offset_y, auto_offset_y)
	static void on_menu_set_dragging(GSimpleAction *action, GVariant *parameter, gpointer data)
	{
		g_action_change_state(G_ACTION(action), parameter);
	}
	static void on_menu_set_dragging_change_state(GSimpleAction *action, GVariant *value,
			gpointer data)
	{
		((Application *) data)->osc_widget->set_dragging(
				(enum Plot_widget::dragging_mode) g_variant_get_int32(value));
		g_simple_action_set_state(action, value);
	}
	void plugin_afe_set_gain_rx_pga1(enum afe_gain_rx_pga1_enum gain_rx_pga1);
	void plugin_afe_set_gain_rx_pga2(enum afe_gain_rx_pga2_enum gain_rx_pga2);
	void plugin_afe_set_filtering(enum afe_filtering_enum filtering);
	static void on_plugin_afe_menu_set_pga1(GtkMenuItem *menuitem, gpointer data);
	static void on_plugin_afe_menu_set_pga2(GtkMenuItem *menuitem, gpointer data);
	static void on_plugin_afe_menu_set_filtering(GtkMenuItem *menuitem, gpointer data);
	static void on_menu_set_statistics(GSimpleAction *action, GVariant *parameter, gpointer data)
	{
		g_action_change_state(G_ACTION(action), parameter);
	}
	static void on_menu_set_statistics_change_state(GSimpleAction *action, GVariant *value,
			gpointer data)
	{
		((Application *) data)->statistics_mode = (enum statistics_mode) g_variant_get_int32(value);
		g_simple_action_set_state(action, value);
	}
	static void on_menu_select_script(GSimpleAction *action, GVariant *parameter, gpointer data)
	{
		// TODO: Implement
		g_warning("Select script: Not implemented");
		g_warning(g_variant_print(parameter, 1));
	}

	void add_toolbar_button(GtkWidget *toolbar, const char *icon_id, const char *icon_text,
			GCallback on_click);
	void add_toolbar_button_action(GtkWidget *toolbar, const char *icon_id, const char *icon_text,
			const char *action_name);
	void add_toolbar_toggle_button(GtkWidget *toolbar, const char *icon_id, const char *icon_text,
			GCallback on_toggle);
	void add_toolbar_toggle_button_action(GtkWidget *toolbar, const char *icon_id,
			const char *icon_text, const char *action_name);
	void add_toolbar_menu_button(GtkWidget *toolbar, const char *icon_id, const char *icon_text,
			const char *menu_id, GCallback on_click);
	void add_toolbar_menu_button_action(GtkWidget *toolbar, const char *icon_id,
			const char *icon_text, const char *menu_id, const char *action_name,
			int button_action_index);
	GtkWidget *create_toolbar_box(void);

	DECLARE_BASIC_WIDGET_REDIR(on_dialog_trigger_mode_changed, dialog_trigger_mode_changed)
	DECLARE_BASIC_WIDGET_REDIR(on_dialog_show, dialog_show)
	DECLARE_BASIC_WIDGET_REDIR(on_dialog_destroy, dialog_destroy)
	DECLARE_BASIC_WIDGET_REDIR(on_dialog_apply, dialog_apply)
	static void on_dialog_ok(GtkWidget *widget, gpointer data);

	void settings_to_dialog(void);
	void dialog_to_settings(void);
	void settings_to_dialog_format(const char *widget_id, const char *format, ...);
	float dialog_to_settings_float(const char *widget_id);

	void update_recording_actions(void);DECLARE_BASIC_MENU_REDIR(on_menu_start, start)
	DECLARE_BASIC_MENU_REDIR(on_menu_stop, stop)
	DECLARE_BASIC_MENU_REDIR(on_menu_save_samples, save_samples)
	DECLARE_BASIC_MENU_REDIR(on_menu_save_png, save_png)
	DECLARE_BASIC_MENU_REDIR(on_menu_save_svg, save_svg)
	void save_to_file(enum save_to_file_options option);
	void get_last_configuration_name(void);
	static void on_menu_load_configuration(GSimpleAction *action, GVariant *parameter,
			gpointer data)
	{
		((Application *) data)->load_configuration(g_variant_get_string(parameter, NULL));
	}
	DECLARE_BASIC_MENU_REDIR(on_menu_reload_configuration, reload_configuration)
	void load_configuration(const char *filename);
	static void on_menu_toggle_grid(GSimpleAction *action, GVariant *parameter, gpointer data)
	{
		assert(parameter == NULL);
		int new_grid = !((Application *) data)->osc_widget->get_grid();
		g_action_change_state(G_ACTION(action), g_variant_new_boolean(new_grid));
	}
	static void on_menu_toggle_grid_change_state(GSimpleAction *action, GVariant *value,
			gpointer data)
	{
		((Application *) data)->osc_widget->set_grid(g_variant_get_boolean(value));
		g_simple_action_set_state(action, value);
	}
	static void on_menu_toggle_samples(GSimpleAction *action, GVariant *parameter, gpointer data)
	{
		int new_plot_samples = !((Application *) data)->osc_widget->get_plot_samples();
		((Application *) data)->osc_widget->set_plot_samples(new_plot_samples);
		g_action_change_state(G_ACTION(action), g_variant_new_boolean(new_plot_samples));
	}
	static void on_menu_toggle_fft(GSimpleAction *action, GVariant *parameter, gpointer data)
	{
		int new_fft = !((Application *) data)->osc_widget->get_plot_fft();
		((Application *) data)->osc_widget->set_plot_fft(new_fft);
		g_action_change_state(G_ACTION(action), g_variant_new_boolean(new_fft));
	}
	static void on_menu_toggle_live_info_panel(GSimpleAction *action, GVariant *parameter,
			gpointer data)
	{
		int new_info_panel = !((Application *) data)->info_panel_visible;
		((Application *) data)->show_info_panel(new_info_panel);
		g_action_change_state(G_ACTION(action), g_variant_new_boolean(new_info_panel));

	}
	static void on_menu_toggle_static_grid(GSimpleAction *action, GVariant *parameter,
			gpointer data)
	{
		int new_static_grid = !((Application *) data)->osc_widget->get_static_grid();
		((Application *) data)->osc_widget->set_static_grid(new_static_grid);
		g_action_change_state(G_ACTION(action), g_variant_new_boolean(new_static_grid));

	}
	static gboolean on_main_window_popup_menu(GtkWidget *widget, gpointer data)
	{
		((Application *) data)->show_about();
		return TRUE;
	}
	static void on_set_drawing_mode(GtkMenuItem *menuitem, gpointer data);
	static void on_set_ybase_to_center(GSimpleAction *action, GVariant* parameter,
			gpointer user_data);
	static void on_set_xybase_to_cursor(GSimpleAction *action, GVariant* parameter,
			gpointer user_data);
	static void on_test_item(GSimpleAction *action, GVariant* parameter, gpointer user_data);
	static GActionEntry action_entries[];
	GtkWidget *create_popup_menu_darea(void);
	GtkWidget *add_submenu_entries_enum(const char *submenu_label, const char *enum_text[],
			uint32_t enum_count, GCallback activate_callback);
	static void on_app_activate(GtkApplication* app, gpointer data)
	{
		Application *application = (class Application*) data;
		assert(app == application->app);
		application->activate();
	}
	void activate(void);
	void initialize_main_window(void);
	void show_info_panel(int visible);

	void set_trigger_threshold_yunit(float value);
	static void *thread_recorder_arg(void *arg)
	{
		return ((Application *) arg)->thread_recorder();
	}
	void *thread_recorder(void);
	GtkWidget* menu_new_from_model(GMenuModel *model);

	static GActionEntry app_entries[];

	static const struct plc_setting_definition accepted_settings[];
	GtkBuilder *builder;
	GtkApplication *app;
	GtkWindow *main_window;
	GtkWidget *main_window_grid;
	GtkWidget *popup_menu_darea;
	GObject *preferences_dialog;
	GtkLabel *label_info;
	int recording_started;
	pthread_t recorder_thread;
	Recorder_interface *recorder;
	Plugin_afe *plugin_afe;
	volatile int end_thread_recorder;
	Plot_widget *osc_widget;
	trigger_mode trigger_mode;
	statistics_mode statistics_mode;
	int32_t trigger_interval_us;
	float trigger_threshold_yunit;
	float trigger_freq_threshold;
	float trigger_freq_hz;
	sample_rx_t trigger_threshold_samples;
	char *active_configuration_filename;
};

#endif /* APPLICATION_H */

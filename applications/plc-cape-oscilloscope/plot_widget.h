/**
 * @file
 * @brief	Plot widget for interactive graphics
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef PLOT_WIDGET_H
#define PLOT_WIDGET_H

#include "plot_area.h"		// graph_drawing_mode
#include "configuration_interface.h"

struct plot_widget_configuration
{
	int exported_image_width;
	int exported_image_height;
};

class Plot_area;

class Plot_widget : public Configuration_interface, private plot_widget_configuration
{
public:
	enum dragging_mode
	{
		dragging_free_cursor = 0,
		dragging_sticky_cursor,
		dragging_scroll_x,
		dragging_scroll_y,
		dragging_scale_x,
		dragging_scale_y,
		dragging_scale_to_area,
	};

	Plot_widget(GActionMap *action_map, GtkBuilder *builder);
	virtual ~Plot_widget();
	void release_configuration(void);
	void set_configuration_defaults(void);
	// Configuration_interface
	int begin_configuration(void);
	int set_configuration_setting(const char *identifier, const char *data);
	int end_configuration(void);
	Configuration_interface *get_plot_area_configuration_interface(void)
	{
		return area;
	}
	void set_popup_menu(GtkWidget *menu)
	{
		popup_menu_darea = menu;
	}
	void show_info_widget(int visible);
	// TODO: Remove direct access to gtk_widget
	GtkWidget *get_graph_widget(void)
	{
		return darea;
	}
	GtkWidget *get_info_widget(void)
	{
		return GTK_WIDGET(info_box);
	}
	void show_settings(GtkWindow *parent_window);
	void set_yunit_symbol(const char* symbol);
	void set_yunit_per_sample(float yunit_per_sample);
	float get_yunit_per_sample(void);
	void set_dragging(enum dragging_mode mode);
	int push_buffer(uint16_t *samples, uint32_t samples_count);
	int save_samples(const char *filename, int in_digital_units);
	int save_png(const char *filename);
	int save_svg(const char *filename);
	float get_osc_buffer_interval(void);
	void set_osc_buffer(float interval_ms, float us_per_sample);
	void auto_scale_x(void);
	void auto_scale_y(void);
	void auto_offset_y(void);
	int get_grid(void);
	void set_grid(int activate);
	int get_static_grid(void);
	void set_static_grid(int enable);
	int get_plot_samples(void);
	void set_plot_samples(int enable);
	int get_plot_fft(void);
	void set_plot_fft(int enable);
	enum graph_drawing_mode get_graph_drawing_mode(void);
	void set_graph_drawing_mode(enum graph_drawing_mode mode);
	void set_ybase_to_center(void);
	void set_xybase_to_cursor(void);

private:
	// Dialog
	static void on_dialog_show(GtkWidget *widget, gpointer data);
	static void on_dialog_destroy(GtkWidget *widget, gpointer data);
	static void on_dialog_apply(GtkWidget *widget, gpointer data);
	static void on_dialog_ok(GtkWidget *widget, gpointer data);
	void settings_to_dialog(void);
	void dialog_to_settings(void);
	void settings_to_dialog_format(const char *widget_id, const char *format, ...);
	float dialog_to_settings_float(const char *widget_id);
	const char *dialog_to_settings_string(const char *widget_id);
	void update_viewer_settings_label(void);
	void update_grid_settings_label(void);
	void update_cursor_settings_label(void);

	// Mouse
	static void on_darea_size_allocate(GtkWidget *widget, GdkRectangle *allocation, gpointer data);
	static gboolean on_darea_button_press(GtkWidget *widget, GdkEvent *event, gpointer data);
	void darea_button_press(GdkEventButton *event_button);
	static gboolean on_darea_button_release(GtkWidget *widget, GdkEvent *event, gpointer data);
	void darea_button_release(GdkEventButton *event_button);
	static gboolean on_darea_motion_notify(GtkWidget *widget, GdkEvent *event, gpointer data);
	void darea_motion_notify(GdkEventMotion *event_motion);
	void hide_cursors(void);
	void set_cursor_ini(float x, float y);
	void set_cursor_end(float x, float y);

	static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data);
	void notify_plot_settings_updated(void);
	void notify_cursors_updated(void);
	void refresh(void);

	static const struct plc_setting_definition accepted_settings[];
	Plot_area *area;
	GtkWidget *darea;
	int info_box_visible;
	GtkBox *info_box;
	GtkLabel *viewer_settings_label;
	GtkLabel *grid_settings_label;
	GtkLabel *cursor_settings_label;
	gulong draw_handler_id;
	GActionMap *action_map;
	GtkBuilder *builder;
	GtkWidget *popup_menu_darea;
	GObject *settings_dialog;
	dragging_mode dragging_mode;
	int darea_width, darea_height;
	gdouble cursor_ini_x, cursor_ini_y;
	gdouble drag_prev_x, drag_prev_y;
	int darea_button_pressed;
	int track_diff_cursor;
};

#endif /* PLOT_WIDGET_H */

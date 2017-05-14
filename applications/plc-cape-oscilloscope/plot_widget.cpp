/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include <math.h>		// fabs
#include "+common/api/+base.h"
#include "configuration.h"
#include "plot_widget.h"
#include "tools.h"

#define MIN_CURSOR_DIFF 20.0

#define OFFSET(member) offsetof(struct plot_widget_configuration, member)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"

const struct plc_setting_definition Plot_widget::accepted_settings[] = {
	{
		"exported_image_width", plc_setting_u32, "Exported image width", {
			NULL }, 0, NULL, OFFSET(exported_image_width) }, {
		"exported_image_height", plc_setting_u32, "Exported image height", {
			NULL }, 0, NULL, OFFSET(exported_image_height) } };

#pragma GCC diagnostic pop

#undef OFFSET

// TODO: Refactor removing references to 'action_amp' and 'builder'
Plot_widget::Plot_widget(GActionMap *action_map, GtkBuilder *builder)
{
	area = new Plot_area();
	this->action_map = action_map;
	this->builder = builder;
	settings_dialog = NULL;
	popup_menu_darea = NULL;
	// Create a main_area for the oscilloscope
	darea = gtk_drawing_area_new();
	viewer_settings_label = NULL;
	cursor_settings_label = NULL;
	// The 'drawing_area' is fixed size by default -> Make it dynamic
	gtk_widget_set_hexpand(darea, TRUE);
	gtk_widget_set_vexpand(darea, TRUE);
	g_signal_connect(G_OBJECT(darea), "size-allocate", G_CALLBACK(on_darea_size_allocate), this);
	gtk_widget_set_events(darea,
			GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
	g_signal_connect(G_OBJECT(darea), "button-press-event", G_CALLBACK(on_darea_button_press),
			this);
	g_signal_connect(G_OBJECT(darea), "button-release-event", G_CALLBACK(on_darea_button_release),
			this);
	g_signal_connect(G_OBJECT(darea), "motion-notify-event", G_CALLBACK(on_darea_motion_notify),
			this);
	draw_handler_id = g_signal_connect(G_OBJECT(darea), "draw", G_CALLBACK(on_draw_event), this);
	// Info panel
	info_box_visible = FALSE;
	info_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 10));
	// Set the preferred (minimum) width of the box
	// If configured to be expandable it can automatically expand if some child requests to
	gtk_widget_set_size_request(GTK_WIDGET(info_box), 150, -1);
	// Set margins for children
	gtk_widget_set_margin_left(GTK_WIDGET(info_box), 3);
	gtk_widget_set_margin_right(GTK_WIDGET(info_box), 3);
	// To uniformly distribute children within the parent (vertical centering) use this:
	// 	gtk_box_set_homogeneous(info_box, TRUE);
	viewer_settings_label = GTK_LABEL(gtk_label_new(NULL));
	// By default the label widget is configured to be centered within parent (GTK_ALIGN_CENTER)
	gtk_widget_set_halign(GTK_WIDGET(viewer_settings_label), GTK_ALIGN_START);
	grid_settings_label = GTK_LABEL(gtk_label_new(NULL));
	gtk_widget_set_halign(GTK_WIDGET(grid_settings_label), GTK_ALIGN_START);
	cursor_settings_label = GTK_LABEL(gtk_label_new(NULL));
	gtk_widget_set_halign(GTK_WIDGET(cursor_settings_label), GTK_ALIGN_START);
	gtk_box_pack_start(info_box, GTK_WIDGET(viewer_settings_label), FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(info_box), GTK_WIDGET(grid_settings_label));
	gtk_container_add(GTK_CONTAINER(info_box), GTK_WIDGET(cursor_settings_label));
	set_configuration_defaults();
}

Plot_widget::~Plot_widget()
{
	// Info panel
	gtk_widget_destroy(GTK_WIDGET(cursor_settings_label));
	gtk_widget_destroy(GTK_WIDGET(grid_settings_label));
	gtk_widget_destroy(GTK_WIDGET(viewer_settings_label));
	gtk_widget_destroy(GTK_WIDGET(info_box));
	// Drawing area
	g_signal_handler_disconnect(G_OBJECT(darea), draw_handler_id);
	delete area;
}

void Plot_widget::release_configuration(void)
{
}

void Plot_widget::set_configuration_defaults(void)
{
	memset((plot_widget_configuration*) this, 0, sizeof(struct plot_widget_configuration));
}

int Plot_widget::begin_configuration(void)
{
	release_configuration();
	set_configuration_defaults();
	return 0;
}

int Plot_widget::set_configuration_setting(const char *identifier, const char *data)
{
	const struct plc_setting_definition *setting_definition = plc_setting_find_definition(
			accepted_settings, ARRAY_SIZE(accepted_settings), identifier);
	if (setting_definition != NULL)
	{
		union plc_setting_data *dst_data =
				(union plc_setting_data *) (((char *) (plot_widget_configuration*) this)
						+ setting_definition->user_data);
		plc_setting_data_release(setting_definition->type, dst_data);
		plc_setting_text_to_data(data, setting_definition->type, dst_data);
	}
	else if (strcmp(identifier, "background_color") == 0)
	{
		GdkRGBA color;
		gboolean ret = gdk_rgba_parse(&color, data);
		if (ret == TRUE)
			gtk_widget_override_background_color(darea, GTK_STATE_FLAG_NORMAL, &color);
	}
	else
	{
		return -1;
	}
	return 0;
}

int Plot_widget::end_configuration(void)
{
	refresh();
	notify_plot_settings_updated();
	return 0;
}

void Plot_widget::show_info_widget(int visible)
{
	info_box_visible = visible;
	if (visible)
	{
		if (area->get_grid())
			gtk_widget_show(GTK_WIDGET(grid_settings_label));
		else
			gtk_widget_hide(GTK_WIDGET(grid_settings_label));
		if (area->get_cursors_visible())
			gtk_widget_show(GTK_WIDGET(cursor_settings_label));
		else
			gtk_widget_hide(GTK_WIDGET(cursor_settings_label));
		gtk_widget_show(GTK_WIDGET(info_box));
		// Force a refreshment of all the data in the info panel
		notify_plot_settings_updated();
		notify_cursors_updated();
	}
	else
	{
		gtk_widget_hide(GTK_WIDGET(info_box));
	}
	area->show_cursors_text(!visible);
	area->show_viewer_info(!visible);
}

void Plot_widget::show_settings(GtkWindow *parent_window)
{
	if (settings_dialog == NULL)
	{
		// TODO: Refactor. Encapsulate in a class
		guint ok = gtk_builder_add_from_file(builder, "plot_widget_settings.ui", NULL);
		assert(ok);
		gint x, y;
		gint width, height;
		gtk_window_get_position(parent_window, &x, &y);
		gtk_window_get_size(parent_window, &width, &height);
		settings_dialog = gtk_builder_get_object(builder, "plot_widget_settings_dialog");
		// By default move preferences window to 'main_window' right border and some margin
		gtk_window_move(GTK_WINDOW(settings_dialog), x + width + 5, y);
		g_signal_connect(settings_dialog, "show", G_CALLBACK(on_dialog_show), this);
		g_signal_connect(settings_dialog, "destroy", G_CALLBACK(on_dialog_destroy), this);
		GObject *apply_button = gtk_builder_get_object(builder, "applyButton");
		g_signal_connect(apply_button, "clicked", G_CALLBACK(on_dialog_apply), this);
		GtkWidget *ok_button = GTK_WIDGET(gtk_builder_get_object(builder, "okButton"));
		g_signal_connect(ok_button, "clicked", G_CALLBACK(on_dialog_ok), this);
		gtk_widget_set_can_default(ok_button, TRUE);
		gtk_window_set_default(GTK_WINDOW(settings_dialog), ok_button);
		g_signal_connect(settings_dialog, "activate-default", G_CALLBACK(on_dialog_ok), this);
		gtk_widget_show_all(GTK_WIDGET(settings_dialog));
	}
	else
	{
		gtk_window_present(GTK_WINDOW(settings_dialog));
	}
}

void Plot_widget::set_yunit_symbol(const char* symbol)
{
	area->set_yunit_symbol(symbol);
	refresh();
}

void Plot_widget::set_yunit_per_sample(float yunit_per_sample)
{
	area->set_yunit_per_sample(yunit_per_sample);
	refresh();
}

float Plot_widget::get_yunit_per_sample(void)
{
	return area->get_yunit_per_sample();
}

void Plot_widget::set_dragging(enum dragging_mode mode)
{
	dragging_mode = mode;
	area->set_sticky_cursor(dragging_mode == dragging_sticky_cursor);
	if ((mode == dragging_free_cursor) || (mode == dragging_sticky_cursor))
		hide_cursors();
}

int Plot_widget::push_buffer(uint16_t *samples, uint32_t samples_count)
{
	int ret = area->push_buffer(samples, samples_count);
	if (ret)
		refresh();
	return ret;
}

int Plot_widget::save_samples(const char *filename, int in_digital_units)
{
	return area->save_samples(filename, in_digital_units);
}

int Plot_widget::save_png(const char *filename)
{
	int width = (exported_image_width > 0) ? exported_image_width : darea_width;
	int height = (exported_image_height > 0) ? exported_image_height : darea_height;
	return area->save_png(filename, width, height);
}

int Plot_widget::save_svg(const char *filename)
{
	int width = (exported_image_width > 0) ? exported_image_width : darea_width;
	int height = (exported_image_height > 0) ? exported_image_height : darea_height;
	return area->save_svg(filename, width, height);
}

float Plot_widget::get_osc_buffer_interval(void)
{
	return area->get_osc_buffer_interval();
}

void Plot_widget::set_osc_buffer(float interval_ms, float us_per_sample)
{
	if (area->set_osc_buffer(interval_ms, us_per_sample))
		refresh();
}

void Plot_widget::auto_scale_x(void)
{
	area->auto_scale_x();
	refresh();
	notify_plot_settings_updated();
}

void Plot_widget::auto_scale_y(void)
{
	area->auto_scale_y();
	refresh();
	notify_plot_settings_updated();
}

void Plot_widget::auto_offset_y(void)
{
	area->auto_offset_y();
	refresh();
	notify_plot_settings_updated();
}

int Plot_widget::get_grid(void)
{
	return area->get_grid();
}

void Plot_widget::set_grid(int activate)
{
	area->set_grid(activate);
	refresh();
	notify_plot_settings_updated();
	if (info_box_visible)
	{
		if (activate)
			gtk_widget_show(GTK_WIDGET(grid_settings_label));
		else
			gtk_widget_hide(GTK_WIDGET(grid_settings_label));
	}
}

int Plot_widget::get_static_grid(void)
{
	return area->get_static_grid();
}

void Plot_widget::set_static_grid(int enable)
{
	area->set_static_grid(enable);
	refresh();
	notify_plot_settings_updated();
}

int Plot_widget::get_plot_samples(void)
{
	return area->get_plot_samples();
}

void Plot_widget::set_plot_samples(int enable)
{
	area->set_plot_samples(enable);
	refresh();
}

int Plot_widget::get_plot_fft(void)
{
	return area->get_plot_fft();
}

void Plot_widget::set_plot_fft(int enable)
{
	area->set_plot_fft(enable);
	refresh();
}

enum graph_drawing_mode Plot_widget::get_graph_drawing_mode(void)
{
	return area->get_graph_drawing_mode();
}

void Plot_widget::set_graph_drawing_mode(enum graph_drawing_mode mode)
{
	area->set_graph_drawing_mode(mode);
	refresh();
}

void Plot_widget::set_ybase_to_center(void)
{
	area->set_yunit_base(area->get_yunit_offset());
	refresh();
}

void Plot_widget::set_xybase_to_cursor(void)
{
	area->set_xybase_to_cursor();
	refresh();
}

void Plot_widget::on_dialog_show(GtkWidget *widget, gpointer data)
{
	((Plot_widget *) data)->settings_to_dialog();
}

void Plot_widget::on_dialog_destroy(GtkWidget *widget, gpointer data)
{
	((Plot_widget *) data)->settings_dialog = NULL;
}

void Plot_widget::on_dialog_apply(GtkWidget *widget, gpointer data)
{
	((Plot_widget *) data)->dialog_to_settings();
}

void Plot_widget::on_dialog_ok(GtkWidget *widget, gpointer data)
{
	on_dialog_apply(widget, data);
	GtkWidget *toplevel = gtk_widget_get_toplevel(widget);
	gtk_widget_destroy(toplevel);
}

void Plot_widget::settings_to_dialog(void)
{
	settings_to_dialog_format("gridYUnitEntry", "%.2f", area->get_grid_yunit());
	settings_to_dialog_format("gridTimeEntry", "%.2f", area->get_grid_time());
	settings_to_dialog_format("yUnitRangeEntry", "%.2f", area->get_yunit_range());
	settings_to_dialog_format("yUnitOffsetEntry", "%.2f", area->get_yunit_offset());
	settings_to_dialog_format("viewerIntervalEntry", "%.2f", area->get_viewer_interval_ms());
	settings_to_dialog_format("fftYRangeNormalizedEntry", "%.2f", area->get_fft_yrange_user());
	settings_to_dialog_format("graphTitleEntry", "%s", area->get_graph_title());
}

void Plot_widget::dialog_to_settings(void)
{
	area->set_grid_yunit(dialog_to_settings_float("gridYUnitEntry"));
	area->set_grid_time(dialog_to_settings_float("gridTimeEntry"));
	area->set_yunit_range(dialog_to_settings_float("yUnitRangeEntry"));
	area->set_yunit_offset(dialog_to_settings_float("yUnitOffsetEntry"));
	area->set_viewer_interval(dialog_to_settings_float("viewerIntervalEntry"));
	area->set_fft_yrange_user(dialog_to_settings_float("fftYRangeNormalizedEntry"));
	area->set_graph_title(dialog_to_settings_string("graphTitleEntry"));
	notify_plot_settings_updated();
	refresh();
}

void Plot_widget::settings_to_dialog_format(const char *widget_id, const char *format, ...)
{
	va_list args;
	char text_buffer[32];
	GObject *widget = gtk_builder_get_object(builder, widget_id);
	va_start(args, format);
	vsprintf(text_buffer, format, args);
	va_end(args);
	gtk_entry_set_text(GTK_ENTRY(widget), text_buffer);
}

float Plot_widget::dialog_to_settings_float(const char *widget_id)
{
	// TODO: Improve error control to detect wrong or incoherent '*.ui' files
	GObject *widget = gtk_builder_get_object(builder, widget_id);
	assert(widget != NULL);
	return strtof(gtk_entry_get_text(GTK_ENTRY(widget)), NULL);
}

const char *Plot_widget::dialog_to_settings_string(const char *widget_id)
{
	GObject *widget = gtk_builder_get_object(builder, widget_id);
	assert(widget != NULL);
	return gtk_entry_get_text(GTK_ENTRY(widget));
}

void Plot_widget::update_viewer_settings_label(void)
{
	assert(viewer_settings_label);
	char text[128];
	char *text_cur = text;
	text_cur += sprintf(text, "<b>Viewer:</b>\nX Range: ");
	text_cur += sprint_custom_float(text_cur, area->get_viewer_interval_ms() / 1000.0, "s");
	text_cur += sprintf(text_cur, "\nX Samples: %u", (uint32_t) round(area->get_viewer_samples()));
	text_cur += sprintf(text_cur, "\nX Offset: ");
	text_cur += sprint_custom_float(text_cur, area->get_viewer_left_us() / 1000000.0, "s");
	text_cur += sprintf(text_cur, "\nY Range: ");
	text_cur += sprint_custom_float(text_cur, area->get_yunit_range(), area->get_yunit_symbol());
	text_cur += sprintf(text_cur, "\nY Center: ");
	text_cur += sprint_custom_float(text_cur, area->get_yunit_offset(), area->get_yunit_symbol());
	gtk_label_set_text(viewer_settings_label, text);
	gtk_label_set_use_markup(viewer_settings_label, TRUE);
}

void Plot_widget::update_grid_settings_label(void)
{
	assert(grid_settings_label);
	char text[128];
	char *text_cur = text;
	text_cur += sprintf(text, "<b>Grid:</b>\nY: ");
	text_cur += sprint_custom_float(text_cur, area->get_grid_yunit(), area->get_yunit_symbol());
	text_cur += sprintf(text_cur, "/div\nX: ");
	text_cur += sprint_custom_float(text_cur, area->get_grid_time() / 1000000.0, "s");
	text_cur += sprintf(text_cur, "/div");
	assert((size_t)(text_cur - text) < sizeof(text));
	gtk_label_set_text(grid_settings_label, text);
	gtk_label_set_use_markup(grid_settings_label, TRUE);
}

void Plot_widget::update_cursor_settings_label(void)
{
	assert(cursor_settings_label);
	char text[128];
	char *text_cur = text;
	char *text_end = text + sizeof(text);
	text_cur += sprintf(text, "<b>Cursors:</b>\nC1: ");
	text_cur += area->sprint_cursor_ini(text_cur, text_end - text_cur);
	text_cur += sprintf(text_cur, "\nC2: ");
	text_cur += area->sprint_cursor_end(text_cur, text_end - text_cur);
	if (!area->get_plot_fft())
	{
		text_cur += sprintf(text_cur, "\nDiff: ");
		text_cur += area->sprint_cursor_diff(text_cur, text_end - text_cur);
		text_cur += sprintf(text_cur, "\nDiff freq: ");
		text_cur += area->sprint_cursor_diff_freq(text_cur, text_end - text_cur);
	}
	assert(text_cur < text_end);
	gtk_label_set_text(cursor_settings_label, text);
	gtk_label_set_use_markup(cursor_settings_label, TRUE);
}

void Plot_widget::on_darea_size_allocate(GtkWidget *widget, GdkRectangle *allocation, gpointer data)
{
	((Plot_widget *) data)->darea_width = allocation->width;
	((Plot_widget *) data)->darea_height = allocation->height;
	// When hooking the "size-allocate" signal we need to explicitly call the
	// 'gtk_widget_set_allocation' function to update widget dimensions
	gtk_widget_set_allocation(widget, allocation);
}

gboolean Plot_widget::on_darea_button_press(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	if (event->type == GDK_BUTTON_PRESS)
	{
		((Plot_widget *) data)->darea_button_press((GdkEventButton *) event);
		return TRUE;
	}
	return FALSE;
}

void Plot_widget::darea_button_press(GdkEventButton *event_button)
{
	switch (event_button->button)
	{
	case 1:
		// Example of checking for the SHIFT button
		//	is_shift_key_pressed = event_button->state & GDK_SHIFT_MASK;
		darea_button_pressed = 1;
		switch (dragging_mode)
		{
		case dragging_scroll_x:
		case dragging_scale_x:
			drag_prev_x = event_button->x;
			break;
		case dragging_scroll_y:
		case dragging_scale_y:
			drag_prev_y = event_button->y;
			break;
		case dragging_free_cursor:
		case dragging_sticky_cursor:
		case dragging_scale_to_area:
			set_cursor_ini(event_button->x, event_button->y);
			track_diff_cursor = 0;
			break;
		default:
			break;
		}
		break;
	case 2:
		break;
	case 3:
		if (popup_menu_darea)
			gtk_menu_popup(GTK_MENU(popup_menu_darea), NULL, NULL, NULL, NULL,
					(event_button != NULL) ? event_button->button : 0,
					gdk_event_get_time((GdkEvent*) event_button));
		break;
	}
}

gboolean Plot_widget::on_darea_button_release(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	if (event->type == GDK_BUTTON_RELEASE)
	{
		((Plot_widget *) data)->darea_button_release((GdkEventButton *) event);
		return TRUE;
	}
	return FALSE;
}

void Plot_widget::darea_button_release(GdkEventButton *event_button)
{
	darea_button_pressed = 0;
	if (dragging_mode == dragging_scale_to_area)
	{
		area->zoom_viewer_to_cursor_area();
		hide_cursors();
		GAction *action = g_action_map_lookup_action(action_map, "set-dragging");
		assert(action);
		g_action_change_state(action, g_variant_new_int32(dragging_free_cursor));
	}
}

gboolean Plot_widget::on_darea_motion_notify(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	// Improvement:
	// For optimization purposes we should capture MOTION events only when necessary
	// We could for example activate the motion events only while on 'click' with:
	//	'gtk_widget_add_events(widget, GDK_POINTER_MOTION_MASK)'
	// Unfortunately it seems there is no function to remove an activated event as
	// 'gtk_widget_set_events' cannot be called with a realized widget
	// So, for the moment capture all the MOTION events
	if (event->type == GDK_MOTION_NOTIFY)
		((Plot_widget *) data)->darea_motion_notify((GdkEventMotion*) event);
	return TRUE;
}

void Plot_widget::darea_motion_notify(GdkEventMotion *event_motion)
{
	if (darea_button_pressed == 1)
	{
		switch (dragging_mode)
		{
		case dragging_scroll_x:
			area->move_viewer_x((drag_prev_x - event_motion->x) / darea_width);
			drag_prev_x = event_motion->x;
			refresh();
			break;
		case dragging_scroll_y:
			area->move_viewer_y((drag_prev_y - event_motion->y) / darea_height);
			drag_prev_y = event_motion->y;
			refresh();
			break;
		case dragging_scale_x:
			area->scale_viewer_x((drag_prev_x - event_motion->x) / darea_width);
			drag_prev_x = event_motion->x;
			refresh();
			break;
		case dragging_scale_y:
			area->scale_viewer_y((drag_prev_y - event_motion->y) / darea_height);
			drag_prev_y = event_motion->y;
			refresh();
			break;
		case dragging_free_cursor:
		case dragging_sticky_cursor:
		case dragging_scale_to_area:
			if (track_diff_cursor)
				set_cursor_end(event_motion->x, event_motion->y);
			if ((fabs(cursor_ini_x - event_motion->x) > MIN_CURSOR_DIFF)
					|| (fabs(cursor_ini_y - event_motion->y) > MIN_CURSOR_DIFF))
				track_diff_cursor = 1;
			break;
		default:
			break;
		}
		notify_plot_settings_updated();
	}
	else if (track_diff_cursor == 0)
	{
		if (event_motion->state & GDK_CONTROL_MASK)
			set_cursor_ini(event_motion->x, event_motion->y);
	}
	else
	{
		// Tolerance mechanism to accept keeping CONTROL key pressed after a diff-cursor
		// Otherwise, mouse movements with the CONTROL pressed will reset the on-the-fly-cursor
		if ((event_motion->state & GDK_CONTROL_MASK) == 0)
			track_diff_cursor = 0;
	}
}

void Plot_widget::hide_cursors(void)
{
	area->reset_cursor();
	refresh();
	notify_plot_settings_updated();
	if (info_box_visible)
		gtk_widget_hide(GTK_WIDGET(cursor_settings_label));
}

void Plot_widget::set_cursor_ini(float x, float y)
{
	cursor_ini_x = x;
	cursor_ini_y = y;
	area->set_cursor_ini(x / darea_width, y / darea_height);
	refresh();
	notify_cursors_updated();
	if (info_box_visible)
		gtk_widget_show(GTK_WIDGET(cursor_settings_label));
}

void Plot_widget::set_cursor_end(float x, float y)
{
	area->set_cursor_end(x / darea_width, y / darea_height);
	refresh();
	notify_cursors_updated();
}

gboolean Plot_widget::on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
	GtkAllocation rect;
	gtk_widget_get_allocation(widget, &rect);
	return ((Plot_widget *) user_data)->area->do_drawing(cr, rect);
}

void Plot_widget::notify_plot_settings_updated(void)
{
	if (settings_dialog)
		settings_to_dialog();
	if (info_box_visible)
	{
		if (viewer_settings_label)
			update_viewer_settings_label();
		if (cursor_settings_label)
			update_cursor_settings_label();
		if (grid_settings_label)
			update_grid_settings_label();
	}
}

void Plot_widget::notify_cursors_updated(void)
{
	if (info_box_visible)
	{
		if (cursor_settings_label)
			update_cursor_settings_label();
	}
}

void Plot_widget::refresh(void)
{
	gtk_widget_queue_draw(darea);
}

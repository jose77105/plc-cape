/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include <cairo.h>
#include <cairo-svg.h>		// cairo_svg_surface_create
#include <glib.h>			// TRUE, FALSE
#include <math.h>
#include "+common/api/+base.h"
#include "configuration.h"
#include "libraries/libplc-tools/api/time.h"
#include "libraries/libplc-tools/api/file.h"
#include "plot_area.h"
#include "tools.h"

#define AUTO_SCALE_RANGE_MIN 20
#define DEFAULT_YUNIT_PER_SAMPLE 1.0
#define SAMPLES_DEF_RANGE (1 << 12)
#define DEFAULT_GRID_DIVISIONS_Y 10.0
#define DEFAULT_GRID_TIME_US 2000
#define DEFAULT_TIME_PER_SAMPLE_US 1.0
#define DEFAULT_OSC_BUFFER_INTERVAL_MS 20.0
#define DEFAULT_VIEWER_SAMPLES 512
#define STATIC_GRID_DIVISIONS_DEFAULT 10
#define GRAPH_TEXT_MARGIN 3.0
#define GRAPH_TEXT_INTERLINE 3.0

#define OFFSET(member) offsetof(struct plot_area_configuration, member)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"

const char *graph_drawing_mode_text[graph_drawing_COUNT] = {
	"Line", "Line-Fast-Decimation", "Bars", "Dots", "Crosses", "Circles", "Rectangles" };

const struct plc_setting_definition Plot_area::accepted_settings[] = {
	{
		"yunit_range", plc_setting_float, "Y-Unit range", {
			NULL }, 0, NULL, OFFSET(yunit_range) }, {
		"yunit_offset", plc_setting_float, "Y-Unit offset", {
			NULL }, 0, NULL, OFFSET(yunit_offset) }, {
		"grid", plc_setting_u32, "Show grid", {
			NULL }, 0, NULL, OFFSET(grid) }, {
		"static_grid", plc_setting_u32, "Static grid", {
			NULL }, 0, NULL, OFFSET(static_grid) }, {
		"static_grid_x_divisions", plc_setting_u8, "Static grid X divisions", {
			NULL }, 0, NULL, OFFSET(static_grid_x_divisions) }, {
		"static_grid_y_divisions", plc_setting_u8, "Static grid Y divisions", {
			NULL }, 0, NULL, OFFSET(static_grid_y_divisions) }, {
		"grid_yunit", plc_setting_float, "Grid y-unit", {
			NULL }, 0, NULL, OFFSET(grid_yunit) }, {
		"grid_time_us", plc_setting_float, "Grid time [us]", {
			NULL }, 0, NULL, OFFSET(grid_time_us) }, {
		"graph_drawing_mode", plc_setting_u32, "Graph drawing mode", {
			NULL }, 0, NULL, OFFSET(graph_drawing_mode) }, {
		"stroke_width", plc_setting_float, "Stroke width", {
			NULL }, 0, NULL, OFFSET(stroke_width) }, {
		"sample_mark_size", plc_setting_float, "Sample mark size", {
			NULL }, 0, NULL, OFFSET(sample_mark_size) }, {
		"font_face", plc_setting_string, "Font face", {
			NULL }, 0, NULL, OFFSET(font_face) }, {
		"font_bold", plc_setting_u32, "Font bold", {
			NULL }, 0, NULL, OFFSET(font_bold) }, {
		"font_size", plc_setting_float, "Font bold", {
			NULL }, 0, NULL, OFFSET(font_size) }, };

#pragma GCC diagnostic pop

#undef OFFSET

static const GdkRGBA samples_cursors_color_default = {
	.0, .0, .4, 1.0 };

static const GdkRGBA fft_cursors_color_default = {
	.0, .4, .0, 1.0 };

static const GdkRGBA grid_color_default = {
	.5, .5, .5, 1.0 };

static const GdkRGBA samples_color_default = {
	.0, .0, .3, 1.0 };

static const GdkRGBA fft_color_default = {
	.0, .3, .0, 1.0 };

// TODO: Make this as a configurable option
static const int viewer_info_behind_graph = 0;

struct graph_plot
{
	float x;
	float y;
	float y_base;
	float sample_mark_size;
	cairo_t *cr;
};

static void plot_sample_lines(struct graph_plot *graph_plot)
{
	cairo_line_to(graph_plot->cr, graph_plot->x, graph_plot->y);
}

static void plot_sample_bars(struct graph_plot *graph_plot)
{
	cairo_move_to(graph_plot->cr, graph_plot->x, graph_plot->y_base);
	cairo_line_to(graph_plot->cr, graph_plot->x, graph_plot->y);
}

static void plot_sample_dots(struct graph_plot *graph_plot)
{
	// To draw a single pixel just add 0.5
	// 	https://cairographics.org/FAQ/
	cairo_move_to(graph_plot->cr, graph_plot->x, graph_plot->y);
	cairo_line_to(graph_plot->cr, graph_plot->x, graph_plot->y + graph_plot->sample_mark_size);
}

static void plot_sample_crosses(struct graph_plot *graph_plot)
{
	cairo_move_to(graph_plot->cr, graph_plot->x - graph_plot->sample_mark_size, graph_plot->y);
	cairo_line_to(graph_plot->cr, graph_plot->x + graph_plot->sample_mark_size, graph_plot->y);
	cairo_move_to(graph_plot->cr, graph_plot->x, graph_plot->y - graph_plot->sample_mark_size);
	cairo_line_to(graph_plot->cr, graph_plot->x, graph_plot->y + graph_plot->sample_mark_size);
}

static void plot_sample_circles(struct graph_plot *graph_plot)
{
	// If there is a 'current point' (started with a 'cairo_move_to' call) 'cairo_arc' traces a
	// line from previous position. To avoid it, remove the current point with 'cairo_new_sub_path'
	cairo_new_sub_path(graph_plot->cr);
	cairo_arc(graph_plot->cr, graph_plot->x, graph_plot->y, graph_plot->sample_mark_size, .0,
			2 * M_PI);
}

static void plot_sample_rectangles(struct graph_plot *graph_plot)
{
	cairo_rectangle(graph_plot->cr, graph_plot->x - graph_plot->sample_mark_size / 2.0,
			graph_plot->y - graph_plot->sample_mark_size / 2.0, graph_plot->sample_mark_size,
			graph_plot->sample_mark_size);
}

Plot_area::Plot_area()
{
	valid_darea_data = 0;
	// Default values
	plot_samples = 1;
	plot_fft = 0;
	fft_in = fft_out = NULL;
	fft_graph = NULL;
	fft_graph_yrange_user = fft_graph_yrange = 0.0;
	yunit_symbol = NULL;
	show_cursors = 0;
	text_over_cursors = 0;
	show_cursor_diff = 0;
	cursor_mode = cursor_free;
	cursor_units = cursor_units_time_yunit;
	cursor_ini_x_ratio = cursor_ini_y_ratio = cursor_end_x_ratio = cursor_end_y_ratio = 0.0;
	graph_title = strdup("");
	set_configuration_defaults();
	init_buffers(DEFAULT_OSC_BUFFER_INTERVAL_MS);
}

Plot_area::~Plot_area()
{
	release_configuration();
	release_buffers();
	if (yunit_symbol)
		free(yunit_symbol);
	free(graph_title);
}

void Plot_area::release_configuration(void)
{
	if (font_face)
	{
		free(font_face);
		font_face = NULL;
	}
}

void Plot_area::set_configuration_defaults(void)
{
	memset((plot_area_configuration*) this, 0, sizeof(struct plot_area_configuration));
	yunit_per_sample = DEFAULT_YUNIT_PER_SAMPLE;
	yunit_range = yunit_per_sample * SAMPLES_DEF_RANGE;
	yunit_offset = yunit_range / 2.0;
	grid_yunit = yunit_range / DEFAULT_GRID_DIVISIONS_Y;
	grid_time_us = DEFAULT_GRID_TIME_US;
	time_per_sample_us = DEFAULT_TIME_PER_SAMPLE_US;
	graph_drawing_mode = graph_drawing_lines;
	sample_mark_size = 3.0;
	samples_color = samples_color_default;
	samples_cursors_color = samples_cursors_color_default;
	fft_color = fft_color_default;
	fft_cursors_color = fft_cursors_color_default;
	grid_color = grid_color_default;
}

int Plot_area::begin_configuration(void)
{
	release_configuration();
	set_configuration_defaults();
	return 0;
}

int Plot_area::set_configuration_setting(const char *identifier, const char *data)
{
	const struct plc_setting_definition *setting_definition = plc_setting_find_definition(
			accepted_settings, ARRAY_SIZE(accepted_settings), identifier);
	if (setting_definition != NULL)
	{
		union plc_setting_data *dst_data =
				(union plc_setting_data *) (((char *) (plot_area_configuration*) this)
						+ setting_definition->user_data);
		plc_setting_data_release(setting_definition->type, dst_data);
		plc_setting_text_to_data(data, setting_definition->type, dst_data);
		return 0;
	}
	else
	{
		GdkRGBA *color_setting = NULL;
		// Parse color settings
		if (strcmp(identifier, "samples_color") == 0)
			color_setting = &samples_color;
		else if (strcmp(identifier, "samples_cursors_color") == 0)
			color_setting = &samples_cursors_color;
		else if (strcmp(identifier, "fft_color") == 0)
			color_setting = &fft_color;
		else if (strcmp(identifier, "fft_cursors_color") == 0)
			color_setting = &fft_cursors_color;
		else if (strcmp(identifier, "grid_color") == 0)
			color_setting = &grid_color;
		if (color_setting)
		{
			gboolean ret = gdk_rgba_parse(color_setting, data);
			assert(ret == TRUE);
			return 0;
		}
	}
	return -1;
}

int Plot_area::end_configuration(void)
{
	if (static_grid)
	{
		if (static_grid_x_divisions == 0)
			static_grid_x_divisions = STATIC_GRID_DIVISIONS_DEFAULT;
		if (static_grid_y_divisions == 0)
			static_grid_y_divisions = STATIC_GRID_DIVISIONS_DEFAULT;
		update_static_grid_dimensions();
	}
	return 0;
}

void Plot_area::show_cursors_text(int visible)
{
	text_over_cursors = visible;
}

void Plot_area::show_viewer_info(int visible)
{
	viewer_info_visible = visible;
}

// TODO: Make this function thread-safe
int Plot_area::push_buffer(sample_rx_t *samples, uint32_t samples_count)
{
	uint32_t remaining_samples = buffer_capture_end - buffer_capture_cur;
	if (samples_count > remaining_samples)
		samples_count = remaining_samples;
	memcpy(buffer_capture_cur, samples, samples_count * sizeof(sample_rx_t));
	buffer_capture_cur += samples_count;
	if (buffer_capture_cur == buffer_capture_end)
	{
		// Swap buffers
		set_buffer_addresses(buffer_capture, buffer_plot);
		buffer_capture_counter++;
		valid_darea_data = 1;
		return 1;
	}
	return 0;
}

int Plot_area::save_samples(const char *filename, int in_digital_units)
{
	if (valid_darea_data)
	{
		if (in_digital_units)
		{
			return plc_file_write_csv(filename, csv_u16, buffer_viewer_ini, buffer_plot_count, 0);
		}
		else
		{
			uint16_t buffer_zero_ref = SAMPLES_ZERO_REF;
			return plc_file_write_csv_units(filename, csv_u16, buffer_viewer_ini, buffer_plot_count,
					time_per_sample_us / 1000000.0, &buffer_zero_ref, yunit_per_sample, 0);
		}
	}
	else
	{
		return -1;
	}
}

int Plot_area::save_png(const char *filename, int width, int height)
{
	cairo_surface_t *cr_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	cairo_t *cr = cairo_create(cr_surface);
	// 'cairo_create' adds increments the ref count on 'cr_surface' so we can already unref it
	cairo_surface_destroy(cr_surface);
	GtkAllocation rect = {
		0, 0, width, height };
	do_drawing(cr, rect);
	cairo_status_t status = cairo_surface_write_to_png(cr_surface, filename);
	cairo_destroy(cr);
	return (status == CAIRO_STATUS_SUCCESS) ? 0 : -1;
}

int Plot_area::save_svg(const char *filename, int width, int height)
{
	cairo_surface_t *cr_surface = cairo_svg_surface_create(filename, width, height);
	cairo_status_t status = cairo_surface_status(cr_surface);
	if (status != CAIRO_STATUS_SUCCESS)
		return -1;
	cairo_t *cr = cairo_create(cr_surface);
	// 'cairo_create' adds increments the ref count on 'cr_surface' so we can already unref it
	cairo_surface_destroy(cr_surface);
	GtkAllocation rect = {
		0, 0, width, height };
	do_drawing(cr, rect);
	cairo_destroy(cr);
	return 0;
}

int Plot_area::set_osc_buffer(float interval_ms, float time_per_sample_us)
{
	if ((interval_ms == buffer_capture_interval_ms)
			&& (time_per_sample_us == this->time_per_sample_us))
		return 0;
	free(buffer_B);
	free(buffer_A);
	this->time_per_sample_us = time_per_sample_us;
	init_buffers(interval_ms);
	return 1;
}

float Plot_area::get_osc_buffer_interval(void)
{
	return buffer_capture_interval_ms;
}

void Plot_area::set_yunit_symbol(const char* symbol)
{
	if (yunit_symbol)
	{
		free(yunit_symbol);
		yunit_symbol = NULL;
	}
	if (symbol)
		yunit_symbol = strdup(symbol);
}

const char *Plot_area::get_yunit_symbol(void)
{
	return yunit_symbol;
}

void Plot_area::set_yunit_per_sample(float yunit_per_sample)
{
	this->yunit_per_sample = yunit_per_sample;
}

void Plot_area::set_yunit_per_sample_preserving_viewer(float yunit_per_sample)
{
	float delta_ratio = yunit_per_sample / this->yunit_per_sample;
	set_yunit_per_sample(yunit_per_sample);
	// Preserve previous sample-based references
	yunit_range *= delta_ratio;
	yunit_offset *= delta_ratio;
	grid_yunit *= delta_ratio;
}

float Plot_area::get_yunit_per_sample(void)
{
	return yunit_per_sample;
}

void Plot_area::set_yunit_range(float range)
{
	yunit_range = range;
	if (static_grid)
		update_static_grid_dimensions();
	if (plot_fft)
		update_fft_yrange_user();
}

float Plot_area::get_yunit_range(void)
{
	return yunit_range;
}

void Plot_area::set_yunit_offset(float offset)
{
	yunit_offset = offset;
}

float Plot_area::get_yunit_offset(void)
{
	return yunit_offset;
}

void Plot_area::set_yunit_base(float base)
{
	yunit_base = base;
}

void Plot_area::set_xybase_to_cursor(void)
{
	yunit_base = yunit_offset - cursor_ini_y_ratio * yunit_range;
	time_us_base = darea_x_ratio_to_time_us(cursor_ini_x_ratio);
}

int Plot_area::get_static_grid(void)
{
	return static_grid;
}

void Plot_area::set_static_grid(int enable)
{
	static_grid = enable;
	if (static_grid)
		update_static_grid_dimensions();
}

int Plot_area::get_plot_samples(void)
{
	return plot_samples;
}

void Plot_area::set_plot_samples(int enable)
{
	plot_samples = enable;
}

int Plot_area::get_plot_fft(void)
{
	return plot_fft;
}

void Plot_area::set_plot_fft(int enable)
{
	plot_fft = enable;
	cursor_units = plot_fft ? cursor_units_fft_frequency : cursor_units_time_yunit;
	if (plot_fft)
	{
		init_buffers_fft();
		recalculate_fft();
	}
	else
	{
		release_buffers_fft();
	}
}

void Plot_area::set_fft_yrange_user(float yrange)
{
	fft_graph_yrange_user = yrange;
	update_fft_yrange_user();
}

float Plot_area::get_fft_yrange_user(void)
{
	return fft_graph_yrange_user;
}

const char *Plot_area::get_graph_title(void)
{
	return graph_title;
}

void Plot_area::set_graph_title(const char *title)
{
	free(graph_title);
	graph_title = strdup(title);
}

int Plot_area::get_grid(void)
{
	return grid;
}

void Plot_area::set_grid(int activate)
{
	grid = activate;
}

void Plot_area::set_grid_yunit(float value)
{
	grid_yunit = value;
}

float Plot_area::get_grid_yunit(void)
{
	return grid_yunit;
}

void Plot_area::set_grid_time(float interval_us)
{
	grid_time_us = interval_us;
}

float Plot_area::get_grid_time(void)
{
	return grid_time_us;
}

void Plot_area::set_sticky_cursor(int sticky)
{
	cursor_mode = sticky ? cursor_sticky : cursor_free;
}

void Plot_area::reset_cursor(void)
{
	show_cursors = 0;
	show_cursor_diff = 0;
}

int Plot_area::get_cursors_visible(void)
{
	return show_cursors;
}

void Plot_area::set_cursor_ini(float x_ratio, float y_ratio)
{
	cursor_ini_x_ratio = x_ratio;
	cursor_ini_y_ratio = y_ratio;
	cursor_end_x_ratio = x_ratio;
	cursor_end_y_ratio = y_ratio;
	show_cursors = 1;
	show_cursor_diff = 0;
}

void Plot_area::set_cursor_end(float x_ratio, float y_ratio)
{
	cursor_end_x_ratio = x_ratio;
	cursor_end_y_ratio = y_ratio;
	show_cursor_diff = 1;
}

int Plot_area::sprint_cursor_ini(char *text, size_t text_size)
{
	float time_us, axis_y_ratio;
	coordinates_to_measures(cursor_ini_x_ratio, cursor_ini_y_ratio, &time_us, &axis_y_ratio);
	return sprint_cursor_pair(text, text_size, cursor_ini_x_ratio, axis_y_ratio, time_us,
			darea_y_ratio_to_yunit(axis_y_ratio));
}

int Plot_area::sprint_cursor_end(char *text, size_t text_size)
{
	float time_us, axis_y_ratio;
	coordinates_to_measures(cursor_end_x_ratio, cursor_end_y_ratio, &time_us, &axis_y_ratio);
	return sprint_cursor_pair(text, text_size, cursor_end_x_ratio, axis_y_ratio, time_us,
			darea_y_ratio_to_yunit(axis_y_ratio));
}

int Plot_area::sprint_cursor_diff(char *text, size_t text_size)
{
	// TODO: Optimize to avoid redundant calculations
	float time_us_ini, time_us_end, axis_ini_y_ratio, axis_end_y_ratio;
	coordinates_to_measures(cursor_ini_x_ratio, cursor_ini_y_ratio, &time_us_ini,
			&axis_ini_y_ratio);
	coordinates_to_measures(cursor_end_x_ratio, cursor_end_y_ratio, &time_us_end,
			&axis_end_y_ratio);
	return sprint_custom_float_pair(text, text_size, fabs(time_us_end - time_us_ini),
			fabs(axis_end_y_ratio - axis_ini_y_ratio) * yunit_range, yunit_symbol);
	return 0;
}

int Plot_area::sprint_cursor_diff_freq(char *text, size_t text_size)
{
	if (cursor_end_x_ratio != cursor_ini_x_ratio)
	{
		float diff_cursor_us = fabs(
				darea_x_ratio_to_time_us(cursor_end_x_ratio)
						- darea_x_ratio_to_time_us(cursor_ini_x_ratio));
		return sprint_custom_float(text, 1000000.0 / diff_cursor_us, "Hz");
	}
	else
	{
		return 0;
	}
}

void Plot_area::auto_scale_x(void)
{
	set_viewer_interval_samples(DEFAULT_VIEWER_SAMPLES);
}

void Plot_area::auto_scale_y(void)
{
	sample_rx_t min, max;
	get_range_viewer(&min, &max);
	yunit_offset = ((max + min) / 2 - SAMPLES_ZERO_REF) * yunit_per_sample;
	sample_rx_t range = max - min;
	if (range < AUTO_SCALE_RANGE_MIN)
		range = AUTO_SCALE_RANGE_MIN;
	set_yunit_range(range * yunit_per_sample);
}

void Plot_area::auto_offset_y(void)
{
	sample_rx_t min, max;
	get_range_viewer(&min, &max);
	yunit_offset = ((max + min) / 2 - SAMPLES_ZERO_REF) * yunit_per_sample;
}

void Plot_area::zoom_viewer_to_cursor_area(void)
{
	float ini_x, ini_y, end_x, end_y;
	if (cursor_end_x_ratio < cursor_ini_x_ratio)
	{
		ini_x = cursor_end_x_ratio;
		end_x = cursor_ini_x_ratio;
	}
	else
	{
		ini_x = cursor_ini_x_ratio;
		end_x = cursor_end_x_ratio;
	}
	if (cursor_end_y_ratio < cursor_ini_y_ratio)
	{
		ini_y = cursor_end_y_ratio;
		end_y = cursor_ini_y_ratio;
	}
	else
	{
		ini_y = cursor_ini_y_ratio;
		end_y = cursor_end_y_ratio;
	}
	if (plot_fft)
	{
		fft_viewer_left += ini_x * fft_viewer_width;
		if (fft_viewer_left < 0.0)
			fft_viewer_left = 0.0;
		fft_viewer_width *= (end_x - ini_x);
	}
	else
	{
		buffer_viewer_left += ini_x * buffer_viewer_width;
		if (buffer_viewer_left < 0.0)
			buffer_viewer_left = 0.0;
		set_viewer_interval_samples((end_x - ini_x) * buffer_viewer_width);
		yunit_offset -= (ini_y + end_y - 1.0) / 2 * yunit_range;
		set_yunit_range((end_y - ini_y) * yunit_range);
	}
}

void Plot_area::move_viewer_x(float delta_x_ratio)
{
	buffer_viewer_left += delta_x_ratio * buffer_plot_count;
	if (buffer_viewer_left < 0.0)
		buffer_viewer_left = 0.0;
	int buffer_viewer_left_integer = round(buffer_viewer_left);
	if (buffer_viewer_left_integer + buffer_plot_count > buffers_count)
	{
		buffer_viewer_left_integer = buffers_count - buffer_plot_count;
		buffer_viewer_left = buffer_viewer_left_integer;
	}
	buffer_viewer_ini = buffer_plot + buffer_viewer_left_integer;
	if (plot_fft)
		recalculate_fft();
}

void Plot_area::move_viewer_y(float delta_y_ratio)
{
	yunit_offset -= delta_y_ratio * yunit_range;
}

void Plot_area::set_viewer_interval(float interval_ms)
{
	set_viewer_interval_samples(interval_ms * 1000.0 / time_per_sample_us);
}

float Plot_area::get_viewer_interval_ms(void)
{
	return buffer_viewer_width * time_per_sample_us / 1000.0;
}

float Plot_area::get_viewer_samples(void)
{
	return buffer_viewer_width;
}

void Plot_area::scale_viewer_x(float delta_x_ratio)
{
	buffer_viewer_left -= delta_x_ratio * buffer_plot_count / 2;
	if (buffer_viewer_left < 0.0)
		buffer_viewer_left = 0.0;
	set_viewer_interval_samples(buffer_viewer_width + delta_x_ratio * buffer_plot_count);
}

void Plot_area::scale_viewer_y(float delta_y_ratio)
{
	set_yunit_range(yunit_range * (1.0 - delta_y_ratio));
}

float Plot_area::get_viewer_left_us(void)
{
	return buffer_viewer_left * time_per_sample_us;
}

enum graph_drawing_mode Plot_area::get_graph_drawing_mode(void)
{
	return graph_drawing_mode;
}

void Plot_area::set_graph_drawing_mode(enum graph_drawing_mode mode)
{
	graph_drawing_mode = mode;
}

gboolean Plot_area::do_drawing(cairo_t *cr, GtkAllocation &area_rect)
{
	cairo_set_source_rgb(cr, .0, .0, .0);
	cairo_rectangle(cr, 0, 0, area_rect.width, area_rect.height);
	cairo_stroke(cr);
	if (font_face)
		cairo_select_font_face(cr, font_face, CAIRO_FONT_SLANT_NORMAL,
				font_bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
	if (font_size != 0.0)
		cairo_set_font_size(cr, font_size);
	if (viewer_info_visible && viewer_info_behind_graph)
		display_viewer_info(cr, area_rect);
	if (grid)
		plot_grid(cr, area_rect.width, area_rect.height);
	if (show_cursors)
		draw_cursors(cr, area_rect);
	if (graph_title[0] != '\0')
	{
		cairo_text_extents_t extents;
		cairo_text_extents(cr, graph_title, &extents);
		cairo_move_to(cr, (area_rect.width - extents.width) / 2,
				extents.height + GRAPH_TEXT_MARGIN);
		cairo_show_text(cr, graph_title);
	}
	if (valid_darea_data)
	{
		if (stroke_width != 0.0)
			cairo_set_line_width(cr, stroke_width);
		if (plot_samples)
			plot_buffer_viewer(cr, area_rect.width, area_rect.height);
		if (plot_fft)
		{
			// Only calculate FFT if required (if canvas is visible)
			if (buffer_capture_counter_fft != buffer_capture_counter)
				recalculate_fft();
			plot_buffer_viewer_fft(cr, area_rect.width, area_rect.height);
		}
	}
	if (viewer_info_visible && !viewer_info_behind_graph)
		display_viewer_info(cr, area_rect);
	return TRUE;
}

void Plot_area::plot_grid(cairo_t *cr, int width, int height)
{
	double prev_line_width = cairo_get_line_width(cr);
	cairo_set_line_width(cr, 1.0);
	cairo_set_source_rgb(cr, grid_color.red, grid_color.green, grid_color.blue);
	const double dashes[] = {
		1.0 };
	cairo_set_dash(cr, dashes, ARRAY_SIZE(dashes), 0);
	if (static_grid)
	{
		float grid_x = width / (float) static_grid_x_divisions;
		float x;
		for (x = 0.0; x < width; x += grid_x)
		{
			int x_round = round(x);
			cairo_move_to(cr, x_round, 0);
			cairo_line_to(cr, x_round, height);
		}
		float grid_y = height / (float) static_grid_y_divisions;
		float y;
		for (y = grid_y; y < height; y += grid_y)
		{
			int y_round = round(y);
			cairo_move_to(cr, 0.0, y_round);
			cairo_line_to(cr, width, y_round);
		}
		cairo_stroke(cr);
		cairo_set_dash(cr, NULL, 0, 0);
	}
	if (!static_grid)
	{
		float delta_x = (float) width / buffer_plot_count;
		float grid_x = grid_time_us / time_per_sample_us * delta_x;
		float min_y_range = (yunit_offset - SAMPLES_MAX_RANGE / 2.0 * yunit_per_sample)
				/ yunit_range * height + height / 2;
		float max_y_range = (yunit_offset + SAMPLES_MAX_RANGE / 2.0 * yunit_per_sample)
				/ yunit_range * height + height / 2;
		float min_y = 0;
		float max_y = height;
		if (min_y < min_y_range)
			min_y = min_y_range;
		if (max_y > max_y_range)
			max_y = max_y_range;
		float x = -((buffer_viewer_left - time_us_base / time_per_sample_us) / buffer_plot_count)
				* width;
		// Remove unnecessary (x < 0.0) lines with 'fmod' or equivalent formula
		// 	x = x - floor(x / grid_x) * grid_x;
		x = fmod(x, grid_x);
		for (; x < width; x += grid_x)
		{
			cairo_move_to(cr, x, min_y);
			cairo_line_to(cr, x, max_y);
		}
		cairo_stroke(cr);
		float zero_ref_y = height / 2 + (yunit_offset - yunit_base) / yunit_range * height;
		float grid_y = grid_yunit / yunit_range * height;
		float y = fmod(zero_ref_y, grid_y);
		// Special case: when the grid area height < draw area height
		if (y < min_y_range)
			y = min_y_range + fmod(zero_ref_y - min_y_range, grid_y);
		for (; y < max_y; y += grid_y)
		{
			cairo_move_to(cr, 0.0, y);
			cairo_line_to(cr, width, y);
		}
		// Draw the queue 'move_to' and 'line_to' with current stroke settings
		// The call to 'cairo_stroke0 is necessary here to allow changing the pen for a new stroke
		cairo_stroke(cr);
		cairo_set_dash(cr, NULL, 0, 0);
		cairo_move_to(cr, 0.0, zero_ref_y);
		cairo_line_to(cr, width, zero_ref_y);
		cairo_stroke(cr);
	}
	cairo_set_line_width(cr, prev_line_width);
}

void Plot_area::plot_buffer_viewer(cairo_t *cr, int width, int height)
{
	cairo_set_source_rgb(cr, samples_color.red, samples_color.green, samples_color.blue);
	float samples_range = yunit_range / yunit_per_sample;
	float pixels_per_sample = height / samples_range;
	cairo_matrix_t matrix;
	cairo_get_matrix(cr, &matrix);
	cairo_translate(cr, 0.0,
			(yunit_offset + yunit_range / 2.0) / yunit_per_sample * pixels_per_sample);
	cairo_move_to(cr, 0, 0);
	struct graph_plot graph_plot;
	graph_plot.cr = cr;
	graph_plot.y_base = -pixels_per_sample * yunit_base;
	graph_plot.sample_mark_size = sample_mark_size;
	graph_plot.x = 0.0;
	plot_sample_t plot_sample = get_plot_sample(graph_drawing_mode);
	uint32_t n = 0, n_next = 0;
	float n_real = 0.0;
	float delta_x, delta_n;
	int decimate = (graph_drawing_mode == graph_drawing_lines_fast_decimation);
	if (decimate && (buffer_plot_count >= (uint32_t) width))
	{
		delta_n = (float) buffer_plot_count / width;
		delta_x = 1.0;
	}
	else
	{
		delta_n = 1.0;
		delta_x = (float) width / buffer_plot_count;
	}
	for (; n < buffer_plot_count;)
	{
		n_real += delta_n;
		n_next = (uint32_t) round(n_real);
		graph_plot.y = -pixels_per_sample * (int16_t) (buffer_viewer_ini[n] - SAMPLES_ZERO_REF);
		plot_sample(&graph_plot);
		graph_plot.x += delta_x;
		n = n_next;
	}
	cairo_stroke(cr);
	cairo_set_matrix(cr, &matrix);
}

void Plot_area::plot_buffer_viewer_fft(cairo_t *cr, int width, int height)
{
	cairo_set_source_rgb(cr, fft_color.red, fft_color.green, fft_color.blue);
	float index_to_pos_x = (float) width / fft_viewer_width;
	float value_to_pos_y = (float) height / fft_graph_yrange;
	uint32_t graph_ini = fft_viewer_left;
	assert(graph_ini >= 0);
	uint32_t graph_count = fft_viewer_width;
	if (graph_ini + graph_count > fft_graph_count)
		graph_count = fft_graph_count - graph_ini;
	plot_double(cr, fft_graph + graph_ini, fft_viewer_width, 0, height, index_to_pos_x,
			value_to_pos_y, graph_drawing_mode, sample_mark_size);
}

void Plot_area::plot_double(cairo_t *cr, float *graph, uint32_t graph_count, int ref_x, int ref_y,
		float index_to_pos_x, float value_to_pos_y, enum graph_drawing_mode graph_drawing_mode,
		float sample_mark_size)
{
	cairo_move_to(cr, ref_x, ref_y);
	struct graph_plot graph_plot;
	graph_plot.cr = cr;
	graph_plot.y_base = ref_y;
	graph_plot.sample_mark_size = sample_mark_size;
	plot_sample_t plot_sample = get_plot_sample(graph_drawing_mode);
	uint32_t index = 0;
	for (; index < graph_count; index++)
	{
		graph_plot.x = ref_x + index_to_pos_x * index;
		graph_plot.y = ref_y - value_to_pos_y * graph[index];
		plot_sample(&graph_plot);
	}
	cairo_stroke(cr);
}

void Plot_area::display_viewer_info(cairo_t *cr, const GtkAllocation &area_rect)
{
	double y_baseline = GRAPH_TEXT_MARGIN - GRAPH_TEXT_INTERLINE;
	char text[64];
	char *text_cur;
	if (plot_samples)
	{
		text_cur = text;
		text_cur += sprintf(text_cur, "Viewer: ");
		*text_cur++ = '[';
		text_cur += sprint_custom_float(text_cur,
				buffer_viewer_width * time_per_sample_us / 1000000.0, "s");
		*text_cur++ = ';';
		*text_cur++ = ' ';
		text_cur += sprint_custom_float(text_cur, yunit_offset, yunit_symbol);
		// '±' is encoded in UTF-8 as 'C2 B1' (see 'http://www.utf8-chartable.de/')
		*text_cur++ = 0xC2;
		*text_cur++ = 0xB1;
		text_cur += sprint_custom_float(text_cur, yunit_range / 2, yunit_symbol);
		*text_cur++ = ']';
		*text_cur = '\0';
		uint32_t chars = text_cur - text;
		assert(chars < sizeof(text));
		y_baseline = print_viewer_info_text(cr, area_rect.width, y_baseline, samples_color, text);
		if (grid)
		{
			text_cur = text;
			text_cur += sprintf(text_cur, "Grid: ");
			sprint_custom_float_pair(text_cur, sizeof(text) - (text_cur - text), grid_time_us,
					grid_yunit, NULL);
			y_baseline = print_viewer_info_text(cr, area_rect.width, y_baseline, samples_color,
					text);
		}
	}
	if (plot_fft)
	{
		cairo_set_source_rgb(cr, fft_color.red, fft_color.green, fft_color.blue);
		text_cur = text;
		text_cur += sprintf(text_cur, "Viewer: ");
		*text_cur++ = '[';
		text_cur += sprint_custom_float(text_cur,
				darea_fft_viewer_index_to_frequency(fft_viewer_left), "Hz");
		*text_cur++ = '.';
		*text_cur++ = '.';
		text_cur += sprint_custom_float(text_cur,
				darea_fft_viewer_index_to_frequency(fft_viewer_left + fft_viewer_width), "Hz");
		*text_cur++ = ';';
		*text_cur++ = ' ';
		float fft_y_range_normalized = fft_graph_yrange / fft_graph_count;
		text_cur += sprint_custom_float(text_cur, fft_y_range_normalized, NULL, 2);
		*text_cur++ = ']';
		*text_cur = '\0';
		uint32_t chars = text_cur - text;
		assert(chars < sizeof(text));
		y_baseline = print_viewer_info_text(cr, area_rect.width, y_baseline, fft_color, text);
		if (grid)
		{
			text_cur = text;
			text_cur += sprintf(text_cur, "Grid: ");
			float x_divisions = buffer_viewer_width * time_per_sample_us / grid_time_us;
			float y_divisions = yunit_range / grid_yunit;
			sprint_custom_float_pair_fft(text_cur, sizeof(text) - (text_cur - text),
					darea_fft_viewer_index_to_frequency(fft_viewer_width) / x_divisions,
					fft_y_range_normalized / y_divisions);
			y_baseline = print_viewer_info_text(cr, area_rect.width, y_baseline, fft_color, text);
		}
	}
}

double Plot_area::print_viewer_info_text(cairo_t *cr, int area_width, double y_baseline,
		const GdkRGBA &text_color, const char *text)
{
	cairo_text_extents_t extents;
	cairo_text_extents(cr, text, &extents);
	y_baseline += GRAPH_TEXT_INTERLINE + extents.height;
	if (!viewer_info_behind_graph)
	{
		cairo_set_source_rgb(cr, 1., 1., 1.);
		cairo_rectangle(cr, area_width - extents.width - GRAPH_TEXT_MARGIN - 1,
				y_baseline + extents.y_bearing - 1, extents.width + 2, extents.height + 2);
		cairo_fill(cr);
	}
	cairo_move_to(cr, area_width - extents.width - GRAPH_TEXT_MARGIN, y_baseline);
	cairo_set_source_rgb(cr, text_color.red, text_color.green, text_color.blue);
	cairo_show_text(cr, text);
	return y_baseline;
}

void Plot_area::draw_cursors(cairo_t *cr, const GtkAllocation &area_rect)
{
	// TODO: Make 'show_cursors_text_abs_and_diff' a user-configurable option
	static const int show_cursors_text_abs_and_diff = 0;
	if (cursor_units == cursor_units_fft_frequency)
		cairo_set_source_rgb(cr, fft_cursors_color.red, fft_cursors_color.green,
				fft_cursors_color.blue);
	else
		cairo_set_source_rgb(cr, samples_cursors_color.red, samples_cursors_color.green,
				samples_cursors_color.blue);
	float time_us_ini, axis_ini_y_ratio;
	coordinates_to_measures(cursor_ini_x_ratio, cursor_ini_y_ratio, &time_us_ini,
			&axis_ini_y_ratio);
	float yunit_ini = darea_y_ratio_to_yunit(axis_ini_y_ratio);
	draw_cursor(cr, area_rect, cursor_ini_x_ratio, axis_ini_y_ratio, yunit_ini, time_us_ini,
			text_over_cursors && (!show_cursor_diff || show_cursors_text_abs_and_diff));
	if (show_cursor_diff)
	{
		float time_us_end, axis_end_y_ratio;
		coordinates_to_measures(cursor_end_x_ratio, cursor_end_y_ratio, &time_us_end,
				&axis_end_y_ratio);
		float yunit_end = darea_y_ratio_to_yunit(axis_end_y_ratio);
		draw_cursor(cr, area_rect, cursor_end_x_ratio, axis_end_y_ratio, yunit_end, time_us_end,
				text_over_cursors && show_cursors_text_abs_and_diff);
		if (text_over_cursors)
		{
			char text[32];
			sprint_cursor_pair(text, sizeof(text), fabs(cursor_end_x_ratio - cursor_ini_x_ratio),
					1.0 - fabs(axis_end_y_ratio - axis_ini_y_ratio),
					fabs(time_us_end - time_us_ini),
					fabs(axis_end_y_ratio - axis_ini_y_ratio) * yunit_range);
			cairo_text_extents_t extents;
			cairo_text_extents(cr, text, &extents);
			double x = (cursor_ini_x_ratio + cursor_end_x_ratio) * area_rect.width / 2.0
					- (extents.width / 2 + extents.x_bearing);
			double y = area_rect.height - (extents.height + extents.y_bearing + 1);
			cairo_move_to(cr, x, y);
			cairo_show_text(cr, text);
		}
		double y1, y2;
		switch (cursor_mode)
		{
		case cursor_free:
			y1 = cursor_ini_y_ratio * area_rect.height;
			y2 = cursor_end_y_ratio * area_rect.height;
			break;
		case cursor_sticky:
			y1 = area_rect.height / 2 - (.5 - axis_ini_y_ratio) * area_rect.height;
			y1 = area_rect.height / 2 - (.5 - axis_end_y_ratio) * area_rect.height;
			break;
		default:
			assert(0);
		}
		float cursor_ini_x = cursor_ini_x_ratio * area_rect.width;
		float cursor_end_x = cursor_end_x_ratio * area_rect.width;
		// Paint the surrounded area
		if (cursor_units == cursor_units_fft_frequency)
			cairo_set_source_rgba(cr, fft_color.red, fft_color.green, fft_color.blue, .1);
		else
			cairo_set_source_rgba(cr, samples_color.red, samples_color.green, samples_color.blue,
					.1);
		cairo_rectangle(cr, cursor_ini_x, y1, cursor_end_x - cursor_ini_x, y2 - y1);
		cairo_fill(cr);
		cairo_stroke(cr);
	}
}

void Plot_area::draw_cursor(cairo_t *cr, const GtkAllocation &area_rect, double x_pos_ratio,
		double y_pos_ratio, float yunit, float time_us, int show_text)
{
	// Draw the axis
	const double dashes[] = {
		3.0 };
	cairo_set_dash(cr, dashes, ARRAY_SIZE(dashes), 0);
	double x = x_pos_ratio * area_rect.width;
	double y = y_pos_ratio * area_rect.height;
	cairo_move_to(cr, x, 0.0);
	cairo_line_to(cr, x, area_rect.height);
	cairo_move_to(cr, 0.0, y);
	cairo_line_to(cr, area_rect.width, y);
	cairo_stroke(cr);
	cairo_set_dash(cr, NULL, 0, 0);
	// Write cursor text
	if (show_text)
	{
		char text[32];
		sprint_cursor_pair(text, sizeof(text), x_pos_ratio, y_pos_ratio, time_us, yunit);
		cairo_text_extents_t extents;
		cairo_text_extents(cr, text, &extents);
		static const double margin = 1.0;
		// In cairo the (x,y) text coordinates refer to (left, bottom-baseline) position of text
		x += extents.x_bearing + 1.0;
		y -= extents.height + extents.y_bearing + 1.0;
		if (x + extents.width > area_rect.width - margin - 1.0)
			x = area_rect.width - extents.width - margin - 1.0;
		if (y > area_rect.height - (extents.height + extents.y_bearing + margin))
			y = area_rect.height - (extents.height + extents.y_bearing + margin);
		if (x < margin)
			x = margin;
		if (y < -extents.y_bearing + margin + 1.0)
			y = -extents.y_bearing + margin + 1.0;
		cairo_move_to(cr, x, y);
		cairo_show_text(cr, text);
	}
}

int Plot_area::sprint_cursor_pair(char *text, uint16_t text_size, float x_pos_ratio,
		float y_pos_ratio, float time_us, float yunit)
{
	switch (cursor_units)
	{
	case cursor_units_time_yunit:
		return sprint_custom_float_pair(text, text_size, time_us, yunit, yunit_symbol);
	case cursor_units_fft_frequency:
		return sprint_custom_float_pair_fft(text, text_size,
				darea_fft_viewer_index_to_frequency(
						fft_viewer_left + x_pos_ratio * fft_viewer_width),
				(1.0 - y_pos_ratio) * fft_graph_yrange / fft_graph_count);
	}
	assert(0);
	return 0;
}

void Plot_area::coordinates_to_measures(float x_ratio, float y_ratio, float *time_us,
		float *axis_y_ratio)
{
	if (time_us)
		*time_us = darea_x_ratio_to_time_us(x_ratio);
	if (axis_y_ratio)
	{
		*axis_y_ratio =
				(cursor_mode == cursor_sticky) ?
						0.5 - (darea_x_ratio_to_yunit(x_ratio) - yunit_offset) / yunit_range :
						y_ratio;
	}
}

void Plot_area::recalculate_fft(void)
{
	uint32_t n;
	for (n = 0; n < buffer_plot_count; n++)
	{
		fft_in[n][0] = buffer_viewer_ini[n];
		fft_in[n][1] = 0;
	}
	fftw_execute(fft_plan);
	// Ignore the continuous component (freq = 0)
	fft_graph[0] = 0.0;
	fft_graph_max = 0.0;
	for (n = 1; n < buffer_plot_count; n++)
	{
		fft_graph[n] = sqrt(fft_out[n][0] * fft_out[n][0] + fft_out[n][1] * fft_out[n][1]);
		if (fft_graph[n] > fft_graph_max)
			fft_graph_max = fft_graph[n];
	}
	update_fft_yrange_user();
	buffer_capture_counter_fft = buffer_capture_counter;
}

void Plot_area::get_range_viewer(sample_rx_t *min, sample_rx_t *max)
{
	sample_rx_t *buffer = buffer_viewer_ini;
	uint32_t count = buffer_plot_count;
	*min = (sample_rx_t) -1;
	*max = 0;
	for (; count > 0; count--, buffer++)
	{
		if (*buffer < *min)
			*min = *buffer;
		if (*buffer > *max)
			*max = *buffer;
	}
}

void Plot_area::set_viewer_interval_samples(float viewer_width)
{
	buffer_viewer_width = viewer_width;
	int osc_buffer_viewer_width_integer = round(buffer_viewer_width);
	if (osc_buffer_viewer_width_integer < 0)
	{
		buffer_viewer_width = 1.0;
		buffer_plot_count = 1;
	}
	else if ((uint32_t) osc_buffer_viewer_width_integer > buffers_count)
	{
		buffer_viewer_width = buffers_count;
		buffer_plot_count = buffers_count;
	}
	else
	{
		buffer_plot_count = osc_buffer_viewer_width_integer;
	}
	if (static_grid)
		update_static_grid_dimensions();
	// If FFT is shown (based on 'buffer_viewer_count' samples) update buffers
	if (plot_fft)
	{
		release_buffers_fft();
		init_buffers_fft();
		recalculate_fft();
	}
	// When scaling it's possible that left position needs to be adjusted to accommodate the whole
	// capturing range. Check it simulating a zero-scroll on x
	move_viewer_x(0);
}

// PRECONDITION: 'buffer_A' and 'buffer_B' unallocated
void Plot_area::init_buffers(float interval_ms)
{
	valid_darea_data = 0;
	buffer_capture_interval_ms = interval_ms;
	buffer_capture_counter = 0;
	buffers_count = ceil(interval_ms * 1000.0 / this->time_per_sample_us);
	buffer_viewer_left = 0.0;
	buffer_viewer_width = DEFAULT_VIEWER_SAMPLES;
	if (buffer_viewer_width > buffers_count)
		buffer_viewer_width = buffers_count;
	buffer_plot_count = round(buffer_viewer_width);
	buffer_A = (sample_rx_t*) calloc(1, buffers_count * sizeof(sample_rx_t));
	buffer_B = (sample_rx_t*) malloc(buffers_count * sizeof(sample_rx_t));
	set_buffer_addresses(buffer_A, buffer_B);
	if (plot_fft)
	{
		release_buffers_fft();
		init_buffers_fft();
	}
	if (static_grid)
		update_static_grid_dimensions();
}

void Plot_area::init_buffers_fft(void)
{
	assert((fft_in == NULL) && (fft_out == NULL) && (fft_graph == NULL));
	fft_graph_count = buffer_plot_count;
	fft_in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * fft_graph_count);
	fft_out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * fft_graph_count);
	fft_plan = fftw_plan_dft_1d(fft_graph_count, fft_in, fft_out, FFTW_FORWARD,
	FFTW_ESTIMATE);
	fft_graph = (float*) malloc(fft_graph_count * sizeof(float));
	buffer_capture_counter_fft = buffer_capture_counter - 1;
	fft_viewer_left = 0.0;
	fft_viewer_width = fft_graph_count / 2.0 - fft_viewer_left;
}

void Plot_area::release_buffers(void)
{
	free(buffer_B);
	buffer_B = NULL;
	free(buffer_A);
	buffer_A = NULL;
	if (plot_fft)
		release_buffers_fft();
}

void Plot_area::release_buffers_fft(void)
{
	free(fft_graph);
	fft_graph = NULL;
	fftw_destroy_plan(fft_plan);
	fftw_free(fft_in);
	fft_in = NULL;
	fftw_free(fft_out);
	fft_out = NULL;
}

void Plot_area::set_buffer_addresses(sample_rx_t *new_buffer_plot, sample_rx_t *new_buffer_capture)
{
	buffer_capture = new_buffer_capture;
	buffer_plot = new_buffer_plot;
	buffer_capture_cur = buffer_capture;
	buffer_capture_end = buffer_capture + buffers_count;
	buffer_viewer_ini = buffer_plot + (uint32_t) round(buffer_viewer_left);
}

float Plot_area::darea_x_ratio_to_yunit(float x_ratio)
{
	if (plot_fft)
	{
		uint32_t sample_index = round(fft_viewer_left + x_ratio * fft_viewer_width);
		if (sample_index >= fft_graph_count)
			sample_index = fft_graph_count - 1;
		float y_ratio = 1.0 - fft_graph[sample_index] / fft_graph_yrange;
		return darea_y_ratio_to_yunit(y_ratio);
	}
	else
	{
		uint32_t sample_index = round(x_ratio * buffer_plot_count);
		if (sample_index >= buffer_plot_count)
			sample_index = buffer_plot_count;
		return ((int32_t) buffer_viewer_ini[sample_index] - SAMPLES_ZERO_REF) * yunit_per_sample;
	}
}

float Plot_area::darea_x_ratio_to_time_us(float x_ratio)
{
	return (buffer_viewer_left + x_ratio * buffer_plot_count) * time_per_sample_us;
}

float Plot_area::darea_y_ratio_to_yunit(float y_ratio)
{
	return (0.5 - y_ratio) * yunit_range + yunit_offset;
}

void Plot_area::update_fft_yrange_user(void)
{
	fft_graph_yrange =
			(fft_graph_yrange_user == 0.0) ?
					fft_graph_max : fft_graph_yrange_user * fft_graph_count;
}

float Plot_area::darea_fft_viewer_index_to_frequency(float viewer_index)
{
	return 1000000.0 / time_per_sample_us / fft_graph_count * viewer_index;
}

void Plot_area::update_static_grid_dimensions(void)
{
	grid_time_us = buffer_viewer_width * time_per_sample_us / static_grid_x_divisions;
	grid_yunit = yunit_range / static_grid_y_divisions;
}

Plot_area::plot_sample_t Plot_area::get_plot_sample(enum graph_drawing_mode graph_drawing_mode)
{
	switch (graph_drawing_mode)
	{
	case graph_drawing_lines:
	case graph_drawing_lines_fast_decimation:
		return plot_sample_lines;
	case graph_drawing_bars:
		return plot_sample_bars;
	case graph_drawing_dots:
		return plot_sample_dots;
	case graph_drawing_crosses:
		return plot_sample_crosses;
	case graph_drawing_circles:
		return plot_sample_circles;
	case graph_drawing_rectangles:
		return plot_sample_rectangles;
	default:
		assert(0);
		return 0;
	}
}

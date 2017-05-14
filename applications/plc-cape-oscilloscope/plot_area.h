/**
 * @file
 * @brief	Plots a graphic in a drawable GtkWidget
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef PLOT_AREA_H
#define PLOT_AREA_H

#include <gtk/gtk.h>
#include <fftw3.h>
#include "configuration_interface.h"

extern const char *graph_drawing_mode_text[];
enum graph_drawing_mode
{
	graph_drawing_lines = 0,
	graph_drawing_lines_fast_decimation,
	graph_drawing_bars,
	graph_drawing_dots,
	graph_drawing_crosses,
	graph_drawing_circles,
	graph_drawing_rectangles,
	graph_drawing_COUNT
};

struct plot_area_configuration
{
	float yunit_per_sample;
	float yunit_range;
	float yunit_offset;
	float yunit_base;
	float time_us_base;
	int grid;
	int static_grid;
	uint8_t static_grid_x_divisions;
	uint8_t static_grid_y_divisions;
	float grid_yunit;
	float grid_time_us;
	float time_per_sample_us;
	enum graph_drawing_mode graph_drawing_mode;
	char *font_face;
	int font_bold;
	float font_size;
	float stroke_width;
	float sample_mark_size;
	GdkRGBA samples_color;
	GdkRGBA samples_cursors_color;
	GdkRGBA fft_color;
	GdkRGBA fft_cursors_color;
	GdkRGBA grid_color;
};

class Plot_area: public Configuration_interface, private plot_area_configuration
{
public:
	Plot_area();
	virtual ~Plot_area();
	void release_configuration(void);
	void set_configuration_defaults(void);
	// Configuration_interface
	int begin_configuration(void);
	int set_configuration_setting(const char *identifier, const char *data);
	int end_configuration(void);
	void show_cursors_text(int visible);
	void show_viewer_info(int visible);
	int push_buffer(uint16_t *samples, uint32_t samples_count);
	int save_samples(const char *filename, int in_digital_units);
	int save_png(const char *filename, int width, int height);
	int save_svg(const char *filename, int width, int height);
	int set_osc_buffer(float interval_ms, float us_per_sample);
	float get_osc_buffer_interval(void);
	void set_yunit_symbol(const char* symbol);
	const char *get_yunit_symbol(void);
	void set_yunit_per_sample(float yunit_per_sample);
	void set_yunit_per_sample_preserving_viewer(float yunit_per_sample);
	float get_yunit_per_sample(void);
	void set_yunit_range(float range);
	float get_yunit_range(void);
	void set_yunit_offset(float offset);
	float get_yunit_offset(void);
	void set_yunit_base(float base);
	void set_xybase_to_cursor(void);
	int get_static_grid(void);
	void set_static_grid(int enable);
	int get_plot_samples(void);
	void set_plot_samples(int enable);
	int get_plot_fft(void);
	void set_plot_fft(int enable);
	void set_fft_yrange_user(float yrange);
	float get_fft_yrange_user(void);
	const char *get_graph_title(void);
	void set_graph_title(const char *title);
	int get_grid(void);
	void set_grid(int activate);
	void set_grid_yunit(float value);
	float get_grid_yunit(void);
	void set_grid_time(float interval_us);
	float get_grid_time(void);
	void set_sticky_cursor(int sticky);
	void reset_cursor(void);
	int get_cursors_visible(void);
	void set_cursor_ini(float x_ratio, float y_ratio);
	void set_cursor_end(float x_ratio, float y_ratio);
	int sprint_cursor_ini(char *text, size_t text_size);
	int sprint_cursor_end(char *text, size_t text_size);
	int sprint_cursor_diff(char *text, size_t text_size);
	int sprint_cursor_diff_freq(char *text, size_t text_size);
	void auto_scale_x(void);
	void auto_scale_y(void);
	void auto_offset_y(void);
	void zoom_viewer_to_cursor_area(void);
	void move_viewer_x(float delta_x_ratio);
	void move_viewer_y(float delta_y_ratio);
	void set_viewer_interval(float interval_ms);
	float get_viewer_interval_ms(void);
	float get_viewer_samples(void);
	void scale_viewer_x(float delta_x_ratio);
	void scale_viewer_y(float delta_y_ratio);
	float get_viewer_left_us(void);
	enum graph_drawing_mode get_graph_drawing_mode(void);
	void set_graph_drawing_mode(enum graph_drawing_mode mode);
	gboolean do_drawing(cairo_t *cr, GtkAllocation &area_rect);

private:
	enum cursor_mode
	{
		cursor_free, cursor_sticky
	};

	enum cursor_units
	{
		cursor_units_time_yunit = 0, cursor_units_fft_frequency,
	};

	typedef void (*plot_sample_t)(struct graph_plot *);

	void plot_grid(cairo_t *cr, int width, int height);
	void plot_buffer_viewer(cairo_t *cr, int width, int height);
	void plot_buffer_viewer_fft(cairo_t *cr, int width, int height);
	static void plot_double(cairo_t *cr, float * graph, uint32_t graph_count, int ref_x, int ref_y,
			float index_to_pos_x, float value_to_pos_y, enum graph_drawing_mode graph_drawing_mode,
			float sample_mark_size);
	void display_viewer_info(cairo_t *cr, const GtkAllocation &area_rect);
	double print_viewer_info_text(cairo_t *cr, int area_width, double y_baseline,
			const GdkRGBA &text_color, const char *text);
	void draw_cursors(cairo_t *cr, const GtkAllocation &area_rect);
	void draw_cursor(cairo_t *cr, const GtkAllocation &area_rect, double x_pos_ratio,
			double y_pos_ratio, float yunit, float time_us, int show_text);
	int sprint_cursor_pair(char *text, uint16_t text_size, float x_pos_ratio, float y_pos_ratio,
			float time_us, float yunit);
	void coordinates_to_measures(float x_ratio, float y_ratio, float *time_us, float *axis_y_ratio);
	void recalculate_fft(void);
	void get_range_viewer(sample_rx_t *min, sample_rx_t *max);
	void set_viewer_interval_samples(float viewer_width);
	void init_buffers(float interval_ms);
	void init_buffers_fft(void);
	void release_buffers(void);
	void release_buffers_fft(void);
	void set_buffer_addresses(sample_rx_t *new_buffer_plot, sample_rx_t *new_buffer_capture);
	float darea_x_ratio_to_yunit(float x_ratio);
	float darea_x_ratio_to_time_us(float x_ratio);
	float darea_y_ratio_to_yunit(float y_ratio);
	void update_fft_yrange_user(void);
	float darea_fft_viewer_index_to_frequency(float viewer_index);
	void update_static_grid_dimensions(void);
	static plot_sample_t get_plot_sample(enum graph_drawing_mode graph_drawing_mode);

	static const struct plc_setting_definition accepted_settings[];
	int valid_darea_data;
	uint32_t buffers_count;
	sample_rx_t *buffer_A, *buffer_B;
	sample_rx_t *buffer_capture, *buffer_capture_cur, *buffer_capture_end;
	uint32_t buffer_capture_counter;
	float buffer_capture_interval_ms;
	sample_rx_t *buffer_plot, *buffer_viewer_ini;
	float buffer_viewer_left;
	float buffer_viewer_width;
	uint32_t buffer_plot_count;
	int plot_samples;
	int plot_fft;
	fftw_complex *fft_in, *fft_out;
	fftw_plan fft_plan;
	uint32_t fft_graph_count;
	float *fft_graph;
	float fft_graph_max;
	float fft_graph_yrange_user;
	float fft_graph_yrange;
	float fft_viewer_left;
	float fft_viewer_width;
	uint32_t buffer_capture_counter_fft;
	char *yunit_symbol;
	int show_cursors;
	int text_over_cursors;
	int viewer_info_visible;
	int show_cursor_diff;
	enum cursor_mode cursor_mode;
	enum cursor_units cursor_units;
	float cursor_ini_x_ratio, cursor_ini_y_ratio;
	float cursor_end_x_ratio, cursor_end_y_ratio;
	char *graph_title;
};

#endif /* PLOT_AREA_H */

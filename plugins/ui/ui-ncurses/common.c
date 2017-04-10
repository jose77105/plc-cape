/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include <ncurses.h>
#include "+common/api/+base.h"
#include "common.h"

ATTR_INTERN void tui_window_print_at_pos(WINDOW *window, int starty, int startx, const char *text,
		chtype color)
{
	wattron(window, color);
	mvwprintw(window, starty, startx, "%s", text);
	wattroff(window, color);
}

ATTR_INTERN void tui_window_print_centered_title(WINDOW *window, const char *text, chtype color)
{
	int height, width;
	getmaxyx(window, height, width);
	// Dummy sentence just to avoid a "variable set but not used" warning
	(void)height;
	wattron(window, color);
	mvwprintw(window, 0, (width - strlen(text)) / 2, "%s", text);
	wattroff(window, color);
}

ATTR_INTERN void tui_window_print_in_middle(WINDOW *window, int starty, int startx, int width,
		const char *text, chtype color)
{
	int length, x, y;
	float temp;
	if (window == NULL)
		window = stdscr;
	getyx(window, y, x);
	if (startx != 0)
		x = startx;
	if (starty != 0)
		y = starty;
	if (width == 0)
		width = 80;
	length = strlen(text);
	temp = (width - length) / 2;
	x = startx + (int) temp;
	tui_window_print_at_pos(window, y, x, text, color);
}

ATTR_INTERN void tui_window_get_dimensions(WINDOW *window, int *pos_y, int *pos_x, int *rows,
		int *cols)
{
	int y, x, r, c;
	getyx(window, y, x);
	// For a specific window 'getmaxyx' can be get to obtain its dimensions:
	//	getmaxyx(stdscr, screen_rows, screen_cols);
	getmaxyx(window, r, c);
	if (pos_y)
		*pos_y = y;
	if (pos_x)
		*pos_x = x;
	if (rows)
		*rows = r;
	if (cols)
		*cols = c;
}

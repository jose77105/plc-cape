/**
 * @file
 * @brief	Common functions used in the plugin
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef COMMON_H
#define COMMON_H

void tui_window_print_at_pos(WINDOW *window, int starty, int startx, const char *text,
		chtype color);
void tui_window_print_centered_title(WINDOW *window, const char *text, chtype color);
void tui_window_print_in_middle(WINDOW *window, int starty, int startx, int width, const char *text,
		chtype color);
void tui_window_get_dimensions(WINDOW *window, int *pos_y, int *pos_x, int *rows, int *cols);

#endif /* COMMON_H */

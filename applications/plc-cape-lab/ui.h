/**
 * @file
 * @brief	User interface menu
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef UI_H
#define UI_H

#include "+common/api/ui.h"

struct ui *ui_create(const char *ui_plugin_name, int argc, char *argv[], struct settings *settings);
void ui_release(struct ui *ui);
void ui_set_event(struct ui *ui, uint32_t id, uint32_t data);
void ui_log_text(struct ui *ui, const char *text);
void ui_set_status_bar(struct ui *ui, const char *text);
void ui_do_menu_loop(struct ui *ui);
void ui_quit(struct ui *ui);
void ui_refresh(struct ui *ui);

#endif /* UI_H */

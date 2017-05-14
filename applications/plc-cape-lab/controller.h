/**
 * @file
 * @brief	Core engine to control the main application functionalities
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "monitor.h"	// enum monitor_profile_enum
#include "profiles.h"	// enum configuration_profile_enum

struct settings;
struct ui;

// 'controller' is a singleton object
void controller_create(int argc, char *argv[]);
void controller_release(void);
void controller_encoder_set_default_configuration(void);
void controller_decoder_set_default_configuration(void);
void controller_encoder_apply_configuration(void);
void controller_decoder_apply_configuration(void);
void controller_reload_encoder_plugin(void);
void controller_push_profile(const char *profile_identifier,
		struct setting_list_item **encoder_settings, struct setting_list_item **decoder_settings);
void controller_reload_decoder_plugin(void);
char *controller_get_settings_text(void);
void controller_activate_async(void);
void controller_deactivate(void);
int controlles_is_active(void);
void controller_set_app_settings(void);
void controller_reload_configuration_profile(const char *profile);
void controller_set_monitoring_profile(enum monitor_profile_enum profile);
void controller_clear_overloads(void);
void controller_log_overloads(void);
void controller_view_current_settings(void);
void controller_measure_encoder_time(void);
struct plc_plugin_list *controller_get_encoder_plugins(void);
struct plc_plugin_list *controller_get_decoder_plugins(void);
struct profiles *controller_get_profiles(void);
struct setting_linked_list_item *controller_encoder_get_settings(void);
struct setting_linked_list_item *controller_decoder_get_settings(void);

#endif /* LED_H */

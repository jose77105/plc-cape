/**
 * @file
 * @brief	Helper functions related to the plugin management
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef LIBPLC_TOOLS_PLUGIN_H
#define LIBPLC_TOOLS_PLUGIN_H

/**
 * @brief Allowed plugin categories
 */
enum plc_plugin_category
{
	plc_plugin_category_encoder = 0,
	plc_plugin_category_decoder,
	plc_plugin_category_ui,
};

/**
 * @brief List of plugins
 */
struct plc_plugin_list
{
	char **list;
	uint32_t list_count;
	uint32_t active_index;
};

/**
 * @brief	Gets the absolute path for a specific plugin
 * @param	category	Category of the plugin
 * @param	plugin_name	Name of the plugin (*without extension*)
 * @return	The absolute path where it is expected the plugin resides.
 *			Release it with _free_ when no longer required
 */
char *plc_plugin_get_abs_path(enum plc_plugin_category category, const char *plugin_name);
/**
 * @brief	Generates a list of all the available plugins for a given category
 * @param	category	Category of the plugins to be analyzed
 * @return	A pointer to the object containing the list of plugin names.
 *			Release it with _plugin_list_release_ when no longer required
 */
struct plc_plugin_list *plc_plugin_list_create(enum plc_plugin_category category);
/**
 * @brief	Releases a list of plugins
 * @param	plc_plugin_list		Pointer to the handler object
 */
void plc_plugin_list_release(struct plc_plugin_list *plc_plugin_list);
/**
 * @brief	Locates a plugin inside a _plugin_list_ from its name
 * @return	The posistion within the _plugin_list_
 */
int plc_plugin_list_find_name(struct plc_plugin_list *plc_plugin_list, const char *plugin_name);

#endif /* LIBPLC_TOOLS_PLUGIN_H */

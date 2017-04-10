/**
 * @file
 * @brief	Support to manage plugins (dynamic libraries)
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef PLUGINS_H
#define PLUGINS_H

struct plugin;

void *load_dl_function(void *so_handle, const char *function_name, int mandatory);
struct plugin *load_plugin(const char *path, void **api, uint32_t *api_version, uint32_t *api_size);
void unload_plugin(struct plugin *plugin);

#endif /* PLUGINS_H */

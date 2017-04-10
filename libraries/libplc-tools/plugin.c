/**
 * @file
 * 
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#define _GNU_SOURCE		// asprintf
#include <dirent.h>		// opendir
#include "+common/api/+base.h"
#include "api/application.h"
#include "api/plugin.h"

#define PLUGINS_MAX_ITEMS 64
#define PLUGINS_DIR "plugins"
#define PLUGINS_CATEGORY_ENCODER "encoder"
#define PLUGINS_CATEGORY_DECODER "decoder"
#define PLUGINS_CATEGORY_UI "ui"

const char *plc_plugin_category_get_rel_dir(enum plc_plugin_category category)
{
	switch (category)
	{
	case plc_plugin_category_encoder:
		return PLUGINS_DIR "/" PLUGINS_CATEGORY_ENCODER;
	case plc_plugin_category_decoder:
		return PLUGINS_DIR "/" PLUGINS_CATEGORY_DECODER;
	case plc_plugin_category_ui:
		return PLUGINS_DIR "/" PLUGINS_CATEGORY_UI;
	default:
		assert(0);
		return PLUGINS_DIR "/Unknown";
	}
}

char *plc_plugin_category_get_abs_dir(enum plc_plugin_category category)
{
	char *plugin_category_abs_dir = NULL;
	// Note: it is assumed that applications are within the 'applications' folder
	//	This way plugins should go up two levels
	char *app_abs_dir = plc_application_get_abs_dir();
	asprintf(&plugin_category_abs_dir, "%s/../../%s", app_abs_dir,
			plc_plugin_category_get_rel_dir(category));
	free(app_abs_dir);
	return plugin_category_abs_dir;
}

ATTR_EXTERN char *plc_plugin_get_abs_path(enum plc_plugin_category category,
		const char *plugin_name)
{
	char *plugin_abs_path = NULL;
	char *plugin_category_abs_dir = plc_plugin_category_get_abs_dir(category);
	asprintf(&plugin_abs_path, "%s/%s/%s.so", plugin_category_abs_dir, plugin_name, plugin_name);
	free(plugin_category_abs_dir);
	return plugin_abs_path;
}

void add_plugins(struct plc_plugin_list *plc_plugin_list, const char *plugins_dir)
{
	DIR *dir = opendir(plugins_dir);
	if (dir)
	{
		struct dirent *entry;
		while ((plc_plugin_list->active_index < plc_plugin_list->list_count)
				&& ((entry = readdir(dir)) != NULL))
		{
			if (entry->d_type == DT_DIR)
			{
				if ((strcmp(entry->d_name, ".") != 0) && (strcmp(entry->d_name, "..") != 0))
				{
					char *plugins_subdir;
					asprintf(&plugins_subdir, "%s/%s", plugins_dir, entry->d_name);
					add_plugins(plc_plugin_list, plugins_subdir);
					free(plugins_subdir);
				}
			}
			else
			{
				const char *d_name_ext = strrchr(entry->d_name, '.');
				if (strcmp(d_name_ext, ".so") == 0)
					plc_plugin_list->list[plc_plugin_list->active_index++] = strndup(entry->d_name,
							d_name_ext - entry->d_name);
			}
		}
		closedir(dir);
	}
}

// PRECONDITION: 'plc_plugin_list' musn't have any allocated memory or a memory leak will occur
ATTR_EXTERN struct plc_plugin_list *plc_plugin_list_create(enum plc_plugin_category category)
{
	struct plc_plugin_list *plc_plugin_list = calloc(1, sizeof(*plc_plugin_list));
	// For simplicity just allocate a long-enough vector to accomodate all the possible plugins
	plc_plugin_list->list_count = PLUGINS_MAX_ITEMS;
	plc_plugin_list->list = malloc(PLUGINS_MAX_ITEMS * sizeof(char*));
	plc_plugin_list->active_index = 0;
	char *plugins_category_abs_dir = plc_plugin_category_get_abs_dir(category);
	add_plugins(plc_plugin_list, plugins_category_abs_dir);
	free(plugins_category_abs_dir);
	plc_plugin_list->list_count = plc_plugin_list->active_index;
	plc_plugin_list->active_index = 0;
	// Truncate the vector releasing the unused memory
	plc_plugin_list->list = realloc(plc_plugin_list->list,
			plc_plugin_list->list_count * sizeof(char*));
	return plc_plugin_list;
}

ATTR_EXTERN void plc_plugin_list_release(struct plc_plugin_list *plc_plugin_list)
{
	uint32_t i;
	for (i = 0; i < plc_plugin_list->list_count; i++)
		free(plc_plugin_list->list[i]);
	free(plc_plugin_list->list);
	free(plc_plugin_list);
}

ATTR_EXTERN int plc_plugin_list_find_name(struct plc_plugin_list *plc_plugin_list,
		const char *plugin_name)
{
	uint32_t i;
	for (i = 0; i < plc_plugin_list->list_count; i++)
		// Allow for use-case mismatching
		if (strcasecmp(plc_plugin_list->list[i], plugin_name) == 0)
			return i;
	return -1;
}

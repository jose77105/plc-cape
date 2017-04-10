/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include <dlfcn.h>		// dlsym
#include "+common/api/+base.h"
#include "common.h"
#include "singletons_provider.h"
#include "plugins.h"

struct plugin
{
	void *so_handle;
	plugin_api_load_t api_load;
	plugin_api_unload_t api_unload;
	void *api;
};

void *load_dl_function(void *so_handle, const char *function_name, int mandatory)
{
	void (*fn)() = (void*)dlsym(so_handle, function_name);
	if (mandatory)
	{
		const char *error;
		if ((error = dlerror()))
		log_and_exit(error);
		assert(fn);
	}
	return fn;
}

static void try_set_singletons_provider(void *so_handle)
{
	// Check for optional 'singletons_provider' management and provide it if accepted by the plugin
	plugin_set_singletons_provider_t plugin_set_singletons_provider = 
		load_dl_function(so_handle, PLUGIN_API_SET_SINGLETON_PROVIDER_STRING, 0);
	if (plugin_set_singletons_provider)
		singletons_provider_configure_plugin(plugin_set_singletons_provider);
}

struct plugin *load_plugin(const char *path, void **api, uint32_t *api_version, uint32_t *api_size)
{
	struct plugin *plugin = calloc(1, sizeof(*plugin));
	plugin->so_handle = dlopen(path, RTLD_LAZY);
	if (!plugin->so_handle)
		log_and_exit(dlerror());
	// Clear any existing error
	dlerror();
	try_set_singletons_provider(plugin->so_handle);
	plugin->api_load = load_dl_function(plugin->so_handle, PLUGIN_API_LOAD_STRING, 1);
	plugin->api_unload = load_dl_function(plugin->so_handle, PLUGIN_API_UNLOAD_STRING, 1);
	plugin->api = plugin->api_load(api_version, api_size);
	// Check that all the functions have been filled by the plugin
	int functions_count = *api_size / sizeof(void (*)());
	void **fn_ptr = plugin->api;
	for (; functions_count > 0; functions_count--, fn_ptr++)
		if (*fn_ptr == NULL)
			log_and_exit("Invalid plugin: some function of the interface is NULL");
	*api = plugin->api;
	return plugin;
}

void unload_plugin(struct plugin *plugin)
{
	plugin->api_unload(plugin->api);
	dlclose(plugin->so_handle);
	free(plugin);
}

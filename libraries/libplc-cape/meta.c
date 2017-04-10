/**
 * @file
 * 
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include "+common/api/+base.h"
#include "api/meta.h"
#include "error.h"
#include "logger.h"

// Connection with the 'singletons_provider'
static singletons_provider_get_t singletons_provider_get = NULL;
static singletons_provider_h singletons_provider_handle = NULL;

ATTR_EXTERN void plc_libcape_set_singletons_provider(singletons_provider_get_t callback,
		singletons_provider_h handle_arg)
{
	singletons_provider_get = callback;
	singletons_provider_handle = handle_arg;
	// Ask for the required callbacks
	uint32_t version;
	void *api;
	void *handle;
	singletons_provider_get(singletons_provider_handle,
		singleton_id_error, &api, &handle, &version);
	if (api)
	{
		assert(version >= 1);
		libplc_cape_error_initialize(api, handle);
	}
	singletons_provider_get(singletons_provider_handle,
		singleton_id_logger, &api, &handle, &version);
	if (api)
	{
		assert(version >= 1);
		libplc_cape_logger_initialize(api, handle);
	}
}

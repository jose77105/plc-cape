/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include "+common/api/+base.h"
// Declaration of 'plc_logger_api_h' to avoid hard-castings
typedef struct logger* plc_logger_api_h;
#define PLC_LOGGER_API_HANDLE_EXPLICIT_DEF
#include "+common/api/logger.h"
#include "common.h"
#include "error.h"
#include "libraries/libplc-adc/api/meta.h"
#include "libraries/libplc-cape/api/meta.h"
#include "singletons_provider.h"

// 'singletons_provider_handle' not required in this application -> NULL
singletons_provider_h singletons_provider_handle = NULL;
static struct plc_logger_api logger_api =
{ logger_log_line, logger_log_sequence, logger_log_sequence_format, logger_log_sequence_format_va };

// Forward declarations
int singletons_provider_get(singletons_provider_h handle, enum singleton_id_enum interface_id,
		void **interface_vtbl, void **interface_handle, uint32_t *interface_version);

void singletons_provider_initialize(void)
{
	// Set the singleton provider to all the libararies accepting it
	plc_libcape_set_singletons_provider(singletons_provider_get, singletons_provider_handle);
	plc_libadc_set_singletons_provider(singletons_provider_get, singletons_provider_handle);
}

void singletons_provider_configure_plugin(plugin_set_singletons_provider_t
		plugin_set_singletons_provider)
{
	plugin_set_singletons_provider(singletons_provider_get, singletons_provider_handle);
}

int singletons_provider_get(singletons_provider_h handle, enum singleton_id_enum interface_id,
		void **interface_vtbl, void **interface_handle, uint32_t *interface_version)
{
	switch (interface_id)
	{
	case singleton_id_error:
		*interface_vtbl = error_ctrl_get_interface();
		*interface_handle = NULL;
		*interface_version = 1;
		break;
	case singleton_id_logger:
		*interface_vtbl = &logger_api;
		*interface_handle = logger;
		*interface_version = 1;
		break;
	}
	return 0;
}

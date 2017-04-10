/**
 * @file
 * 
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include "+common/api/+base.h"
#include "+common/api/logger.h"
#include "api/meta.h"

// Connection with the 'singletons_provider'
static singletons_provider_get_t singletons_provider_get = NULL;
static singletons_provider_h singletons_provider_handle = NULL;

ATTR_INTERN struct plc_logger_api *plc_adc_logger_api = NULL;
ATTR_INTERN void *plc_adc_logger_handle = NULL;

ATTR_EXTERN void plc_libadc_set_singletons_provider(singletons_provider_get_t callback,
		singletons_provider_h handle_arg)
{
	singletons_provider_get = callback;
	singletons_provider_handle = handle_arg;
	// Ask for the required callbacks
	uint32_t version;
	singletons_provider_get(singletons_provider_handle,
		singleton_id_logger, (void**)&plc_adc_logger_api, &plc_adc_logger_handle, &version);
	assert(!plc_adc_logger_api || (version >= 1));
}

ATTR_INTERN void plc_libadc_log_line(const char *msg)
{
	if (plc_adc_logger_api)
		plc_adc_logger_api->log_line(plc_adc_logger_handle, msg);
}

/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include "+common/api/+base.h"
#include "libraries/libplc-cape/api/cape.h"
#include "plugin_afe.h"

// TODO: The operations in this file are AFE-based. They should be put in a plugin. When so, remove
//	the dependency with the 'libplc-cape' library

#define PLC_DRIVERS 1

const char *afe_filtering_enum_text[] = {
	"CENELEC B/C/D", "CENELEC A" };

Plugin_afe::Plugin_afe()
{
	plc_cape = plc_cape_create(PLC_DRIVERS);
	plc_afe = plc_cape_get_afe(plc_cape);
	plc_afe_activate_blocks(plc_afe, (afe_block_enum) (afe_block_rx | afe_block_ref2));
	gain_rx_pga1 = afe_gain_rx_pga1_025;
	gain_rx_pga2 = afe_gain_rx_pga2_1;
	plc_afe_set_gains_rx(plc_afe, gain_rx_pga1, gain_rx_pga2);
}

Plugin_afe::~Plugin_afe()
{
	plc_cape_release(plc_cape);
}

int Plugin_afe::execute_command(uint32_t command_id, void *params, void **response)
{
	if (response)
		*response = NULL;
	switch (command_id)
	{
	case command_plc_afe_is_present:
	{
		assert(response);
		*response = malloc(sizeof(int));
		*(int*) *response = !plc_cape_in_emulation(plc_cape);
		return 1;
	}
	case command_plc_afe_get_rx_pga1_gain:
	{
		*(enum afe_gain_rx_pga1_enum*) params = gain_rx_pga1;
		return 1;
	}
	case command_plc_afe_set_rx_pga1_gain:
	{
		enum afe_gain_rx_pga1_enum gain_rx_pga1_new = (enum afe_gain_rx_pga1_enum) (uint32_t) params;
		if (gain_rx_pga1_new >= afe_gain_rx_pga1_COUNT)
			break;
		plc_afe_set_gains_rx(plc_afe, gain_rx_pga1_new, gain_rx_pga2);
		gain_rx_pga1 = gain_rx_pga1_new;
		return 1;
	}
	case command_plc_afe_set_rx_pga2_gain:
	{
		enum afe_gain_rx_pga2_enum gain_rx_pga2_new = (enum afe_gain_rx_pga2_enum) (uint32_t) params;
		if (gain_rx_pga2_new >= afe_gain_rx_pga2_COUNT)
			break;
		plc_afe_set_gains_rx(plc_afe, gain_rx_pga1, gain_rx_pga2_new);
		gain_rx_pga2 = gain_rx_pga2_new;
		return 1;
	}
	case command_plc_afe_set_filtering_band:
	{
		struct afe_settings settings;
		settings.cenelec_a = (uint32_t) params;
		settings.gain_tx_pga = afe_gain_tx_pga_025;
		settings.gain_rx_pga1 = gain_rx_pga1;
		settings.gain_rx_pga2 = gain_rx_pga2;
		plc_afe_configure(plc_afe, &settings);
		return 1;
	}
	default:
		break;
	}
	// TODO: Set errno
	return 0;
}

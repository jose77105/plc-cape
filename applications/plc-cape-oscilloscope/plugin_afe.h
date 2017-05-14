/**
 * @file
 * @brief	Plugin implementing the extended functionalities given by the PlcCape AFE-based board
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef PLUGIN_AFE_H
#define PLUGIN_AFE_H

#include "libraries/libplc-cape/api/afe.h"

enum command_id_enum
{
	command_none = 0,
	command_plc_afe_is_present,				// response: (int) 0..1
	command_plc_afe_get_rx_pga1_gain,		// params: (enum afe_gain_rx_pga1_enum*)
	command_plc_afe_set_rx_pga1_gain,		// params: (enum afe_gain_rx_pga1_enum)
	command_plc_afe_get_rx_pga2_gain,		// params: (enum afe_gain_rx_pga1_enum*)
	command_plc_afe_set_rx_pga2_gain,		// params: (enum afe_gain_rx_pga1_enum)
	command_plc_afe_set_filtering_band,		// params: (uint32_t) 0 for CENELEC B/C/D, 1 for CENELEC A
};

extern const char *afe_filtering_enum_text[];
enum afe_filtering_enum
{
	afe_filtering_cenelec_bcd = 0,
	afe_filtering_cenelec_a,
	afe_filtering_COUNT
};

class Plugin_afe
{
public:
	Plugin_afe();
	virtual ~Plugin_afe();
	int execute_command(uint32_t command_id, void *params, void **response);

private:
	struct plc_cape *plc_cape;
	struct plc_afe *plc_afe;
	enum afe_gain_rx_pga1_enum gain_rx_pga1;
	enum afe_gain_rx_pga2_enum gain_rx_pga2;
};

#endif /* PLUGIN_AFE_H */

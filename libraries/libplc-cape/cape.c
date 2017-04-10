/**
 * @file
 * @brief	**Main** file
 *
 * @see		@ref library-libplc-cape
 *
 * @cond COPYRIGHT_NOTES
 *
 * ##LICENSE
 *
 *		This file is part of plc-cape project.
 *
 *		plc-cape project is free software: you can redistribute it and/or modify
 *		it under the terms of the GNU General Public License as published by
 *		the Free Software Foundation, either version 3 of the License, or
 *		(at your option) any later version.
 *
 *		plc-cape project is distributed in the hope that it will be useful,
 *		but WITHOUT ANY WARRANTY; without even the implied warranty of
 *		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *		GNU General Public License for more details.
 *
 *		You should have received a copy of the GNU General Public License
 *		along with plc-cape project.  If not, see <http://www.gnu.org/licenses/>. 
 *
 * @copyright
 *	Copyright (C) 2016 Jose Maria Ortega
 * 
 * @endcond
 */

#include <errno.h>		// errno
#include "+common/api/+base.h"
#include "afe.h"
#include "api/afe.h"
#include "api/cape.h"
#include "api/leds.h"
#include "error.h"
#include "leds.h"
#include "libraries/libplc-gpio/api/gpio.h"
#include "spi.h"

struct plc_cape
{
	// int plc_cape_version;
	struct plc_afe *plc_afe;
	struct spi *spi;
	struct plc_leds *plc_leds;
	struct plc_gpio *plc_gpio;
};

// TODO: Remove global variable required by 'leds.c'
ATTR_EXTERN int plc_cape_emulation = 0;
ATTR_EXTERN int plc_cape_version = 1;

ATTR_EXTERN struct plc_cape *plc_cape_create(int plc_driver)
{
	struct plc_cape *plc_cape = (struct plc_cape*) calloc(1, sizeof(struct plc_cape));

	// TODO: Buscar un sistema mejor para detección de BBB
	// Note: In theory '&>' can be used to redirect all the outputs but it seems to don't work with
	//	this 'system' command
	//	So, do it per both outputs, standard and error
	// Note: The BBB accepts both '/dev/nul' and '/dev/null', but the std linux on the deskop
	//	accepts only '/dev/null'
	plc_cape_emulation =
			(system("ls /sys/devices/bone_capemgr.*/slots > /dev/null 2> /dev/null") == 0) ? 0 : 1;
	plc_gpio_set_soft_emulation(plc_cape_emulation);

	// Get the running PlcCape version depending on the DTBO loaded
	plc_cape_version = 1;
	if (!plc_cape_emulation
			&& system("grep PLC_CAPE_V2 /sys/devices/bone_capemgr.*/slots > /dev/null 2> /dev/null")
					== 0)
		plc_cape_version = 2;

	int res_ok = 0;
	plc_cape->plc_gpio = plc_gpio_create();
	// Double parentheses to avoid a compiler warning
	if (plc_cape->plc_gpio)
	{
		plc_cape->plc_leds = plc_leds_create(plc_cape->plc_gpio);
		if (plc_cape->plc_leds)
		{
			plc_cape->spi = spi_create(plc_driver, plc_cape->plc_gpio);
			if (plc_cape->spi)
			{
				plc_cape->plc_afe = plc_afe_create(plc_cape->plc_gpio, plc_cape->spi);
				if (plc_cape->plc_afe)
				{
					res_ok = 1;
				}
				else
				{
					libplc_cape_set_error_msg_errno("Error at AFE initialization");
				}
			}
			else
			{
				libplc_cape_set_error_msg_errno("Error at SPI initialization");
			}
		}
		else
		{
			libplc_cape_set_error_msg_errno("Error at LEDS initialization");
		}
	}
	else
	{
		libplc_cape_set_error_msg_errno("Error at GPIO initialization");
	}
	if (!res_ok)
	{
		if (plc_cape->plc_gpio)
			plc_gpio_release(plc_cape->plc_gpio);
		if (plc_cape->plc_leds)
			plc_leds_release(plc_cape->plc_leds);
		if (plc_cape->spi)
			spi_release(plc_cape->spi);
		if (plc_cape->plc_afe)
			plc_afe_release(plc_cape->plc_afe);
		free(plc_cape);
		return NULL;
	}
	return plc_cape;
}

ATTR_EXTERN void plc_cape_release(struct plc_cape *plc_cape)
{
	plc_afe_release(plc_cape->plc_afe);
	spi_release(plc_cape->spi);
	plc_leds_release(plc_cape->plc_leds);
	plc_gpio_release(plc_cape->plc_gpio);
	free(plc_cape);
}

ATTR_EXTERN struct plc_afe *plc_cape_get_afe(struct plc_cape *plc_cape)
{
	return plc_cape->plc_afe;
}

ATTR_EXTERN struct plc_leds *plc_cape_get_leds(struct plc_cape *plc_cape)
{
	return plc_cape->plc_leds;
}

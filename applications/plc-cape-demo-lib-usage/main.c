/**
 * @file
 * @brief	**Main** file
 *
 * @see		@ref application-plc-cape-demo-lib-usage
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

#include <unistd.h>		// usleep
#include "+common/api/+base.h"
#include "libraries/libplc-tools/api/time.h"

int main(void)
{
	uint32_t tick_ini = plc_time_get_tick_ms();
	usleep(10000);
	uint32_t tick_end = plc_time_get_tick_ms();
	printf("'usleep' executed in %d ms\n", tick_end - tick_ini);
	return 0;
}

/**
 * @file
 * @brief	**Main** file
 *
 * @see		@ref application-plc-cape-oscilloscope
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
 *	Copyright (C) 2017 Jose Maria Ortega
 * 
 * @endcond
 */

#include <locale.h>			// setlocale
#include <glib/gprintf.h>	// g_vprintf
#include "+common/api/+base.h"
#include "configuration.h"
#include "application.h"

#ifdef DEBUG
void plc_trace_gprintf(const char *function_name, const char *format, ...)
{
	// For debugging in gtk we can use g_log, g_debug, g_print, g_printf, etc.
	va_list args;
	va_start(args, format);
	// Prefix the logging message with the function name
	g_printf("%s: ", function_name);
	g_vprintf(format, args);
	// Add a new line
	g_printf("\n");
	va_end(args);
}

// Tune 'plc_debug_level' according to the development stage:
//	* '3' is the most verbose for bug finding, showing all the traces in the code (1, 2 and 3)
//	* '2' is the usual choice in debugging because shows interesting but not excessive messages
//	* '1' is the usual choice for normal operation because only shows critical messages
int plc_debug_level = 3;
void (*plc_trace)(const char *function_name, const char *format, ...) = plc_trace_gprintf;

#endif

class Configuration *app_cfg = NULL;

int main(int argc, char **argv)
{
	// 'gtk_init' exits if not GUI available. Use 'gtk_init_check for proper checking
	if (gtk_init_check(&argc, &argv) != TRUE)
	{
		fputs("GUI cannot be started. Aborted!\n", stderr);
		return EXIT_FAILURE;
	}
	// Note: 'gtk_init_check' sets the locale to the user's locale
	// The locale is used for example in printf("%f") or scanf("%f") to properly interpret the
	// decimal point. In English it is the dot '.'; in Spanish it is the comma ','.
	//	g_print("Test set_locale representation for floats: %f\n", 1.5f);
	// To avoid having different configuration files according to the user's locale set it to POSIX
	char *prev_locale = strdup(setlocale(LC_NUMERIC, NULL));
	char *locale = setlocale(LC_NUMERIC, "POSIX");
	assert(locale != NULL);
	app_cfg = new Configuration();
	Application *app_gui = new Application();
	int status = app_gui->do_menu_loop(argc, argv);
	delete app_gui;
	delete app_cfg;
	locale = setlocale(LC_NUMERIC, prev_locale);
	assert(locale != NULL);
	return status;
}

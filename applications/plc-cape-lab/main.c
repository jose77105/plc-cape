/**
 * @file
 * @brief	**Main** file
 *
 * @see		@ref application-plc-cape-lab
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

#include <bits/signum.h>	// SIG_ERR
#include <signal.h>
#include <unistd.h>			// sleep, readlink
#include "+common/api/+base.h"
#include "common.h"
#include "controller.h"
#include "singletons_provider.h"
#include "libraries/libplc-cape/api/meta.h"
#include "libraries/libplc-tools/api/application.h"
#include "libraries/libplc-tools/api/time.h"
#include "settings.h"
#include "ui.h"

#ifdef DEBUG
// Tune 'plc_debug_level' according to the development stage:
//	* '3' is the most verbose for bug finding, showing all the traces in the code (1, 2 and 3)
//	* '2' is the usual choice in debugging because shows interesting but not excessive messages
//	* '1' is the usual choice for normal operation because only shows critical messages
int plc_debug_level = 2;
#endif

static struct settings *settings = NULL;
static struct ui *ui = NULL;
// 'logger' is a global object publicly accessible
struct logger *logger = NULL;

static void terminate(void);

// NOTE: calling 'exit' (even from the signal) will invoke 'terminate' for proper clean-up
static void sigterm_handler(int signo)
{
	log_line("SIGTERM received");
	exit(EXIT_SUCCESS);
}

static void sigint_handler(int signo)
{
	log_line("SIGINT received");
	exit(EXIT_SUCCESS);
}

#ifndef DEBUG
static void sigsegv_handler(int signo)
{
	log_line("SIGSEGV received");
	exit(EXIT_FAILURE);
}
#endif

int main(int argc, char *argv[])
{
	TRACE(3, "Application started");
	int n;
	for (n = 1; n < argc; n++)
		if (strcmp(argv[n], "--help") == 0)
		{
			fprintf(stderr, settings_get_usage_message());
			return 0;
		}

	// For proper clean-up (e.g. stopping DMA in progress) capture most typical signals:
	//	* SIGTERM: triggerd by a KILL request
	//	* SIGINT: triggerd by CTRL+C
	//	* SIGSEGV: triggered when a segmentation violation (a typical error in development phase)
	// For a detailed overview of 'signals' refer to 'man 7 signals' or here:
	//	http://man7.org/linux/man-pages/man7/signal.7.html
	if (signal(SIGTERM, sigterm_handler) == SIG_ERR)
		log_errno_and_exit("Cannot register the SIGTERM handler");

	if (signal(SIGINT, sigint_handler) == SIG_ERR)
		log_errno_and_exit("Cannot register the SIGINT handler");

#ifndef DEBUG
	// In the release version capture the 'Segmentation Faults' for proper clean-up when possible
	// This doesn't fix the problem but at least closes the application gracefully
	// In debug version it's more convenient to not capture them to simplify the analsys with a
	// debugger (e.g. GDB) as it breaks in the controversial point
	if (signal(SIGSEGV, sigsegv_handler) == SIG_ERR)
		log_errno_and_exit("Cannot register the SIGSEGV handler");
#endif

	atexit(terminate);

	TRACE(3, "Initializing main objects");
	error_ctrl_initialize();

	settings = settings_create();
	char *error_msg;

	settings_parse_args(settings, argc, argv, &error_msg);
	if (error_msg != NULL)
	{
		log_line(error_msg);
		free(error_msg);
		log_line(settings_get_usage_message());
		exit(EXIT_FAILURE);
	}

	TRACE(3, "Initializing UI");
	ui = ui_create(settings->ui_plugin_name, argc, argv, settings);
	assert(ui);

	logger = logger_create_log_text((void*) ui_log_text, ui);
	singletons_provider_initialize();

	TRACE(3, "Initializing controller");
	controller_create(settings, ui);

	if (settings->autostart)
	{
		controller_activate_async();
		// If autostart + max_duration -> close the application after the test
		if (settings->communication_timeout_ms > 0)
			return 0;
	}

	log_line("Ready. Select an option from the menu");
	ui_do_menu_loop(ui);

	// NOTE: A explicit call to 'terminate()' is not required as it is done on exit the app because
	//	the 'atexit'. If we explicitly invoke it the 'terminate' function will be called twice

	// NOTE: 'return 0' is equivalent to 'exit(EXIT_SUCCESS)'
	return 0;
}

static void terminate(void)
{
	TRACE(3, "Terminating application");

	__sighandler_t sig_res = signal(SIGINT, SIG_DFL);
	assert(sig_res != SIG_ERR);

	sig_res = signal(SIGTERM, SIG_DFL);
	assert(sig_res != SIG_ERR);

	TRACE(3, "Releasing controller");
	controller_release();

	TRACE(3, "Releasing objects");
	if (settings)
	{
		settings_release(settings);
		settings = NULL;
	}
	TRACE(3, "Releasing UI");
#ifdef DEBUG
	log_line("Press a key to close UI...");
	getchar();
#endif
	// As the logger depends on the 'ui' released it prior to 'ui_release'
	if (logger)
	{
		logger_release(logger);
		logger = NULL;
	}
	if (ui)
	{
		ui_release(ui);
		ui = NULL;
	}
	TRACE(3, "UI released");
	// Release global shared objects at the end
	error_ctrl_terminate();
	TRACE(3, "Application terminated");
}

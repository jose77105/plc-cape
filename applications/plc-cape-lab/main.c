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
 *	plc-cape-lab - A laboratory tool to interact with PlcCape boards
 *	Copyright (C) 2016-2017 Jose Maria Ortega
 * 
 * @endcond
 */

#include <signal.h>			// SIG_ERR
#include "+common/api/+base.h"
#include "cmdline.h"
#include "common.h"
#include "controller.h"

#ifdef DEBUG
// Tune 'plc_debug_level' according to the development stage:
//	* '3' is the most verbose for bug finding, showing all the traces in the code (1, 2 and 3)
//	* '2' is the usual choice in debugging because shows interesting but not excessive messages
//	* '1' is the usual choice for normal operation because only shows critical messages
int plc_debug_level = 2;
#endif

static void terminate(void);

// NOTE: calling 'exit' (even from the signal) will invoke 'terminate' for proper clean-up
static void sigterm_handler(int signo)
{
	log_line_and_exit("SIGTERM received");
}

static void sigint_handler(int signo)
{
	log_line_and_exit("SIGINT received");
}

static void sigabrt_handler(int signo)
{
	log_line_and_exit("SIGABORT received");
}

#ifndef DEBUG
static void sigsegv_handler(int signo)
{
	log_line_and_exit("SIGSEGV received");
}
#endif

int main(int argc, char *argv[])
{
	TRACE(3, "Application started");
	int n;
	for (n = 1; n < argc; n++)
		if (strcmp(argv[n], "--help") == 0)
		{
			fputs(cmdline_get_usage_message(), stderr);
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

#ifdef DEBUG
	if (signal(SIGABRT, sigabrt_handler) == SIG_ERR)
		log_errno_and_exit("Cannot register the SIGABRT handler");
#else
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

	TRACE(3, "Initializing controller");
	controller_create(argc, argv);

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

	// Release global shared objects at the end
	error_ctrl_terminate();
	TRACE(3, "Application terminated");
}

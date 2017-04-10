/**
 * @file
 * 
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include <termios.h>		// ICANON
#include <unistd.h>			// STDIN_FILENO
#include "+common/api/+base.h"
#include "api/terminal_io.h"

struct plc_terminal_io
{
	struct termios prev_settings;
	struct termios new_settings;
};

ATTR_EXTERN struct plc_terminal_io *plc_terminal_io_create(void)
{
	struct plc_terminal_io *plc_terminal_io = calloc(1, sizeof(struct plc_terminal_io));
	tcgetattr(STDIN_FILENO, &plc_terminal_io->prev_settings);
	plc_terminal_io->new_settings = plc_terminal_io->prev_settings;
	// ICANON makes 'getchar' to return at '\n', EOF or EOL
	plc_terminal_io->new_settings.c_lflag &= ~(ICANON);
	// TCSANOW updates attributes
	tcsetattr(STDIN_FILENO, TCSANOW, &plc_terminal_io->new_settings);
	return plc_terminal_io;
}

ATTR_EXTERN void plc_terminal_io_release(struct plc_terminal_io *plc_terminal_io)
{
	// Restore terminal settings
	tcsetattr(STDIN_FILENO, TCSANOW, &plc_terminal_io->prev_settings);
	free(plc_terminal_io);
}

ATTR_EXTERN int plc_terminal_io_kbhit(struct plc_terminal_io *plc_terminal_io)
{
	struct timeval tv;
	fd_set fs;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	FD_ZERO(&fs);
	FD_SET(STDIN_FILENO, &fs);
	select(STDIN_FILENO + 1, &fs, NULL, NULL, &tv);
	if (FD_ISSET(STDIN_FILENO, &fs) == 0)
	{
		return 0;
	}
	else
	{
		// 'getchar' flushes the character detected in the 'fs'
		return getchar();
	}
}

ATTR_EXTERN int plc_terminal_io_getchar(struct plc_terminal_io *plc_terminal_io)
{
	return getchar();
}

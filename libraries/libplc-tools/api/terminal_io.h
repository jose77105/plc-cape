/**
 * @file
 * @brief	Extended input/output functionality in terminal consoles, as checking for key pressing
 *			in non-blocking mode
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef LIBPLC_TOOLS_TERMINAL_IO_H
#define LIBPLC_TOOLS_TERMINAL_IO_H

#ifdef __cplusplus
extern "C" {
#endif

struct plc_terminal_io;

/**
 * @brief	Creates a new object handler
 * @return	Pointer to the handler object
 */
struct plc_terminal_io *plc_terminal_io_create(void);
/**
 * @brief	Releases a handler object
 * @param	plc_terminal_io	Pointer to the handler object
 */
void plc_terminal_io_release(struct plc_terminal_io *plc_terminal_io);
/**
 * @brief	**Checks** if a key has been pressed
 * @param	plc_terminal_io	Pointer to the handler object
 * @return	If no key has been pressed it returns 0 immediately
			If a key has been pressed it returns the character code
 */
int plc_terminal_io_kbhit(struct plc_terminal_io *plc_terminal_io);
/**
 * @brief	**Waits** for a key to be pressed
 * @param	plc_terminal_io	Pointer to the handler object
 * @return	Character code of the key pressed
 */
int plc_terminal_io_getchar(struct plc_terminal_io *plc_terminal_io);

#ifdef __cplusplus
}
#endif

#endif /* LIBPLC_TOOLS_TERMINAL_IO_H */

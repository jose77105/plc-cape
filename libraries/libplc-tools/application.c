/**
 * @file
 * 
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include <libgen.h>		// dirname
#include <unistd.h>		// readlink
#include "+common/api/+base.h"
#include "api/application.h"

ATTR_EXTERN char *plc_application_get_abs_path(void)
{
	char *path = malloc(PLC_MAX_PATH);
	ssize_t chars_written = readlink("/proc/self/exe", path, PLC_MAX_PATH);
	assert(chars_written < PLC_MAX_PATH);
	path[chars_written] = '\0';
	path = realloc(path, chars_written+1);
	return path;
}

ATTR_EXTERN char *plc_application_get_abs_dir(void)
{
	char *path = plc_application_get_abs_path();
	char *dir = strdup(dirname(path));
	free(path);
	return dir;
}

ATTR_EXTERN char *plc_application_get_output_abs_dir(void)
{
	return plc_application_get_abs_dir();
}
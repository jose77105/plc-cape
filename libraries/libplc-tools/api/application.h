/**
 * @file
 * @brief	Helper functions related to the management of applications
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef LIBPLC_TOOLS_APPLICATION_H
#define LIBPLC_TOOLS_APPLICATION_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief	Gets the absolute full path of the running application
 * @return	A pointer to the path. Release it with _free_ when no longer required
 */
char *plc_application_get_abs_path(void);
/**
 * @brief	Gets the absolute base directory of the running application
 * @return	A pointer to the base directory. Release it with _free_ when no longer required
 */
char *plc_application_get_abs_dir(void);
/**
 * @brief	Gets the absolute directory where the output results (if any) will be stored
 * @return	A pointer to the output directory. Release it with _free_ when no longer required
 */
char *plc_application_get_output_abs_dir(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBPLC_TOOLS_APPLICATION_H */

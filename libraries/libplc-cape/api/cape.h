/**
 * @file
 * @brief	_PlcCape_ board management (main **entry-point**)
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef LIBPLC_CAPE_PLC_CAPE_H
#define LIBPLC_CAPE_PLC_CAPE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief	Creates the main object to interact with a _PlcCape_ board
 * @param	plc_driver	Indicates the driver to use:
 *						* **0**: standard SPI & ADC drivers
 *						* **1**: customized SPI & ADC driver, required for maximum performance
 * @return	A pointer to the created object
 */
struct plc_cape *plc_cape_create(int plc_driver);
/**
 * @brief	Release a _plc_cape_ object
 * @param	plc_cape	Pointer to the handler object
 */
void plc_cape_release(struct plc_cape *plc_cape);
/**
 * @brief	Gets access to the child object encapsulating the AFE031 functionality
 * @param	plc_cape	Pointer to the handler object
 * @return	A pointer to the _plc_afe_ child object
 */
struct plc_afe *plc_cape_get_afe(struct plc_cape *plc_cape);
/**
 * @brief	Gets access to the child object encapsulating the access to LEDs
 * @param	plc_cape	Pointer to the handler object
 * @return	A pointer to the _plc_leds_ child object
 */
struct plc_leds *plc_cape_get_leds(struct plc_cape *plc_cape);
/**
 * @brief	Indicates if the _plc_cape_ library is working in normal or emulated mode
 * @param	plc_cape	Pointer to the handler object
 * @return	1 if the library is working in emulated mode, 0 otherwise
 */
int plc_cape_in_emulation(struct plc_cape *plc_cape);

#ifdef __cplusplus
}
#endif

#endif /* LIBPLC_CAPE_PLC_CAPE_H */

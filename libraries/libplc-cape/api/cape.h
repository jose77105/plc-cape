/**
 * @file
 * @brief	_PlcCape_ board management (main **entry-point**)
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef LIBPLC_CAPE_PLC_CAPE_H
#define LIBPLC_CAPE_PLC_CAPE_H

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
 */
struct plc_afe *plc_cape_get_afe(struct plc_cape *plc_cape);
/**
 * @brief	Gets access to the child object encapsulating the access to LEDs
 * @param	plc_cape	Pointer to the handler object
 */
struct plc_leds *plc_cape_get_leds(struct plc_cape *plc_cape);

#endif /* LIBPLC_CAPE_PLC_CAPE_H */

/**
 * @file
 * @brief	LEDs control
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef LIBPLC_CAPE_LEDS_H
#define LIBPLC_CAPE_LEDS_H

struct plc_leds;

/**
 * @brief	Toggles the activity LED
 * @details	The activity LED is intended to notify the user that the PlcBoard is not idle nor frozen
 *			Therefore it is recommended to flash the LED by software when it is in communication
 *			mode (ready to TX, TX in progress, ready to RX...)
 * @param	plc_leds	Pointer to the handler object
 */
void plc_leds_toggle_app_activity(struct plc_leds *plc_leds);
/**
 * @brief	Sets the state of the "Activity" LED
 * @param	plc_leds	Pointer to the handler object
 * @param	on			0 to turn it off; 1 to turn it on
 */
void plc_leds_set_app_activity(struct plc_leds *plc_leds, int on);
/**
 * @brief	Sets the state of the "Transmitting data" LED
 * @param	plc_leds	Pointer to the handler object
 * @param	on			0 to turn it off; 1 to turn it on
 */
void plc_leds_set_tx_activity(struct plc_leds *plc_leds, int on);
/**
 * @brief	Sets the state of the "Receiving data" LED
 * @param	plc_leds	Pointer to the handler object
 * @param	on			0 to turn it off; 1 to turn it on
 */
void plc_leds_set_rx_activity(struct plc_leds *plc_leds, int on);

#endif /* LIBPLC_CAPE_LEDS_H */

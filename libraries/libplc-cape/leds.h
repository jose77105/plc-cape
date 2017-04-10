/**
 * @file
 * @brief	Internal interface for LEDs control
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef LEDS_H
#define LEDS_H

struct plc_gpio;
struct plc_leds;

struct plc_leds *plc_leds_create(struct plc_gpio *plc_gpio);
void plc_leds_release(struct plc_leds *plc_leds);

#endif /* LEDS_H */

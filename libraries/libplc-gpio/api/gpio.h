/**
 * @file
 * @brief	General Purpose Input/Output public functionality
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef LIBPLC_GPIO_GPIO_H
#define LIBPLC_GPIO_GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief	Sets the 'gpio' library to work in normal mode (hardware-based) or in emulation mode
 * @param	soft_emulation	0 for normal mode, 1 for emulation
 */
void plc_gpio_set_soft_emulation(int soft_emulation);

struct plc_gpio;
/**
 * @brief	Creates a new object handler for 'plc_gpio' direct access, through memory mapping
 * @return	Pointer to the handler object
 */
struct plc_gpio *plc_gpio_create(void);
/**
 * @brief	Releases a handler object
 * @param	plc_gpio	Pointer to the handler object
 */
void plc_gpio_release(struct plc_gpio *plc_gpio);

struct plc_gpio_pin_out;
/**
 * @brief	Creates a handler for an output pin accessed through fast direct memory access
 * @param	plc_gpio	The parent 'gpio' manager created with #plc_gpio_create
 * @param	pin_bank	Bank where the GPIO is located in the BBB
 * @param	pin_mask	Mask within the bank identifying the pin
 * @return	Pointer to the handler object
 */
struct plc_gpio_pin_out *plc_gpio_pin_out_create(struct plc_gpio *plc_gpio, uint32_t pin_bank,
		uint32_t pin_mask);
/**
 * @brief	Releases a handler object
 * @param	plc_gpio_pin_out	Pointer to the handler object
 */
void plc_gpio_pin_out_release(struct plc_gpio_pin_out *plc_gpio_pin_out);
/**
 * @brief	Sets the level of a GPIO
 * @param	plc_gpio_pin_out	Pointer to the handler object
 * @param	on				0 to switch off the output, 1 to turn it on
 */
void plc_gpio_pin_out_set(struct plc_gpio_pin_out *plc_gpio_pin_out, int on);
/**
 * @brief	Toggles the logical level of a GPIO
 * @param	plc_gpio_pin_out	Pointer to the handler object
 */
void plc_gpio_pin_out_toggle(struct plc_gpio_pin_out *plc_gpio_pin_out);

struct plc_gpio_pin_in;
/**
 * @brief	Creates a handler for an input pin accessed through fast direct memory access
 * @param	plc_gpio	The parent 'gpio' manager created with #plc_gpio_create
 * @param	pin_bank	Bank where the GPIO is located in the BBB
 * @param	pin_mask	Mask within the bank identifying the pin
 * @return	Pointer to the handler object
 */
struct plc_gpio_pin_in *plc_gpio_pin_in_create(struct plc_gpio *plc_gpio, uint32_t pin_bank,
		uint32_t pin_mask);
/**
 * @brief	Releases a handler object
 * @param	plc_gpio_pin_in	Pointer to the handler object
 */
void plc_gpio_pin_in_release(struct plc_gpio_pin_in *plc_gpio_pin_in);
/**
 * @brief	Retrieves the current state of a GPIO input
 * @param	plc_gpio_pin_in	Pointer to the handler object
 * @return	0 for low level, 1 for high level
 */
int plc_gpio_pin_in_get(struct plc_gpio_pin_in *plc_gpio_pin_in);

struct plc_gpio_sysfs_pin;
/**
 * @brief	Creates a handler for a GPIO using the 'sysfs' file system and underlying drivers
 * @param	gpio_number	Identifying number of the GPIO
 * @param	output		0 to declare the GPIO as as input, 1 to declare it as an output
 * @return	Pointer to the handler object
 */
struct plc_gpio_sysfs_pin *plc_gpio_sysfs_pin_create(uint32_t gpio_number, int output);
/**
 * @brief	Releases a handler object
 * @param	plc_gpio_sysfs_pin	Pointer to the handler object
 */
void plc_gpio_sysfs_pin_release(struct plc_gpio_sysfs_pin *plc_gpio_sysfs_pin);
/**
 * @brief	Retrieves the current state of a GPIO declared as an input
 * @param	plc_gpio_sysfs_pin	Pointer to the handler object
 * @return	0 for low level, 1 for high level
 */
int plc_gpio_sysfs_pin_get(struct plc_gpio_sysfs_pin *plc_gpio_sysfs_pin);
/**
 * @brief	Sets the level of a GPIO declared as an output
 * @param	plc_gpio_sysfs_pin	Pointer to the handler object
 * @param	on				0 to switch off the output, 1 to turn it on
 */
void plc_gpio_sysfs_pin_set(struct plc_gpio_sysfs_pin *plc_gpio_sysfs_pin, int on);
/**
 * @brief	Sets a callback to be notified immediately when a change in a GPIO input happens
 * @param	plc_gpio_sysfs_pin	Pointer to the handler object
 * @param	callback		The callback to be called when the input changes of state
 * @param	data			Optional user data to be included in the first parameter of the callback
 * @details
 *	The callback is called from a different thread which is created when this function is called.
 *	The caller is responsible of putting in place the required contention mechanisms to avoid
 *	troubles if accesing a shared resource
 */
void plc_gpio_sysfs_pin_callback(struct plc_gpio_sysfs_pin *plc_gpio_sysfs_pin,
		void (*callback)(void *data, int flag_status), void *data);

#ifdef __cplusplus
}
#endif

#endif /* LIBPLC_GPIO_GPIO_H */

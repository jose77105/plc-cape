/**
 * @file
 * @brief	Declarations of symbols specific to the _BeagleBone Black_
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef COMMON_BBB_H
#define COMMON_BBB_H

/* 
* On board leds:
* USR0: Heartbeat
* USR1: uSD access
* USR2: CPU activity
* USR3: eMMC access
*/
// User LEDs on GPIO1 bank
// Standard names: USR0 = GPIO1_21 = GPIO_53 (32+21), USR1 = GPIO1_22 = GPIO_54...
#define USR_LED_BANK 1
#define USR0_LED_MASK (1<<21)
#define USR1_LED_MASK (1<<22)
#define USR2_LED_MASK (1<<23)
#define USR3_LED_MASK (1<<24)

// Onboard leds can be also flashed thorugh sysfs:
//	echo 1 > /sys/class/leds/beaglebone\:green\:usr1/brightness

// Example of access to GPIO_P9_12_MASK
// Interactive mapping of GPIO pin to GPIO C symbol in
// * http://eskimon.fr/beaglebone-black-gpio1-interactive-map
// * https://github.com/derekmolloy/boneDeviceTree/blob/master/docs/BeagleboneBlackP9HeaderTable.pdf
// * BBB_SRM.pdf page 72 column 'mode 7'
// * It is also possible to get the GPIOn[] name through a BoneScript:
//		http://beagleboard.org/Support/BoneScript/getPinMode/

// The mask can be obtained from the Offset information as follows:
//	#define GPIO_P9_n_MASK (1 << (index of GPIOn))
//	#define GPIO_P9_n_BANK (0 or 1 depending on GPIOn suffix)
//
// Nomenclature:
//	* GPIO_P9_n: physical pin number in P9 connector
//	* GPIOn[m]: GPIO bank-dependent name with n = pin bank.
//		Used in the Adafruit schematics in the form GPIOn_m
//	* GPIO_n: GPIO bank-indepedent name, used in http://beagleboard.org/support/bone101
//		Other context-base names are also used there, as UART*

#ifdef __cplusplus
extern "C" {
#endif

// GPIO0[30]; GPIO_30 | UART4_RXD
#define GPIO_P9_11_MASK (1<<30)
#define GPIO_P9_11_BANK 0

// GPIO1[28]; GPIO_60
#define GPIO_P9_12_MASK (1<<28)
#define GPIO_P9_12_BANK 1

// GPIO0[31]; GPIO_31 | UART4_TXD1
#define GPIO_P9_13_MASK (1<<31)
#define GPIO_P9_13_BANK 0

// GPIO1[18]; GPIO_50 | EHRPWM1A
#define GPIO_P9_14_NUMBER 50
#define GPIO_P9_14_MASK (1<<18)
#define GPIO_P9_14_BANK 1

// GPIO1[16]; GPIO_48
#define GPIO_P9_15_MASK (1<<16)
#define GPIO_P9_15_BANK 1

// GPIO1[19]; GPIO_51 | EHRPWM1B
#define GPIO_P9_16_NUMBER 51
#define GPIO_P9_16_MASK (1<<19)
#define GPIO_P9_16_BANK 1

// GPIO1[17]; GPIO_49
#define GPIO_P9_23_NUMBER 49
#define GPIO_P9_23_MASK (1<<17)
#define GPIO_P9_23_BANK 1

// GPIO0[15]; GPIO_15 | UART1_TXD
#define GPIO_P9_24_MASK (1<<15)
#define GPIO_P9_24_BANK 0

// GPIO0[14]; GPIO_14 | UART1_RXD
#define GPIO_P9_26_MASK (1<<14)
#define GPIO_P9_26_BANK 0

// GPIO3[17]; GPIO_113 | SPI1_CS0 | ECAPPWM2
// Warning: By deafult the BBB allocates this P9_28 to 'mcasp0_axr2' (mux == 2 mode)
//	So not usable unless explicitly modiying the main DTB:
//		https://groups.google.com/forum/#!topic/beagleboard/YNhtwbe_b4k
#define GPIO_P9_28_NUMBER 113
#define GPIO_P9_28_MASK (1<<17)
#define GPIO_P9_28_BANK 3

#define ADC_MAX_CAPTURE_RATE_SPS 200000.0

#ifdef __cplusplus
}
#endif

#endif /* COMMON_BBB_H */

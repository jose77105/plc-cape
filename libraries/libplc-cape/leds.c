/**
 * @file
 * 
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include <pthread.h>
#include "+common/api/+base.h"
#include "+common/api/bbb.h"
#include "api/leds.h"
#include "leds.h"
#include "libraries/libplc-gpio/api/gpio.h"

/*
 #define ZC_OUT_MASK GPIO_P9_12_MASK
 #define ZC_OUT_BANK GPIO_P9_12_BANK
 */

// PlcCape V1 declarations
#define LED_APP_RUNNING_MASK_V1 USR1_LED_MASK
#define LED_APP_RUNNING_BANK_V1 USR_LED_BANK
#define LED_APP_RUNNING_ON_V1 1
#define LED_TX_MASK_V1 USR3_LED_MASK
#define LED_TX_BANK_V1 USR_LED_BANK
#define LED_RX_MASK_V1 USR3_LED_MASK
#define LED_RX_BANK_V1 USR_LED_BANK

// PlcCape V2 declarations
#define LED_APP_RUNNING_MASK_V2 GPIO_P9_11_MASK
#define LED_APP_RUNNING_BANK_V2 GPIO_P9_11_BANK
// GPIO_P9_11 is active by low-level
#define LED_APP_RUNNING_ON_V2 0
#define LED_TX_MASK_V2 GPIO_P9_13_MASK
#define LED_TX_BANK_V2 GPIO_P9_13_BANK
#define LED_RX_MASK_V2 GPIO_P9_15_MASK
#define LED_RX_BANK_V2 GPIO_P9_15_BANK

struct plc_leds
{
	struct plc_gpio_pin_out *pin_app_running;
	int pin_app_running_level_on;
	struct plc_gpio_pin_out *pin_tx;
	struct plc_gpio_pin_out *pin_rx;
	pthread_mutex_t mutex_activity;
};

ATTR_INTERN struct plc_leds *plc_leds_create(struct plc_gpio *plc_gpio)
{
	// TODO: Move global 'plc_cape_version' to function parameter
	extern int plc_cape_version;
	struct plc_leds *plc_leds = calloc(1, sizeof(struct plc_leds));
	if (plc_cape_version >= 2)
	{
		plc_leds->pin_app_running = plc_gpio_pin_out_create(plc_gpio, LED_APP_RUNNING_BANK_V2,
		LED_APP_RUNNING_MASK_V2);
		plc_leds->pin_app_running_level_on = LED_APP_RUNNING_ON_V2;
		plc_leds->pin_tx = plc_gpio_pin_out_create(plc_gpio, LED_TX_BANK_V2, LED_TX_MASK_V2);
		plc_leds->pin_rx = plc_gpio_pin_out_create(plc_gpio, LED_RX_BANK_V2, LED_RX_MASK_V2);
	}
	else
	{
		plc_leds->pin_app_running = plc_gpio_pin_out_create(plc_gpio, LED_APP_RUNNING_BANK_V1,
		LED_APP_RUNNING_MASK_V1);
		plc_leds->pin_app_running_level_on = LED_APP_RUNNING_ON_V1;
		plc_leds->pin_tx = plc_gpio_pin_out_create(plc_gpio, LED_TX_BANK_V1, LED_TX_MASK_V1);
		plc_leds->pin_rx = plc_gpio_pin_out_create(plc_gpio, LED_RX_BANK_V1, LED_RX_MASK_V1);
	}
	plc_gpio_pin_out_set(plc_leds->pin_app_running, plc_leds->pin_app_running_level_on);
	plc_gpio_pin_out_set(plc_leds->pin_tx, 0);
	plc_gpio_pin_out_set(plc_leds->pin_rx, 0);
	int ret = pthread_mutex_init(&plc_leds->mutex_activity, NULL);
	assert(ret == 0);
	return plc_leds;
}

ATTR_INTERN void plc_leds_release(struct plc_leds *plc_leds)
{
	int ret = pthread_mutex_destroy(&plc_leds->mutex_activity);
	assert(ret == 0);
	plc_gpio_pin_out_set(plc_leds->pin_rx, 0);
	plc_gpio_pin_out_release(plc_leds->pin_rx);
	plc_gpio_pin_out_set(plc_leds->pin_tx, 0);
	plc_gpio_pin_out_release(plc_leds->pin_tx);
	plc_gpio_pin_out_set(plc_leds->pin_app_running, !plc_leds->pin_app_running_level_on);
	plc_gpio_pin_out_release(plc_leds->pin_app_running);
	free(plc_leds);
}

ATTR_EXTERN void plc_leds_toggle_app_activity(struct plc_leds *plc_leds)
{
	// Led access mutexed because can be called from different threads
	pthread_mutex_lock(&plc_leds->mutex_activity);
	plc_gpio_pin_out_toggle(plc_leds->pin_app_running);
	pthread_mutex_unlock(&plc_leds->mutex_activity);
}

ATTR_EXTERN void plc_leds_set_app_activity(struct plc_leds *plc_leds, int on)
{
	pthread_mutex_lock(&plc_leds->mutex_activity);
	plc_gpio_pin_out_set(plc_leds->pin_app_running,
			on ? plc_leds->pin_app_running_level_on : !plc_leds->pin_app_running_level_on);
	pthread_mutex_unlock(&plc_leds->mutex_activity);
}

ATTR_EXTERN void plc_leds_set_tx_activity(struct plc_leds *plc_leds, int on)
{
	pthread_mutex_lock(&plc_leds->mutex_activity);
	plc_gpio_pin_out_set(plc_leds->pin_tx, on);
	pthread_mutex_unlock(&plc_leds->mutex_activity);
}

ATTR_EXTERN void plc_leds_set_rx_activity(struct plc_leds *plc_leds, int on)
{
	pthread_mutex_lock(&plc_leds->mutex_activity);
	plc_gpio_pin_out_set(plc_leds->pin_rx, on);
	pthread_mutex_unlock(&plc_leds->mutex_activity);
}

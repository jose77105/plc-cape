/**
 * @file
 * 
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#define _GNU_SOURCE		// Required for 'asprintf' declaration
#include <unistd.h>
#include "+common/api/+base.h"
#include "+common/api/bbb.h"
#include "afe.h"
#include "afe_commands.h"
#include "api/afe.h"
#include "error.h"
#include "libraries/libplc-gpio/api/gpio.h"
#include "spi.h"

// Logical IO mapping
#define GPIO_SHUTDOWN_MASK GPIO_P9_26_MASK
#define GPIO_SHUTDOWN_BANK GPIO_P9_26_BANK

#define GPIO_TX_FLAG_NUMBER GPIO_P9_14_NUMBER
#define GPIO_RX_FLAG_NUMBER GPIO_P9_16_NUMBER

// In PlcCape v1 the 'INT' signal (ERROR_FLAG) in in P9_28 (SPI_CS0) which is reserved
// by default for audio. Reconfiguration would be required for proper use
#define GPIO_ERROR_FLAG_NUMBER_V1 GPIO_P9_28_NUMBER
#define GPIO_ERROR_FLAG_NUMBER_V2 GPIO_P9_23_NUMBER

struct plc_afe
{
	int standby;
	struct spi *spi;
	struct plc_gpio_pin_out *pin_shutdown;
	struct plc_gpio_sysfs_pin *pin_tx_flag;
	int tx_flag;
	struct plc_gpio_sysfs_pin *pin_rx_flag;
	int rx_flag;
	struct plc_gpio_sysfs_pin *pin_ok_flag;
	int ok_flag;
	void (*controller_flags_callback)(void *data, int tx_flag, int rx_flag, int ok_flag);
	void *flags_callback_data;
	int chip_revision;
	enum afe_block_enum blocks_enabled;
	enum afe_calibration_enum calibration_mode;
};

ATTR_EXTERN const char *afe_gain_tx_pga_enum_text[afe_gain_tx_pga_COUNT] = { "0.25", "0.50",
		"0.71", "1.00" };

ATTR_EXTERN const char *afe_gain_rx_pga1_enum_text[afe_gain_rx_pga1_COUNT] = { "0.25", "0.50",
		"1.00", "2.00" };

ATTR_EXTERN const char *afe_gain_rx_pga2_enum_text[afe_gain_rx_pga2_COUNT] = { "1", "4",
		"16", "64" };

ATTR_EXTERN const char *afe_calibration_enum_text[afe_calibration_COUNT] = { "none",
		"dac_txpga_txfilter", "dac_txpga_rxpga1_rxfilter_rxpga2", "dac_txpga" };

void afe_set_reg_value(struct plc_afe *plc_afe, uint8_t reg, uint8_t content)
{
	spi_write_command(plc_afe->spi, reg, content);
	uint8_t ret = spi_read_command(plc_afe->spi, reg);
	assert(ret == content);
}

void afe_reg_set_mask(struct plc_afe *plc_afe, uint8_t reg, uint8_t mask, uint8_t content)
{
	uint8_t ret = spi_read_command(plc_afe->spi, reg) & ~mask;
	ret |= content;
	spi_write_command(plc_afe->spi, reg, ret);
	ret = spi_read_command(plc_afe->spi, reg) & mask;
	assert(ret == content);
}

void afe_set_gains(struct plc_afe *plc_afe, uint8_t gain_mask, uint8_t gain_cmd)
{
	uint8_t ret;
	ret = spi_read_command(plc_afe->spi, AFEREG_GAIN_SELECT);
	spi_write_command(plc_afe->spi, AFEREG_GAIN_SELECT, (ret & ~gain_mask) | gain_cmd);
	ret = spi_read_command(plc_afe->spi, AFEREG_GAIN_SELECT);
	assert((ret & gain_mask) == gain_cmd);
}

// TODO: Remove global 'plc_cape_version'
extern int plc_cape_version;
extern int plc_cape_emulation;
ATTR_INTERN struct plc_afe *plc_afe_create(struct plc_gpio *plc_gpio, struct spi *spi)
{
	struct plc_afe *plc_afe = (struct plc_afe*) calloc(1, sizeof(struct plc_afe));
	plc_afe->spi = spi;

	// Configure GPIOs
	plc_afe->pin_shutdown = plc_gpio_pin_out_create(plc_gpio, GPIO_SHUTDOWN_BANK,
			GPIO_SHUTDOWN_MASK);
	plc_gpio_pin_out_set(plc_afe->pin_shutdown, 0);

	// Get revision and check for the expected value as a way to be sure the AFE board is connected
	spi_write_command(plc_afe->spi, AFEREG_RESET, AFEREG_RESET_SOFTRESET);
	plc_afe->chip_revision = spi_read_command(plc_afe->spi, AFEREG_REVISION);

	// For the TX_FLAG and RX_FLAG inputs use 'sysfs' to benefit from event-driven implementation
	// With direct GPIO we would have used
	//		'pin_rx_flag = plc_gpio_pin_in_create(plc_gpio, GPIO_P9_14_BANK, GPIO_P9_14_MASK)'
	plc_afe->pin_tx_flag = plc_gpio_sysfs_pin_create(GPIO_TX_FLAG_NUMBER, 0);
	plc_afe->pin_rx_flag = plc_gpio_sysfs_pin_create(GPIO_RX_FLAG_NUMBER, 0);
	plc_afe->pin_ok_flag = plc_gpio_sysfs_pin_create(
			(plc_cape_version >= 2) ? GPIO_ERROR_FLAG_NUMBER_V2 : GPIO_ERROR_FLAG_NUMBER_V1, 0);

	// Execute a soft reset command (0x14) to clear the AFE registers to their default values.
	// It's not required after power-on but interesting to be sure we always start from default
	//	condition
	spi_write_command(plc_afe->spi, AFEREG_RESET, AFEREG_RESET_SOFTRESET);

	// Get revision and check for the expected value as a way to be sure the AFE board is connected
	plc_afe->chip_revision = spi_read_command(plc_afe->spi, AFEREG_REVISION);
	// TODO: Improve error control, e.g. another 'afe_init' function return TRUE if ok 
	if (!plc_cape_emulation && (plc_afe->chip_revision != AFEREG_REVISION_EXPECTED))
	{
		libplc_cape_set_error_msg("A valid AFE board is not found!");
		plc_gpio_sysfs_pin_release(plc_afe->pin_ok_flag);
		plc_gpio_sysfs_pin_release(plc_afe->pin_rx_flag);
		plc_gpio_sysfs_pin_release(plc_afe->pin_tx_flag);
		plc_gpio_pin_out_set(plc_afe->pin_shutdown, 1);
		plc_gpio_pin_out_release(plc_afe->pin_shutdown);
		free(plc_afe);
		return NULL;
	}

	// Configure with default settings
	struct afe_settings settings;
	memset(&settings, 0, sizeof(settings));
	if (!plc_cape_emulation)
		plc_afe_configure(plc_afe, &settings);

	return plc_afe;
}

ATTR_INTERN void plc_afe_release(struct plc_afe *plc_afe)
{
	if (plc_afe->standby)
		plc_afe_set_standy(plc_afe, 0);
	plc_afe_disable_all(plc_afe);
	plc_afe_set_standy(plc_afe, 1);
	plc_gpio_sysfs_pin_release(plc_afe->pin_ok_flag);
	plc_gpio_sysfs_pin_release(plc_afe->pin_rx_flag);
	plc_gpio_sysfs_pin_release(plc_afe->pin_tx_flag);
	free(plc_afe);
}

ATTR_EXTERN void plc_afe_configure(struct plc_afe *plc_afe, const struct afe_settings *settings)
{
	uint8_t ret;

	// Set CENELEC-B/C/D mode
	afe_reg_set_mask(plc_afe, AFEREG_CONTROL1, AFEREG_CONTROL1_CA_CBCD,
			settings->cenelec_a ? 0 : AFEREG_CONTROL1_CA_CBCD);

	// Activate interrupt notification (INT) by thermal and current limit conditions
	// This also enables the T_FLAG and I_FLAG in the RESET register
	ret = spi_read_command(plc_afe->spi, AFEREG_CONTROL2);
	ret &= AFEREG_CONTROL2_ENABLE_OVERLOADS_AND;
	ret |= AFEREG_CONTROL2_ENABLE_OVERLOADS_OR;
	spi_write_command(plc_afe->spi, AFEREG_CONTROL2, ret);
	ret = spi_read_command(plc_afe->spi, AFEREG_CONTROL2);
	assert((ret & AFEREG_CONTROL2_ENABLE_OVERLOADS_OR) == AFEREG_CONTROL2_ENABLE_OVERLOADS_OR);

	afe_set_gains(plc_afe,
			AFEREG_GAIN_SELECT_TX_PGA_MASK | AFEREG_GAIN_SELECT_RX_PGA1_MASK
					| AFEREG_GAIN_SELECT_RX_PGA2_MASK,
			AFEREG_GAIN_SELECT_TX_PGA[settings->gain_tx_pga]
					| AFEREG_GAIN_SELECT_RX_PGA1[settings->gain_rx_pga1]
					| AFEREG_GAIN_SELECT_RX_PGA2[settings->gain_rx_pga2]);
}

ATTR_EXTERN char *plc_afe_get_info(struct plc_afe *plc_afe)
{
	char *info;
	if (!plc_afe)
	{
		asprintf(&info, "N/A\n");
		return info;
	}
	char blocks_text[32] = "";
	if (plc_afe->blocks_enabled & afe_block_dac)
		strcat(blocks_text, "DAC ");
	if (plc_afe->blocks_enabled & afe_block_tx)
		strcat(blocks_text, "TX ");
	if (plc_afe->blocks_enabled & afe_block_ref2)
		strcat(blocks_text, "REF2 ");
	if (plc_afe->blocks_enabled & afe_block_pa)
		strcat(blocks_text, "PA ");
	if (plc_afe->blocks_enabled & afe_block_pa_out)
		strcat(blocks_text, "PAOUT ");
	if (plc_afe->blocks_enabled & afe_block_rx)
		strcat(blocks_text, "RX ");
	if (plc_afe->blocks_enabled & afe_block_ref1)
		strcat(blocks_text, "REF1 ");
	if (plc_afe->blocks_enabled & afe_block_zc)
		strcat(blocks_text, "ZC ");
	// Remove trailing space
	if (blocks_text[0] != '\0')
		blocks_text[strlen(blocks_text) - 1] = '\0';
	// Calibration
	char calibration_text[16] = "";
	switch (plc_afe->calibration_mode)
	{
	case afe_calibration_none:
		strcpy(calibration_text, "None");
		break;
	case afe_calibration_dac_txpga_txfilter:
		strcpy(calibration_text, "TX_CAL");
		break;
	case afe_calibration_dac_txpga_rxpga1_rxfilter_rxpga2:
		strcpy(calibration_text, "RX_CAL");
		break;
	case afe_calibration_dac_txpga:
		strcpy(calibration_text, "TX_PGA_CAL");
		break;
	default:
		assert(0);
	}
	// Compose info
	asprintf(&info, "Revision: %d\n"
			"Blocks enabled: %s\n"
			"Calibration mode: %s\n", plc_afe->chip_revision, blocks_text, calibration_text);
	// TODO: Add 'spi_get_info'
	//		char *info_spi = spi_get_info(spi); ... free(info_spi);

	return info;
}

void afe_set_tx_flag_callback(void *data, int flag_status)
{
	struct plc_afe *plc_afe = data;
	plc_afe->tx_flag = flag_status;
	// TODO: Improvement. Use three different callbacks
	plc_afe->controller_flags_callback(plc_afe->flags_callback_data, plc_afe->tx_flag,
			plc_afe->rx_flag, plc_afe->ok_flag);
}

void afe_set_rx_flag_callback(void *data, int flag_status)
{
	struct plc_afe *plc_afe = data;
	plc_afe->rx_flag = flag_status;
	plc_afe->controller_flags_callback(plc_afe->flags_callback_data, plc_afe->tx_flag,
			plc_afe->rx_flag, plc_afe->ok_flag);
}

void afe_set_ok_flag_callback(void *data, int flag_status)
{
	struct plc_afe *plc_afe = data;
	plc_afe->ok_flag = flag_status;
	plc_afe->controller_flags_callback(plc_afe->flags_callback_data, plc_afe->tx_flag,
			plc_afe->rx_flag, plc_afe->ok_flag);
}

// TODO: Optimization: Use only one shared thread for TX_FLAG and RX_FLAG
// TODO: Add function to uncallback
ATTR_EXTERN void plc_afe_set_flags_callback(struct plc_afe *plc_afe,
		void (*callback)(void *data, int tx_flag, int rx_flag, int ok_flag), void *data)
{
	plc_afe->controller_flags_callback = callback;
	plc_afe->flags_callback_data = data;
	plc_gpio_sysfs_pin_callback(plc_afe->pin_tx_flag, afe_set_tx_flag_callback, plc_afe);
	plc_gpio_sysfs_pin_callback(plc_afe->pin_rx_flag, afe_set_rx_flag_callback, plc_afe);
	plc_gpio_sysfs_pin_callback(plc_afe->pin_ok_flag, afe_set_ok_flag_callback, plc_afe);
	// Get initial values and sent first 'callback' with
	plc_afe->tx_flag = plc_gpio_sysfs_pin_get(plc_afe->pin_tx_flag);
	plc_afe->rx_flag = plc_gpio_sysfs_pin_get(plc_afe->pin_rx_flag);
	plc_afe->ok_flag = plc_gpio_sysfs_pin_get(plc_afe->pin_ok_flag);
	plc_afe->controller_flags_callback(plc_afe->flags_callback_data, plc_afe->tx_flag,
			plc_afe->rx_flag, plc_afe->ok_flag);
}

ATTR_EXTERN void plc_afe_set_standy(struct plc_afe *plc_afe, int standby)
{
	plc_gpio_pin_out_set(plc_afe->pin_shutdown, standby);
	plc_afe->standby = standby;
}

ATTR_EXTERN void plc_afe_activate_blocks(struct plc_afe *plc_afe, enum afe_block_enum blocks)
{
	uint8_t reg1 = 0x0;
	uint8_t reg2 = 0x0;
	// AFEREG_ENABLE1 blocks
	if (blocks & afe_block_pa)
		reg1 |= AFEREG_ENABLE1_PA;
	if (blocks & afe_block_tx)
		reg1 |= AFEREG_ENABLE1_TX;
	if (blocks & afe_block_rx)
		reg1 |= AFEREG_ENABLE1_RX;
	if (blocks & afe_block_dac)
		reg1 |= AFEREG_ENABLE1_DAC;
	// AFEREG_ENABLE2 blocks
	if (blocks & afe_block_zc)
		reg2 |= AFEREG_ENABLE2_ZERO_CROSSING;
	// REF1 block sets the AVDD/2 ref for the Power Amplifier
	if (blocks & afe_block_ref1)
		reg2 |= AFEREG_ENABLE2_REF1;
	if (blocks & afe_block_ref2)
		reg2 |= AFEREG_ENABLE2_REF2;
	if (blocks & afe_block_pa_out)
		reg2 |= AFEREG_ENABLE2_PA_OUT;
	// Activate blocks
	afe_set_reg_value(plc_afe, AFEREG_ENABLE1, reg1);
	afe_set_reg_value(plc_afe, AFEREG_ENABLE2, reg2);
	plc_afe->blocks_enabled = blocks;
	// TODO: Improve. If 15V on with PA off -> Overcurrent flag. We must clear it when enabling PA
	//	for proper monitoring
	//	It seems there is an initial transient when enabling PA. Maybe a C is required?
	//	For the moment just wait for a reasonable amount of time (e.g. 10ms)
	usleep(10000);
	plc_afe_clear_overloads(plc_afe);

	// TODO: Revisar estos comentarios
	// NOTA: Se demuestra que al activar el 'afe_block_tx' se produce un glitch en TX_F_OUT de 0,6V
	//	y 300ns. Se ha probado con un 'sleep(1)' antes y después para estar seguro que se produce en
	//	este momento. Además tampoco depende del valor del DAC en el momento previo a la activación

	// NOTA: Sin 'afe_block_tx' la salida del TX_PGA + TX_FILTER (TX_F_OUT) es la salida directa del
	//	DAC que va entre 0 y 0.5V.
	//	En este modo el DAC abarca todo el rango pero en sentido negativo:
	//		plc_afe_transfer_dac_sample(plc_afe, 0): 0,5V
	//		plc_afe_transfer_dac_sample(plc_afe, 100): 0,45V
	//		plc_afe_transfer_dac_sample(plc_afe, AFE_DAC_MAX_RANGE-100): 0,05V
	//		plc_afe_transfer_dac_sample(plc_afe, 0): 0,05V
	//
	//	Si además del 'afe_block_dac' se activa el 'afe_block_tx' entonces actúa el TX_PGA que tiene
	//	una amplificación negativa
	//	Con la doble negación se hace que al activar el conjunto SPI(0) -> TX_F_OUT 0V; SPI(MAX_DAC)
	//	-> TX_F_FOUT 0.5V

	// Activate DAC line to turn on DAC mode and send a first sample to the buffer to put DAC output
	//	in a known state

	// Enable DAC block to prepare DAC output with a stable sample before enabling TX
	// Enable TX block
	// Enable the REF2 block to set the AVDD/2 ref as the center reference. The offset shifts the
	//	wave around this center
	// If REF2 is off the center is at GND
	// REF1 has no effect in this mode (it acts only on PA)

}

ATTR_EXTERN void plc_afe_set_calibration_mode(struct plc_afe *plc_afe,
		enum afe_calibration_enum calibration_mode)
{
	uint8_t reg = 0;
	// Calibration modes routes internal paths of the AFE
	// It's like connecting the corresponding AFE031 pins and internally opening the connection
	//	beyond
	switch (calibration_mode)
	{
	case afe_calibration_none:
		break;
	case afe_calibration_dac_txpga_txfilter:
		reg = AFEREG_CONTROL1_CALIB_TX;
		break;
	case afe_calibration_dac_txpga_rxpga1_rxfilter_rxpga2:
		reg = AFEREG_CONTROL1_CALIB_RX;
		break;
	case afe_calibration_dac_txpga:
		// TX_PGA_OUT -> RX_PGA2_OUT (BBB.ADC)
		reg = AFEREG_CONTROL1_CALIB_TX_PGA;
		break;
	default:
		assert(0);
	}
	afe_reg_set_mask(plc_afe, AFEREG_CONTROL1, AFEREG_CONTROL1_CALIB_MASK, reg);
	plc_afe->calibration_mode = calibration_mode;
	// TODO: Review if necessary
	usleep(10000);
	plc_afe_clear_overloads(plc_afe);
}

ATTR_EXTERN void plc_afe_set_gain_tx(struct plc_afe *plc_afe, enum afe_gain_tx_pga_enum gain)
{
	assert(gain < ARRAY_SIZE(AFEREG_GAIN_SELECT_TX_PGA));
	afe_set_gains(plc_afe, AFEREG_GAIN_SELECT_TX_PGA_MASK, AFEREG_GAIN_SELECT_TX_PGA[gain]);
}

ATTR_EXTERN void plc_afe_set_gains_rx(struct plc_afe *plc_afe, enum afe_gain_rx_pga1_enum gain1,
		enum afe_gain_rx_pga2_enum gain2)
{
	assert((gain1 < ARRAY_SIZE(AFEREG_GAIN_SELECT_RX_PGA1))
			&& (gain2 < ARRAY_SIZE(AFEREG_GAIN_SELECT_RX_PGA2)));
	afe_set_gains(plc_afe, AFEREG_GAIN_SELECT_RX_PGA1_MASK | AFEREG_GAIN_SELECT_RX_PGA2_MASK,
			AFEREG_GAIN_SELECT_RX_PGA1[gain1] | AFEREG_GAIN_SELECT_RX_PGA2[gain2]);
}

ATTR_EXTERN void plc_afe_disable_all(struct plc_afe *plc_afe)
{
	// Disable all blocks (DAC, TX, ZC, REFs, PA_OUT...)
	plc_afe_activate_blocks(plc_afe, 0);
}

ATTR_EXTERN void plc_afe_clear_overloads(struct plc_afe *plc_afe)
{
	uint8_t ret = spi_read_command(plc_afe->spi, AFEREG_RESET);
	ret &= AFEREG_RESET_CLEAR_OVERLOADS_MASK;
	spi_write_command(plc_afe->spi, AFEREG_RESET, ret);
}

ATTR_EXTERN uint8_t plc_afe_get_overloads(struct plc_afe *plc_afe)
{
	return spi_read_command(plc_afe->spi, AFEREG_RESET);
}

ATTR_EXTERN void plc_afe_configure_spi(struct plc_afe *plc_afe, uint32_t spi_freq,
		uint16_t spi_delay)
{
	spi_configure(plc_afe->spi, spi_freq, spi_delay);
}

ATTR_EXTERN void plc_afe_set_dac_mode(struct plc_afe *plc_afe, int enable)
{
	spi_set_dac_mode(plc_afe->spi, enable);
}

ATTR_EXTERN void plc_afe_transfer_dac_sample(struct plc_afe *plc_afe, uint16_t sample)
{
	spi_transfer_dac_sample(plc_afe->spi, sample);
}

ATTR_INTERN struct spi *plc_afe_get_spi(struct plc_afe *plc_afe)
{
	return plc_afe->spi;
}

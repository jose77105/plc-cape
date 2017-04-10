/**
 * @file
 * @brief	AFE031 management
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef LIBPLC_CAPE_AFE_H
#define LIBPLC_CAPE_AFE_H

/**
 * @brief	Blocks within the AFE that can be selectively enabled or disabled
 */
enum afe_block_enum
{
	afe_block_none = 0x0,
	afe_block_dac = 0x1,
	afe_block_tx = 0x2,
	afe_block_ref2 = 0x04,
	afe_block_pa = 0x8,
	afe_block_pa_out = 0x10,
	afe_block_rx = 0x20,
	afe_block_ref1 = 0x40,
	afe_block_zc = 0x80,
};

enum afe_gain_tx_pga_enum
{
	afe_gain_tx_pga_025 = 0,
	afe_gain_tx_pga_050,
	afe_gain_tx_pga_071,
	afe_gain_tx_pga_100
};

enum afe_gain_rx_pga1_enum
{
	afe_gain_rx_pga1_025 = 0,
	afe_gain_rx_pga1_050,
	afe_gain_rx_pga1_100,
	afe_gain_rx_pga1_200
};

enum afe_gain_rx_pga2_enum
{
	afe_gain_rx_pga2_1 = 0,
	afe_gain_rx_pga2_4,
	afe_gain_rx_pga2_16,
	afe_gain_rx_pga2_64
};

enum afe_calibration_enum
{
	afe_calibration_none = 0,
	afe_calibration_dac_txpga_txfilter,
	afe_calibration_dac_txpga_rxpga1_rxfilter_rxpga2,
	afe_calibration_dac_txpga,
};

/**
 * @brief	List of settings that can be configured in a single @ref plc_afe_configure call
 */
struct afe_settings
{
	int cenelec_a;
	enum afe_gain_tx_pga_enum gain_tx_pga;
	enum afe_gain_rx_pga1_enum gain_rx_pga1;
	enum afe_gain_rx_pga2_enum gain_rx_pga2;
};

struct plc_afe;

/**
 * @brief	Updates a group of the AFE settings
 * @param	plc_afe		Pointer to the handler object
 * @param	settings	Group of settings
 * @details	This function allows to configure in a single call some of the available settings.
 *			Other configurable settings must be configured with the corresponding call
 */
void plc_afe_configure(struct plc_afe *plc_afe, const struct afe_settings *settings);
/**
 * @brief	Gets current configuration in a textual representation for displaying purposes
 * @param	plc_afe		Pointer to the handler object
 * @return	A string with the information. It should be released with 'free' when no longer required
 */
char *plc_afe_get_info(struct plc_afe *plc_afe);
/**
 * @brief	Sets a callback that will be called when some change in the AFE flags is detected
 * @param	plc_afe		Pointer to the handler object
 * @param	callback	The callback that will be called when changes detected
 * @param	data		Optional user data that will be sent in the first parameter of the callback
 */
void plc_afe_set_flags_callback(struct plc_afe *plc_afe,
		void (*callback)(void *data, int tx_flag, int rx_flag, int ok_flag), void *data);
/**
 * @brief	Sets the AFE in normal operating mode or in standby
 * @param	plc_afe		Pointer to the handler object
 * @param	standby		0 for normal mode, 1 for standby
 */
void plc_afe_set_standy(struct plc_afe *plc_afe, int standby);
/**
 * @brief	Selects which blocks to enable in the AFE
 * @param	plc_afe		Pointer to the handler object
 * @param	blocks		OR mask of the blocks to be enabled
 */
void plc_afe_activate_blocks(struct plc_afe *plc_afe, enum afe_block_enum blocks);
/**
 * @brief	Configures the AFE to work in one of the calibration modes or in normal operation
 * @param	plc_afe		Pointer to the handler object
 * @param	calibration_mode	The calibration mode or 'none' for normal operation
 */
void plc_afe_set_calibration_mode(struct plc_afe *plc_afe,
		enum afe_calibration_enum calibration_mode);
/**
 * @brief	Configures the gain of the TX_PGA block
 * @param	plc_afe		Pointer to the handler object
 * @param	gain		One of the predefined valid gains for TX_PGA
 */
void plc_afe_set_gain_tx(struct plc_afe *plc_afe, enum afe_gain_tx_pga_enum gain);
/**
 * @brief	Configures the gain of the RX_PGA1 and RX_PGA2 blocks
 * @param	plc_afe		Pointer to the handler object
 * @param	gain1		One of the predefined valid gains for RX_PGA1
 * @param	gain2		One of the predefined valid gains for RX_PGA2
 */
void plc_afe_set_gains_rx(struct plc_afe *plc_afe, enum afe_gain_rx_pga1_enum gain1,
		enum afe_gain_rx_pga2_enum gain2);
/**
 * @brief	Disables all the internal blocks of AFE
 * @param	plc_afe		Pointer to the handler object
 */
void plc_afe_disable_all(struct plc_afe *plc_afe);
/**
 * @brief	Clears the 'overload' flag of the AFE, which are triggered by an excess of temperature
 *			or current
 * @param	plc_afe		Pointer to the handler object
 */
void plc_afe_clear_overloads(struct plc_afe *plc_afe);
/**
 * @brief	Gets the current 'overload' flag status
 * @param	plc_afe		Pointer to the handler object
 * @return	The value of the AFEREG_RESET register of the AFE, which includes the 'overload'
 *			status and cause
 */
uint8_t plc_afe_get_overloads(struct plc_afe *plc_afe);
/**
 * @brief	Configures the SPI bus
 * @param	plc_afe			Pointer to the handler object
 * @param	spi_dac_freq	Transmission rate of the SPI bus (in samples per second)
 * @param	spi_dac_delay	Delay between samples in microseconds
 */
void plc_afe_configure_spi(struct plc_afe *plc_afe, uint32_t spi_dac_freq, uint16_t spi_dac_delay);
/**
 * @brief	Switches between DAC mode and Commands mode
 * @param	plc_afe	Pointer to the handler object
 * @param	enable	0 for Commands mode; 1 for DAC mode
 */
void plc_afe_set_dac_mode(struct plc_afe *plc_afe, int enable);
/**
 * @brief	Transfers a single value to the DAC
 * @param	plc_afe	Pointer to the handler object
 * @param	sample	DAC value
 */
void plc_afe_transfer_dac_sample(struct plc_afe *plc_afe, uint16_t sample);

#endif /* LIBPLC_CAPE_AFE_H */

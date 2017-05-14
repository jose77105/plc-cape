/**
 * @file
 * @brief	Analysis tools for the ADC received data
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef LIBPLC_ADC_ANALYSIS_H
#define LIBPLC_ADC_ANALYSIS_H

#ifdef __cplusplus
extern "C" {
#endif

enum plc_rx_statistics_mode
{
	plc_rx_statistics_none = 0,
	plc_rx_statistics_values,
	plc_rx_statistics_time,
};

struct rx_statistics
{
	uint32_t buffers_handled;
	uint32_t buffer_preparation_us;
	uint32_t buffer_preparation_min_us;
	uint32_t buffer_preparation_max_us;
	uint32_t buffer_cycle_us;
	uint32_t buffer_cycle_min_us;
	uint32_t buffer_cycle_max_us;
	sample_rx_t buffer_min;
	sample_rx_t buffer_max;
	float buffer_dc_mean;
	float buffer_ac_mean;
};

/**
 * @brief	Creates an object instance
 * @return	A pointer to the created object
 */
struct plc_rx_analysis *plc_rx_analysis_create(void);
/**
 * @brief	Releases an object
 * @param	plc_rx_analysis	Pointer to the handler object
 */
void plc_rx_analysis_release(struct plc_rx_analysis *plc_rx_analysis);
/**
 * @brief	Configures the analysis
 * @param	plc_rx_analysis	Pointer to the handler object
 * @param	freq_adc_sps	Frequency of the ADC in samples per second
 * @param	data_bit_us		Length of a bit of data in microseconds
 * @param	data_offset		The value captured by the ADC for incoming samples corresponding to idle
 *							or no data
 * @param	data_hi_threshold_detection
 *							The minimum delta value over the data_offset to consider the incoming
 *							samples to contain some data
 */
void plc_rx_analysis_configure(struct plc_rx_analysis *plc_rx_analysis, float freq_adc_sps,
		uint32_t data_bit_us, sample_rx_t data_offset, sample_rx_t data_hi_threshold_detection);
/**
 * @brief	Resets an analysis for starting a new fresh session
 * @param	plc_rx_analysis	Pointer to the handler object
 */
void plc_rx_analysis_reset(struct plc_rx_analysis *plc_rx_analysis);
/**
 * @brief	Set the type of statistics to calculate from the received buffer
 * @param	plc_rx_analysis	Pointer to the handler object
 * @param	mode			The statistics requested on buffer analysis
 */
void plc_rx_analysis_set_statistics_mode(struct plc_rx_analysis *plc_rx_analysis, 
	enum plc_rx_statistics_mode mode);
/**
 * @brief	Analyzes a buffer considering the current 'plc_rx_statistics_mode'
 * @param	plc_rx_analysis	Pointer to the handler object
 * @param	buffer			Buffer to analyze
 * @param	buffer_samples	Number of samples in the buffer
 */
ATTR_EXTERN int plc_rx_analysis_analyze_buffer(struct plc_rx_analysis *plc_rx_analysis, 
	sample_rx_t *buffer, uint32_t buffer_samples);
/**
 * @brief	Gets the current statistics
 * @param	plc_rx_analysis	Pointer to the handler object
 * @return	A pointer to a volatile struct with the statistics information
 */
const struct rx_statistics *plc_rx_analysis_get_statistics(struct plc_rx_analysis *plc_rx_analysis);

// INTERNAL?
void plc_rx_analysis_report_rx_statistics_time(struct plc_rx_analysis *plc_rx_analysis,
	uint32_t buffer_preparation_us);

#ifdef __cplusplus
}
#endif

#endif /* LIBPLC_ADC_ANALYSIS_H */

/**
 * @file
 * @brief	High speed ADC capturing
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef LIBPLC_ADC_ADC_H
#define LIBPLC_ADC_ADC_H

struct plc_adc;
struct settings_rx;

typedef void (*rx_buffer_completed_callback_t)(
		void *data, sample_rx_t *samples_buffer, uint32_t samples_buffer_count);

/**
 * @brief	Gets the current ADC capturing frequency
 * @return	The capturing frequency in samples per second
 */
float plc_adc_get_sampling_frequency(void);
/**
 * @brief	Creates an object instance
 * @param	plc_driver	0 to use standard drivers to access the ADC, 1 to use the customized ones
 *			for higher capturing data rates
 * @return	A pointer to the created object
 */
struct plc_adc *plc_adc_create(int plc_driver);
/**
 * @brief	Releases an object
 * @param	plc_adc	Pointer to the handler object
 */
void plc_adc_release(struct plc_adc *plc_adc);
/**
 * @brief	Set a callback that will be called when the capture buffer is completed
 * @param	plc_adc	Pointer to the handler object
 * @param	rx_buffer_completed_callback
 *					Callback that will be called when a buffer of samples is completed
 * @param	rx_buffer_completed_callback_data
 *					Optional user data to be sent as the first parameter in the callback
 * @details
 *	The callback is called from a different thread that is initiated in the #plc_adc_start_capture
 *	call. The caller is responsible of the contenion mechanisms to access any possible shared
 *	resource
 */
void plc_adc_set_rx_buffer_completed_callback(struct plc_adc *plc_adc,
		rx_buffer_completed_callback_t rx_buffer_completed_callback,
		void *rx_buffer_completed_callback_data);
/**
 * @brief	Just reads one sample from the ADC
 * @param	plc_adc	Pointer to the handler object
 * @return	The value of the sample captured by the ADC
 */
sample_rx_t plc_adc_read_sample(struct plc_adc *plc_adc);
/**
 * @brief	Starts the continuous capturing mode from ADC to dedicated buffers
 * @param	plc_adc				Pointer to the handler object
 * @param	buffer_samples		Number of samples to bufferize before calling the callback function
 * @param	kernel_buffering	0 to use buffering in user-space (standard driver), 1 to use
 *								buffering in kernel-space (provided by the custom ADC driver)
 */
void plc_adc_start_capture(struct plc_adc *plc_adc, uint32_t buffer_samples, int kernel_buffering);
/**
 * @brief	Stops the ADC continuous capturing mode
 * @param	plc_adc	Pointer to the handler object
 */
void plc_adc_stop_capture(struct plc_adc *plc_adc);

#endif /* LIBPLC_ADC_ADC_H */

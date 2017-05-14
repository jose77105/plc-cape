/**
 * @file
 * @brief	High speed ADC capturing
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef LIBPLC_ADC_ADC_H
#define LIBPLC_ADC_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

#define ADC_BITS 12
#define ADC_RANGE (1 << ADC_BITS)

struct plc_adc;
struct settings_rx;

typedef void (*rx_buffer_completed_callback_t)(void *data, sample_rx_t *samples_buffer,
		uint32_t samples_buffer_count);

/**
 * @brief	Creates an object instance
 * @param	rx_device	Capturing device
 * @return	A pointer to the created object
 */
struct plc_adc *plc_adc_create(enum plc_rx_device_enum rx_device);
/**
 * @brief	Releases an object
 * @param	plc_adc	Pointer to the handler object
 */
void plc_adc_release(struct plc_adc *plc_adc);
/**
 * @brief	Gets the current ADC capturing frequency
 * @param	plc_adc	Pointer to the handler object
 * @return	The capturing frequency in samples per second
 */
float plc_adc_get_sampling_frequency(struct plc_adc *plc_adc);
/**
 * @brief	Set a callback that will be called when the capture buffer is completed
 * @param	plc_adc	Pointer to the handler object
 * @param	rx_buffer_completed_callback
 *					Callback that will be called when a buffer of samples is completed
 * @param	rx_buffer_completed_callback_data
 *					Optional user data to be sent as the first parameter in the callback
 * @details
 *	The callback is called from a different thread that is initiated in the #plc_adc_start_capture
 *	call. The caller is responsible of the contention mechanisms to access any possible shared
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
 * @param	freq_capture_sps	ADC capture frequency in samples per second
 * @return	0 if OK; -1 if error
 */
int plc_adc_start_capture(struct plc_adc *plc_adc, uint32_t buffer_samples, int kernel_buffering,
		float freq_capture_sps);
/**
 * @brief	Stops the ADC continuous capturing mode
 * @param	plc_adc	Pointer to the handler object
 */
void plc_adc_stop_capture(struct plc_adc *plc_adc);

#ifdef __cplusplus
}
#endif

#endif /* LIBPLC_ADC_ADC_H */

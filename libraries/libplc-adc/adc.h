/**
 * @file
 * @brief	Transmission scheduling modes
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef ADC_H
#define ADC_H

#include "api/adc.h"

#define THREAD_TIMEOUT_SECONDS 5

#ifndef PLC_ADC_HANDLE_EXPLICIT_DEF
typedef void *plc_adc_h;
#endif

struct plc_adc_api
{
	void (*release)(plc_adc_h handle);
	float (*get_sampling_frequency)(plc_adc_h handle);
	void (*set_rx_buffer_completed_callback)(plc_adc_h handle,
			rx_buffer_completed_callback_t rx_buffer_completed_callback,
			void *rx_buffer_completed_callback_data);
	sample_rx_t (*read_sample)(plc_adc_h handle);
	int (*start_capture)(plc_adc_h handle, uint32_t buffer_samples, int kernel_buffering,
			float freq_capture_sps);
	void (*stop_capture)(plc_adc_h handle);
};

plc_adc_h plc_adc_bbb_create(struct plc_adc_api *api, int std_driver);
plc_adc_h plc_adc_alsa_create(struct plc_adc_api *api);
plc_adc_h plc_adc_fifo_create(struct plc_adc_api *api);

#endif /* ADC_H_ */

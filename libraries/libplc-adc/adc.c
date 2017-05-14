/**
 * @file
 * @brief	**Main** file
 *
 * @see		@ref library-libplc-adc
 *
 * @cond COPYRIGHT_NOTES
 *
 * ##LICENSE
 *
 *		This file is part of plc-cape project.
 *
 *		plc-cape project is free software: you can redistribute it and/or modify
 *		it under the terms of the GNU General Public License as published by
 *		the Free Software Foundation, either version 3 of the License, or
 *		(at your option) any later version.
 *
 *		plc-cape project is distributed in the hope that it will be useful,
 *		but WITHOUT ANY WARRANTY; without even the implied warranty of
 *		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *		GNU General Public License for more details.
 *
 *		You should have received a copy of the GNU General Public License
 *		along with plc-cape project.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega
 *
 * @endcond
 */

#include "+common/api/+base.h"
#include "adc.h"

struct plc_adc
{
	struct plc_adc_api api;
	plc_adc_h handle;
};

ATTR_EXTERN struct plc_adc *plc_adc_create(enum plc_rx_device_enum rx_device)
{
	struct plc_adc *plc_adc = calloc(1, sizeof(struct plc_adc));
	switch (rx_device)
	{
	case plc_rx_device_adc_bbb:
		plc_adc->handle = plc_adc_bbb_create(&plc_adc->api, 0);
		break;
	case plc_rx_device_adc_bbb_std_drivers:
		plc_adc->handle = plc_adc_bbb_create(&plc_adc->api, 1);
		break;
	case plc_rx_device_alsa:
		plc_adc->handle = plc_adc_alsa_create(&plc_adc->api);
		break;
	case plc_rx_device_internal_fifo:
		plc_adc->handle = plc_adc_fifo_create(&plc_adc->api);
		break;
	default:
		assert(0);
	}
	return plc_adc;
}

ATTR_EXTERN void plc_adc_release(struct plc_adc *plc_adc)
{
	plc_adc->api.release(plc_adc->handle);
	free(plc_adc);
}

ATTR_EXTERN float plc_adc_get_sampling_frequency(struct plc_adc *plc_adc)
{
	return plc_adc->api.get_sampling_frequency(plc_adc->handle);
}

ATTR_EXTERN void plc_adc_set_rx_buffer_completed_callback(struct plc_adc *plc_adc,
		rx_buffer_completed_callback_t rx_buffer_completed_callback,
		void *rx_buffer_completed_callback_data)
{
	plc_adc->api.set_rx_buffer_completed_callback(plc_adc->handle, rx_buffer_completed_callback,
			rx_buffer_completed_callback_data);
}

ATTR_EXTERN sample_rx_t plc_adc_read_sample(struct plc_adc *plc_adc)
{
	return plc_adc->api.read_sample(plc_adc->handle);
}

ATTR_EXTERN int plc_adc_start_capture(struct plc_adc *plc_adc, uint32_t buffer_samples,
		int kernel_buffering, float freq_capture_sps)
{
	return plc_adc->api.start_capture(plc_adc->handle, buffer_samples, kernel_buffering, freq_capture_sps);
}

ATTR_EXTERN void plc_adc_stop_capture(struct plc_adc *plc_adc)
{
	return plc_adc->api.stop_capture(plc_adc->handle);
}

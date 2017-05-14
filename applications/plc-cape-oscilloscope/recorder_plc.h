/**
 * @file
 * @brief	Implementation of a recorder based on the PlcCape functionality
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef RECORDER_PLC_H
#define RECORDER_PLC_H

#include "+common/api/+base.h"
#include "libraries/libplc-adc/api/analysis.h"
#include "recorder_interface.h"

struct plc_adc;
struct plc_rx_analysis;

struct recorder_plc_configuration
{
	float capturing_rate_sps;
	enum plc_rx_statistics_mode rx_statistics_mode;
};

class Recorder_plc: public Recorder_interface, private recorder_plc_configuration
{
public:
	Recorder_plc();
	virtual ~Recorder_plc();
	virtual void release(void) { delete this; }
	virtual void initialize(void);
	virtual void terminate(void);
	void release_configuration(void);
	void set_configuration_defaults(void);
	// Configuration_interface
	virtual int begin_configuration(void);
	virtual int set_configuration_setting(const char *identifier, const char *data);
	virtual int end_configuration(void);
	virtual void set_preferred_capturing_rate(float capturing_rate_sps);
	virtual float get_real_capturing_rate(void);
	virtual void start_recording(void);
	virtual void stop_recording(void);
	virtual void pause(void);
	virtual void resume(void);
	virtual void pop_recorded_buffer(sample_rx_t *samples, uint32_t samples_count);
	virtual void get_statistics(char *text, size_t text_size);

private:
	static void rx_buffer_completed_callback(void *data, sample_rx_t *samples_buffer,
			uint32_t samples_buffer_count);

	struct plc_adc * plc_adc;
	enum plc_rx_device_enum rx_device;
	uint32_t samples_adc_len;
	sample_rx_t *samples_adc;
	sample_rx_t *samples_adc_cur;
	sample_rx_t *samples_adc_end;
	sample_rx_t *samples_adc_pop;
	int paused;
	uint32_t samples_adc_stored;
	uint32_t overflows_detected;
	uint32_t samples_adc_capturing_time_per_quarter_buffer_us;
	struct plc_rx_analysis *plc_rx_analysis;
};

#endif /* RECORDER_PLC_H */

/**
 * @file
 * @brief	Public interface that all the 'recorder' classes must implement
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef RECORDER_INTERFACE_H
#define RECORDER_INTERFACE_H

#include "configuration_interface.h"

class Recorder_interface : public Configuration_interface
{
public:
	virtual ~Recorder_interface() {};
	virtual void release(void) = 0;
	virtual void initialize(void) = 0;
	virtual void terminate(void) = 0;
	virtual void set_preferred_capturing_rate(float capturing_rate_sps) = 0;
	virtual float get_real_capturing_rate(void) = 0;
	virtual void start_recording(void) = 0;
	virtual void stop_recording(void) = 0;
	virtual void pause(void) = 0;
	virtual void resume(void) = 0;
	virtual void pop_recorded_buffer(sample_rx_t *samples, uint32_t samples_count) = 0;
	virtual void get_statistics(char *text, size_t text_size) = 0;
};

#endif /* RECORDER_INTERFACE_H */

/**
 * @file
 * @brief	Reception engine for the incoming signals
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef RX_H
#define RX_H

extern const char *rx_mode_enum_text[];
enum rx_mode_enum
{
	rx_mode_none = 0,
	rx_mode_sample_by_sample,
	rx_mode_buffer_by_buffer,
	rx_mode_kernel_buffering,
	rx_mode_COUNT
};

extern const char *demodulation_mode_enum_text[];
enum demodulation_mode_enum
{
	demod_mode_none = 0,
	demod_mode_real_time,
	demod_mode_deferred,
	demod_mode_COUNT
};

struct rx_settings
{
	enum rx_mode_enum rx_mode;
	enum demodulation_mode_enum demod_mode;
	uint32_t samples_to_file;
	uint32_t data_bit_us;
	uint16_t demod_data_hi_threshold;
	uint16_t data_offset;
	uint16_t data_hi_threshold_detection;
};

struct plc_adc;
struct decoder;
struct monitor;
struct plc_leds;
struct rx;
struct settings;

struct rx *rx_create(const struct rx_settings *settings, struct monitor *monitor,
		struct plc_leds *leds, struct plc_adc *plc_adc, struct decoder *decoder);
void rx_release(struct rx *rx);
void rx_start_capture(struct rx *rx);
void rx_stop_capture(struct rx *rx);

#endif /* RX_H */

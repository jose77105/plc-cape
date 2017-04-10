/**
 * @file
 * @brief	Generic purpose _encoder_ converting incoming received samples to useful data
 * @details
 *	It's a wrapper for _encoder_-type plugins
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef ENCODER_H
#define ENCODER_H

#include "plugins/encoder/api/encoder.h"

extern const char *stream_type_enum_text[];

struct encoder;

struct encoder *encoder_create(const char *path);
void encoder_release(struct encoder *encoder);
void encoder_configure_WAVE(struct encoder *encoder, enum stream_type_enum stream_type,
		int samples_offset, int samples_range, float samples_freq, const char *filename,
		uint32_t bit_width_us);
void encoder_configure_WAV(struct encoder *encoder, const char *filename);
void encoder_configure_OOK(struct encoder *encoder, int samples_offset, int samples_range,
		float samples_freq, uint32_t bit_width_us, const char *message_to_send);
void encoder_configure_PWM(struct encoder *encoder, int samples_offset, int samples_range,
		float samples_freq, uint32_t bit_width_us, const char *message_to_send);
void encoder_reset(struct encoder *encoder);
void encoder_prepare_next_samples(struct encoder *encoder, uint16_t *buffer, uint32_t buffer_count);
void encoder_set_copy_mode(struct encoder *encoder, uint16_t *data_source, uint32_t data_source_len,
		int continuous_mode);

#endif /* ENCODER_H */

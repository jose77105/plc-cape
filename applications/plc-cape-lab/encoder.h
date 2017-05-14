/**
 * @file
 * @brief	Generic purpose _encoder_ converting incoming received samples to useful data
 * @details
 *	It's a wrapper for _encoder_-type plugins
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef ENCODER_H
#define ENCODER_H

#include "plugins/encoder/api/encoder.h"

struct encoder;
struct setting_list_item;

struct encoder *encoder_create(const char *path);
void encoder_release(struct encoder *encoder);
void encoder_set_configuration(struct encoder *encoder, const struct setting_list_item *setting_list);
void encoder_set_default_configuration(struct encoder *encoder);
void encoder_apply_configuration(struct encoder *encoder, float sampling_rate_sps,
		uint32_t bit_width_us);
void encoder_reset(struct encoder *encoder);
void encoder_prepare_next_samples(struct encoder *encoder, uint16_t *buffer, uint32_t buffer_count);
void encoder_set_copy_mode(struct encoder *encoder, uint16_t *data_source, uint32_t data_source_len,
		int continuous_mode);
int encoder_is_ready(struct encoder *encoder);
struct setting_linked_list_item *encoder_get_settings(struct encoder *encoder);
const char *encoder_get_name(struct encoder *encoder);

#endif /* ENCODER_H */

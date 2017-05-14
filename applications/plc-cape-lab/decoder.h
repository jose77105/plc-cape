/**
 * @file
 * @brief	Generic purpose _decoder_ converting incoming received samples to useful data
 * @details
 *	It's a wrapper for _decoder_-type plugins
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef DECODER_H
#define DECODER_H

struct decoder;
struct setting_list_item;

struct decoder *decoder_create(const char *path);
void decoder_release(struct decoder *decoder);
void decoder_set_configuration(struct decoder *decoder, const struct setting_list_item *setting_list);
void decoder_set_default_configuration(struct decoder *decoder);
void decoder_apply_configuration(struct decoder *decoder, sample_rx_t data_offset,
		sample_rx_t data_hi_threshold_detection, float capturing_rate_sps, uint32_t bit_width_us,
		uint32_t samples_to_file);
void decoder_initialize_demodulator(struct decoder *decoder, uint32_t chunk_samples);
void decoder_terminate_demodulator(struct decoder *decoder);
uint32_t decoder_parse_next_samples(struct decoder *decoder, const sample_rx_t *buffer_in,
		uint8_t *buffer_data_out, uint32_t buffer_data_out_count);
int decoder_is_ready(struct decoder *decoder);
struct setting_linked_list_item *decoder_get_settings(struct decoder *decoder);
const char *decoder_get_name(struct decoder *decoder);

#endif /* DECODER_H */

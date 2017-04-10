/**
 * @file
 * @brief	Generic purpose _decoder_ converting incoming received samples to useful data
 * @details
 *	It's a wrapper for _decoder_-type plugins
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef DECODER_H
#define DECODER_H

struct decoder;

struct decoder *decoder_create(const char *path);
void decoder_release(struct decoder *decoder);
void decoder_initialize_demodulator(struct decoder *decoder, uint32_t chunk_samples,
		sample_rx_t demod_data_hi_threshold, sample_rx_t data_offset,
		float samples_per_bit, uint32_t samples_to_file);
void decoder_terminate_demodulator(struct decoder *decoder);
uint32_t decoder_parse_next_samples(struct decoder *decoder, const sample_rx_t *buffer_in,
		uint8_t *buffer_data_out, uint32_t buffer_data_out_count);

#endif /* DECODER_H */

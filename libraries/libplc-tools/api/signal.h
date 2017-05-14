/**
 * @file
 * @brief	Signal processing tools
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef LIBPLC_TOOLS_SIGNAL_H
#define LIBPLC_TOOLS_SIGNAL_H

#ifdef __cplusplus
extern "C"
{
#endif

// TODO: Replace 'plc_signal_iir' with a more meaningful name (e.g. 'plc_demodulator')
struct plc_signal_iir;

/**
 * @brief	Demodulator types
 */
enum plc_signal_iir_demodulator_enum
{
	/// Demodulation process skipped
	plc_signal_iir_demodulator_none = 0,
	/// Demodulation done with the 'absolute' function
	plc_signal_iir_demodulator_abs,
	/// Demodulation by 'cosinus'
	plc_signal_iir_demodulator_cos,
};

/**
 * @brief	Creates a new object encapsulating demodulation functionalities
 * @param	chunk_samples	Size of the intermediate buffer for processing at chunks
 * @param	iir_a			'a' coefficients of the IIR filter applied after demodulation
 * @param	iir_a_count		Number of 'a' coefficients provided
 * @param	iir_b			'b' coefficients for the IIR filter applied after demodulation
 * @param	iir_b_count		Number of 'b' coefficients provided
 * @return	Pointer to the handler object
 */
struct plc_signal_iir *plc_signal_iir_create(uint32_t chunk_samples, const float *iir_a,
		uint32_t iir_a_count, const float *iir_b, uint32_t iir_b_count);
/**
 * @brief	Releases a handler object
 * @param	plc_signal_iir	Pointer to the handler object
 */
void plc_signal_iir_release(struct plc_signal_iir *plc_signal_iir);
/**
 * @brief	Gets a pointer to the buffer with the filtered samples
 * @param	plc_signal_iir	Pointer to the handler object
 * @return	Pointer to the buffer with the resulting filtered samples
 */
float* plc_signal_get_buffer_out(struct plc_signal_iir *plc_signal_iir);
/**
 * @brief	Indicates the amount of samples to be stored on a file
 * @param	plc_signal_iir		Pointer to the handler object
 * @param	samples_to_file		The amount of samples to be stored
 */
void plc_signal_iir_set_samples_to_file(struct plc_signal_iir *plc_signal_iir,
		uint32_t samples_to_file);
/**
 * @brief	Sets the demodulator type
 * @param	plc_signal_iir		Pointer to the handler object
 * @param	demodulator			Demodulator used before the filtering process
 */
void plc_signal_iir_set_demodulator(struct plc_signal_iir *plc_signal_iir,
		enum plc_signal_iir_demodulator_enum demodulator);
/**
 * @brief	Sets the frequency that will be used on demodulation
 * @param	plc_signal_iir		Pointer to the handler object
 * @param	digital_frequency	Frequency in digital terms (0.0 to 1.0)
 */
void plc_signal_iir_set_demodulation_frequency(struct plc_signal_iir *plc_signal_iir,
		float digital_frequency);
/**
 * @brief	Resets the object to its initial state
 * @param	plc_signal_iir		Pointer to the handler object
 */
void plc_signal_iir_reset(struct plc_signal_iir *plc_signal_iir);
/**
 * @brief	Process a chunk of samples
 * @param	plc_signal_iir		Pointer to the handler object
 * @param	buffer_in			Pointer to the input buffer to process
 * @param	buffer_in_offset	Reference offset level in the input buffer
 */
void plc_signal_iir_process_chunk(struct plc_signal_iir *plc_signal_iir,
		const sample_rx_t *buffer_in, sample_rx_t buffer_in_offset);

#ifdef __cplusplus
}
#endif

#endif /* LIBPLC_TOOLS_SIGNAL_H */

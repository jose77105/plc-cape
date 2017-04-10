/**
 * @file
 * @brief	Public API of _decoder_ plugins
 *
 * @see		@ref plugins-decoder
 * 
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef PLUGINS_DECODER_DECODER_H
#define PLUGINS_DECODER_DECODER_H

/**
 * @brief PLUGIN API duality. See @ref plugins for more info
 */
#ifndef PLUGINS_API_HANDLE_EXPLICIT_DEF
typedef void *decoder_api_h;
#endif

/**
 * @brief Decoder plugin interface
 * @details
 *	Interace that any _Decoder_-type plugin must obey
 * @warning
 *	The _struct decoder_api_ can only have functions (pure **interface** concept)
 *	For backwards compatibility add only new functions to the end of the struct
 */
struct decoder_api {
	/**
	 * @brief	Constructor of a plugin-specific decoder object
	 * @return
	 *			It must return a handle to the created object to be used on the other methods of the
	 *			_api_ interface
	 */
	decoder_api_h (*create)(void);
	/**
	 * @brief	Destructor of a previously created plugin-specific decoder object
	 * @param	handle	Handle to the decoder-plugin
	 */
	void (*release)(decoder_api_h handle);
	/**
	 * @brief	Intialize a decoding session
	 * @param	handle	Handle to the decoder-plugin
	 */
	void (*initialize_demodulator)(decoder_api_h handle, uint32_t chunk_samples, 
		sample_rx_t demod_data_hi_threshold, sample_rx_t data_offset,
		float samples_per_bit, uint32_t samples_to_file);
	/**
	 * @brief	Terminate a decoding session
	 * @param	handle	Handle to the decoder-plugin
	 */
	void (*terminate_demodulator)(decoder_api_h handle);
	/**
	 * @brief		Decode a chunk of raw received samples to the correponding data
	 * @param[in]	handle					Handle to the decoder-plugin
	 * @param[in]	buffer_in				Buffer with the incoming raw samples
	 * @param[out]	buffer_data_out			Buffer with allocated space for the data output
	 * @param[in]	buffer_data_out_count	Space available in the output buffer in bytes
	 * @return
	 *				The number of data effectively decoded and stored in _buffer_data_out_
	 */
	uint32_t (*parse_next_samples)(decoder_api_h handle, const sample_rx_t *buffer_in,
		data_tx_rx_t *buffer_data_out, uint32_t buffer_data_out_count);
};

#endif /* PLUGINS_DECODER_DECODER_H */

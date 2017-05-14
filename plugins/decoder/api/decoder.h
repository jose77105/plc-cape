/**
 * @file
 * @brief	Public API of _decoder_ plugins
 *
 * @see		@ref plugins-decoder
 * 
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef PLUGINS_DECODER_DECODER_H
#define PLUGINS_DECODER_DECODER_H

struct plc_setting_definition;
union plc_setting_data;

/**
 * @brief PLUGIN API duality. See @ref plugins for more info
 */
#ifndef PLUGINS_API_HANDLE_EXPLICIT_DEF
typedef void *decoder_api_h;
#endif

/**
 * @brief 	Decoder plugin interface
 * @details
 *			Interface that any _Decoder_-type plugin must obey
 * @warning
 *			The _struct decoder_api_ can only have functions (pure **interface** concept)
 *			For backwards compatibility add only new functions to the end of the struct
 */
struct decoder_api
{
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
	 * @brief	Asks for the settings are accepted by the plugin
	 * @param	handle			Handle to the decoder-plugin
	 * @param	accepted_settings_count	The number of accepted settings
	 * @return
	 * 			A presistent pointer with a list of setting definitions.
	 * 	 		The pointer must not be released
	 */
	const struct plc_setting_definition *(*get_accepted_settings)(decoder_api_h handle,
			uint32_t *accepted_settings_count);
	/**
	 * @brief	Switch the plugin to _Configuration mode_
	 * @param	handle	Handle to the decoder-plugin
	 * @return	0 if the _Configuration mode_ can be started; -1 if error
	 */
	int (*begin_settings)(decoder_api_h handle);
	/**
	 * @brief	Set the value of a specific setting. It requires the plugin to be in _Configuration
	 *			mode_
	 * @param	handle		Handle to the decoder-plugin
	 * @param	identifier	Identifier of the setting to be configured
	 * @param	data		New data to be set. The type of it depends on each specific setting
	 * @return	0 if the provided setting and value are valid; -1 if error
	 */
	int (*set_setting)(decoder_api_h handle, const char *identifier, union plc_setting_data data);
	/**
	 * @brief	End the _Configuration mode_ process. The configuration of the plugin is then
	 *			effectively updated with the settings provided
	 * @param	handle	Handle to the decoder-plugin
	 * @return	0 if the whole configuration is coherent and can be applied; -1 if error
	 */
	int (*end_settings)(decoder_api_h handle);
	/**
	 * @brief	Intialize a decoding session
	 * @param	handle			Handle to the decoder-plugin
	 * @param	chunk_samples	Samples of each chunk of buffered data
	 */
	void (*initialize)(decoder_api_h handle, uint32_t chunk_samples);
	/**
	 * @brief	Terminate a decoding session
	 * @param	handle	Handle to the decoder-plugin
	 */
	void (*terminate)(decoder_api_h handle);
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

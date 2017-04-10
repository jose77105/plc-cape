/**
 * @file
 * @brief	Public API of _encoder_ plugins
 * 
 * @see		@ref plugins-encoder
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef PLUGINS_ENCODER_ENCODER_H
#define PLUGINS_ENCODER_ENCODER_H

/**
 * Common configurable settings.
 * A plugin should reuse these predefined symbols whenever possible
 */
enum encoder_setting_enum
{
	encoder_setting_freq_dac_sps = 1,		// float*
	encoder_setting_stream_type,			// enum stream_type_enum
	encoder_setting_offset,					// uint32_t
	encoder_setting_range,					// uint32_t
	encoder_setting_freq,					// float*
	encoder_setting_filename,				// const char*
	encoder_setting_bit_width_us,			// uint32_t
	encoder_setting_message,				// const char*
};

/**
 * Type of wave generation
 */
enum stream_type_enum
{
	stream_ramp = 0,
	stream_triangular,
	stream_constant,
	stream_freq_max,
	stream_freq_max_div2,
	stream_freq_sweep,
	stream_freq_sweep_preload,
	stream_file,
	stream_bit_padding_per_cycle,
	stream_am_modulation,
	stream_freq_sinus,
	stream_ook_pattern,
	stream_ook_hi,
	stream_COUNT
};

/**
 * @brief PLUGIN API duality. See @ref plugins for more info
 */
#ifndef PLUGINS_API_HANDLE_EXPLICIT_DEF
typedef void *encoder_api_h;
#endif

/**
 * @brief	Encoder plugin interface
 * @details
 *			Interface that any _Encoder_-type plugin must obey
 * @warning
 *			* The _struct encoder_api_ can only have functions (pure **interface** concept)\n
 *			* To guarantee backwards compatibility add always new functions to the end of the struct
 *			and never modify the behavior of the existing ones
 */
struct encoder_api {
	/**
	 * @brief	Constructor of a plugin-specific encoder object
	 * @return
	 *			It must return a handle to the created object to be used on the other methods of the
	 *			_api_ interface
	 */
	encoder_api_h (*create)(void);
	/**
	 * @brief	Destructor of a previously created plugin-specific encoder object
	 * @param	handle	Handle to the encoder-plugin
	 */
	void (*release)(encoder_api_h handle);
	/**
	 * @brief	Switch the plugin to _Configuration mode_
	 * @param	handle	Handle to the encoder-plugin
	 * @return	0 if the _Configuration mode_ can be started; -1 if error
	 */
	int (*begin_settings)(encoder_api_h handle);
	/**
	 * @brief	Set the value of a specific setting. It requires the plugin to be in _Configuration
	 *			mode_
	 * @param	handle	Handle to the encoder-plugin
	 * @param	setting	Identifier of the setting to be configured
	 * @param	data	New data to be set. The type of it depends on each specific _setting_
	 * @return	0 if the provided setting and value are valid; -1 if error
	 */
	int (*set_setting)(encoder_api_h handle, enum encoder_setting_enum setting, const void *data);
	/**
	 * @brief	End the _Configuration mode_ process. The configuration of the plugin is then
	 *			effectively updated with the settings provided
	 * @param	handle	Handle to the encoder-plugin
	 * @return	0 if the whole configuration is coherent and can be applied; -1 if error
	 */
	int (*end_settings)(encoder_api_h handle);
	/**
	 * @brief	Reset the encoder-plugin to start a new cycle from a fresh condition
	 * @param	handle	Handle to the encoder-plugin
	 */
	void (*reset)(encoder_api_h handle);
	/**
	 * @brief	Request for filling next consecutive samples
	 * @details
	 *			Function called by the *plc-cape framework* each time a buffer is required to be
	 *			filled with successive content
	 * @param	handle			Handle to the encoder-plugin
	 * @param	buffer			A pointer to the buffer that must be filled by the plugin
	 * @param	buffer_count	The length of the buffer in items 
	 */
	void (*prepare_next_samples)(encoder_api_h handle, sample_tx_t *buffer, uint32_t buffer_count);
};

#endif /* PLUGINS_ENCODER_ENCODER_H */

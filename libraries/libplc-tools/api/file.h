/**
 * @file
 * @brief	Generic file functionality as writing data in Octave-compatible format
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef LIBPLC_TOOLS_FILE_H
#define LIBPLC_TOOLS_FILE_H

/**
 * @brief	Type of binary data used on binary-to-csv conversions
 */
enum csv_type_enum
{
	/// Unsigned 8-bits (BYTE)
	csv_u8 = 0,
	/// Unsigned 16-bits (WORD)
	csv_u16,
	/// Unsigned 32-bits (DWORD)
	csv_u32,
	/// Single precision float (FLOAT)
	csv_float
};

/**
 * @brief	Write a string to a file given a path splited in two parameters (for simplicity)
 * @param	base_path	first part of the path
 * @param	rel_path	second part of the path
 * @param	value		text to write
 * @details
 *	This function can be used with any type of file but is specially aimed to comfortably write
 *	data to device-based files ('/dev/xxx')
 */
void plc_file_write_string(const char *base_path, const char *rel_path, const char *value);
/**
 * @brief	Write a format-based-string to a file given a path splited in two parameters
 *			(for simplicity)
 * @param	base_path	first part of the path
 * @param	rel_path	second part of the path
 * @param	format		a _printf_-style-based format string
 * @param	...			optional arguments as required by _format_
 * @details
 *			This function can be used with any type of file but is specially aimed to comfortably
 *			write data to device-based files ('/dev/xxx')
 */
void plc_file_write_string_format(const char *base_path, const char *rel_path,
		const char *format, ...);
/**
 * @brief	Read a numerical value from a file just reading a string and convertint it to _int_
 * @param	base_path	first part of the path
 * @param	rel_path	second part of the path
 * @return	Numerical value parsed from first text readed from file
 * @details
 *	This function can be used with any type of file but is specially aimed to comfortably read
 *	numerical data from a device-based file ('/dev/xxx')
 */
int plc_file_get_int(const char *base_path, const char *rel_path);
/**
 * @brief	Write a buffer of binary data to a typical 'csv' file
 * @param	filename		target path
 * @param	csv_type_enum	type of binary data conforming the _buffer_
 * @param	buffer			buffer of data to be converted
 * @param	buffer_count	number of items of the buffer
 * @param	append			0 to create a new file, 1 to add files to the end of the file
 * @return	0 if ok, -1 if error (consult _errno_ for extended information)
 */
int plc_file_write_csv(const char *filename, enum csv_type_enum csv_type_enum, void *buffer,
		uint32_t buffer_count, int append);
/**
 * @brief	Write two paired buffers of binary data to a typical 'csv' file (in two columns)
 * @param	filename		target path
 * @param	csv_type_enum_x	type of binary data conforming the _buffer_x_
 * @param	buffer_x		first buffer with the data to be converted
 * @param	csv_type_enum_y	type of binary data conforming the _buffer_y_
 * @param	buffer_y		second buffer witht the data to be converted
 * @param	buffer_count	number of items of both buffers
 * @param	append			0 to create a new file, 1 to add files to the end of the file
 * @return	0 if ok, -1 if error (consult _errno_ for extended information)
 */
int plc_file_write_csv_xy(const char *filename, enum csv_type_enum csv_type_enum_x,
		void *buffer_x, enum csv_type_enum csv_type_enum_y, void *buffer_y, uint32_t buffer_count,
		int append);

#endif /* LIBPLC_TOOLS_FILE_H */

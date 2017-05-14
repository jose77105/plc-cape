/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#define _GNU_SOURCE		// Required for 'asprintf' declaration
#include <err.h>		// warnx
#include <errno.h>		// errno
#include <fcntl.h>		// S_IRUSR
#include <locale.h>		// set_locale
#include <stdarg.h>		// va_start
#include <unistd.h>		// va_start
#include "+common/api/+base.h"
#include "api/file.h"

ATTR_EXTERN void plc_file_write_string(const char *base_path, const char *rel_path,
		const char *value)
{
	char *abs_path;
	int ret = asprintf(&abs_path, "%s%s", base_path, rel_path);
	assert(ret >= 0);
	// Alternative: use 'int fd = open(abs_path, O_WRONLY)' + 'write' + 'close'
	FILE * fp = fopen(abs_path, "w");
	assert(fp != NULL);
	fputs(value, fp);
	fclose(fp);
	free(abs_path);
}

ATTR_EXTERN void plc_file_write_string_format(const char *base_path, const char *rel_path,
		const char *format, ...)
{
	char *abs_path;
	int ret = asprintf(&abs_path, "%s%s", base_path, rel_path);
	assert(ret >= 0);
	// Alternative: use 'int fd = open(abs_path, O_WRONLY)' + 'write' + 'close'
	FILE * fp = fopen(abs_path, "w");
	assert(fp != NULL);
	va_list ap;
	va_start(ap, format);
	vfprintf(fp, format, ap);
	va_end(ap);
	fclose(fp);
	free(abs_path);
}

ATTR_EXTERN int plc_file_get_int(const char *base_path, const char *rel_path)
{
	char *abs_path;
	int ret = asprintf(&abs_path, "%s%s", base_path, rel_path);
	assert(ret >= 0);
	// Alternative: use 'int fd = open(abs_path, O_WRONLY)' + 'write' + 'close'
	FILE * fp = fopen(abs_path, "r");
	assert(fp != NULL);
	int value;
	fscanf(fp, "%d", &value);
	fclose(fp);
	free(abs_path);
	return value;
}

void set_error(const char *msg)
{
	warnx("%s", msg);
}

float csv_buffer_item_to_float_u8(void *buffer_item)
{
	return *(uint8_t*) buffer_item;
}

float csv_buffer_item_to_float_u16(void *buffer_item)
{
	return *(uint16_t*) buffer_item;
}

float csv_buffer_item_to_float_u32(void *buffer_item)
{
	return *(uint32_t*) buffer_item;
}

float csv_buffer_item_to_float_float(void *buffer_item)
{
	return *(float*) buffer_item;
}

// PRECONDITION: 'line' with enough allocated length
int csv_buffer_item_to_text_u8(char *line, void *buffer_item)
{
	// Casting to 'uint16_t' for safety as it's the type expected by the '%hu'
	return sprintf(line, "%hu", (uint16_t) *(uint8_t*) buffer_item);
}

int csv_buffer_item_to_text_u16(char *line, void *buffer_item)
{
	return sprintf(line, "%hu", *(uint16_t*) buffer_item);
}

int csv_buffer_item_to_text_u32(char *line, void *buffer_item)
{
	return sprintf(line, "%u", *(uint32_t*) buffer_item);
}

int csv_buffer_item_to_text_float(char *line, void *buffer_item)
{
	return sprintf(line, "%f", *(float*) buffer_item);
}

void csv_get_buffer_type_data(enum csv_type_enum csv_type_enum, uint32_t *buffer_item_size,
		float (**buffer_item_to_float)(void *buffer_item),
		int (**buffer_item_to_text)(char *line, void *buffer_item))
{
	uint32_t item_size;
	float (*item_to_float)(void *buffer_item);
	int (*item_to_text)(char *line, void *buffer_item);
	switch (csv_type_enum)
	{
	case csv_u8:
		item_size = sizeof(uint8_t);
		item_to_float = csv_buffer_item_to_float_u8;
		item_to_text = csv_buffer_item_to_text_u8;
		break;
	case csv_u16:
		item_size = sizeof(uint16_t);
		item_to_float = csv_buffer_item_to_float_u16;
		item_to_text = csv_buffer_item_to_text_u16;
		break;
	case csv_u32:
		item_size = sizeof(uint32_t);
		item_to_float = csv_buffer_item_to_float_u32;
		item_to_text = csv_buffer_item_to_text_u32;
		break;
	case csv_float:
		item_size = sizeof(float);
		item_to_float = csv_buffer_item_to_float_float;
		item_to_text = csv_buffer_item_to_text_float;
		break;
	default:
		item_size = 0;
		item_to_float = NULL;
		item_to_text = NULL;
		assert(0);
	}
	if (buffer_item_size)
		*buffer_item_size = item_size;
	if (buffer_item_to_float)
		*buffer_item_to_float = item_to_float;
	if (buffer_item_to_text)
		*buffer_item_to_text = item_to_text;
}

ATTR_EXTERN int plc_file_write_csv(const char *filename, enum csv_type_enum csv_type_enum,
		void *buffer, uint32_t buffer_count, int append)
{
	uint32_t buffer_item_size;
	int (*buffer_item_to_text)(char *line, void *buffer_item);
	csv_get_buffer_type_data(csv_type_enum, &buffer_item_size, NULL, &buffer_item_to_text);
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	int file = open(filename, O_CREAT | O_WRONLY | (append ? O_APPEND : O_TRUNC), mode);
	if (file < 0)
	{
		return -1;
	}
	else
	{
		int last_error = 0;
		char *prev_locale = strdup(setlocale(LC_NUMERIC, NULL));
		// Functions like 'printf' use the local information to print decimal points
		//	In European locale it means a ','. By default Octave uses '.' so let's force it
		char *locale = setlocale(LC_NUMERIC, "POSIX");
		assert(locale != NULL);
		for (; buffer_count > 0; buffer_count--)
		{
			char line[32];
			int len = buffer_item_to_text(line, buffer);
			line[len++] = '\n';
			buffer += buffer_item_size;
			int bytes_written = write(file, line, len);
			if (bytes_written < 0)
			{
				last_error = errno;
				break;
			}
			assert(bytes_written == len);
		}
		locale = setlocale(LC_NUMERIC, prev_locale);
		assert(locale != NULL);
		if (last_error)
		{
			errno = last_error;
			return -1;
		}
		return 0;
	}
}

ATTR_EXTERN int plc_file_write_csv_units(const char *filename, enum csv_type_enum csv_type_enum,
		void *buffer, uint32_t buffer_count, float sample_to_unit_x, void *buffer_zero_ref,
		float sample_to_unit_y, int append)
{
	uint32_t buffer_item_size;
	float (*buffer_item_to_float)(void *buffer_item);
	int (*buffer_item_to_text)(char *line, void *buffer_item);
	csv_get_buffer_type_data(csv_type_enum, &buffer_item_size, &buffer_item_to_float,
			&buffer_item_to_text);
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	int file = open(filename, O_CREAT | O_WRONLY | (append ? O_APPEND : O_TRUNC), mode);
	if (file < 0)
	{
		return -1;
	}
	else
	{
		int last_error = 0;
		char *prev_locale = strdup(setlocale(LC_NUMERIC, NULL));
		char *locale = setlocale(LC_NUMERIC, "POSIX");
		assert(locale != NULL);
		float x = 0.0;
		float y_offset = buffer_item_to_float(buffer_zero_ref);
		for (; buffer_count > 0; buffer_count--, x += sample_to_unit_x)
		{
			char line[64];
			int len = sprintf(line, "%f %f\n", x,
					(buffer_item_to_float(buffer) - y_offset) * sample_to_unit_y);
			buffer += buffer_item_size;
			int bytes_written = write(file, line, len);
			if (bytes_written < 0)
			{
				last_error = errno;
				break;
			}
			assert(bytes_written == len);
		}
		close(file);
		locale = setlocale(LC_NUMERIC, prev_locale);
		assert(locale != NULL);
		if (last_error)
		{
			errno = last_error;
			return -1;
		}
		return 0;
	}
}

ATTR_EXTERN int plc_file_write_csv_xy(const char *filename, enum csv_type_enum csv_type_enum_x,
		void *buffer_x, enum csv_type_enum csv_type_enum_y, void *buffer_y, uint32_t buffer_count,
		int append)
{
	uint32_t buffer_item_size_x, buffer_item_size_y;
	int (*buffer_item_to_text_x)(char *line, void *buffer_item);
	int (*buffer_item_to_text_y)(char *line, void *buffer_item);
	csv_get_buffer_type_data(csv_type_enum_x, &buffer_item_size_x, NULL, &buffer_item_to_text_x);
	csv_get_buffer_type_data(csv_type_enum_y, &buffer_item_size_y, NULL, &buffer_item_to_text_y);
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	int file = open(filename, O_CREAT | O_WRONLY | (append ? O_APPEND : O_TRUNC), mode);
	if (file < 0)
	{
		return -1;
	}
	else
	{
		int last_error = 0;
		char *prev_locale = strdup(setlocale(LC_NUMERIC, NULL));
		char *locale = setlocale(LC_NUMERIC, "POSIX");
		assert(locale != NULL);
		for (; buffer_count > 0; buffer_count--)
		{
			char line[64];
			int len = buffer_item_to_text_x(line, buffer_x);
			line[len++] = ' ';
			buffer_x += buffer_item_size_x;
			len += buffer_item_to_text_y(line + len, buffer_y);
			line[len++] = '\n';
			buffer_y += buffer_item_size_y;
			int bytes_written = write(file, line, len);
			if (bytes_written < 0)
			{
				last_error = errno;
				break;
			}
			assert(bytes_written == len);
		}
		close(file);
		locale = setlocale(LC_NUMERIC, prev_locale);
		assert(locale != NULL);
		if (last_error)
		{
			errno = last_error;
			return -1;
		}
		return 0;
	}
}

/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#define _GNU_SOURCE		// Required for 'asprintf' declaration
#include <err.h>		// warnx
#include <errno.h>		// errno
#include <fcntl.h>		// S_IRUSR
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
		int (**buffer_item_to_text)(char *line, void *buffer_item))
{
	switch (csv_type_enum)
	{
	case csv_u8:
		*buffer_item_size = sizeof(uint8_t);
		*buffer_item_to_text = csv_buffer_item_to_text_u8;
		break;
	case csv_u16:
		*buffer_item_size = sizeof(uint16_t);
		*buffer_item_to_text = csv_buffer_item_to_text_u16;
		break;
	case csv_u32:
		*buffer_item_size = sizeof(uint32_t);
		*buffer_item_to_text = csv_buffer_item_to_text_u32;
		break;
	case csv_float:
		*buffer_item_size = sizeof(float);
		*buffer_item_to_text = csv_buffer_item_to_text_float;
		break;
	default:
		*buffer_item_size = 0;
		*buffer_item_to_text = NULL;
		assert(0);
	}
}

ATTR_EXTERN int plc_file_write_csv(const char *filename, enum csv_type_enum csv_type_enum,
		void *buffer, uint32_t buffer_count, int append)
{
	uint32_t buffer_item_size;
	int (*buffer_item_to_text)(char *line, void *buffer_item);
	csv_get_buffer_type_data(csv_type_enum, &buffer_item_size, &buffer_item_to_text);
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	int file = open(filename, O_CREAT | O_WRONLY | (append ? O_APPEND : O_TRUNC), mode);
	if (file < 0)
	{
		return -1;
	}
	else
	{
		for (; buffer_count > 0; buffer_count--)
		{
			char line[32];
			int len = buffer_item_to_text(line, buffer);
			line[len++] = '\n';
			buffer += buffer_item_size;
			int bytes_written = write(file, line, len);
			if (bytes_written < 0)
			{
				int last_error = errno;
				close(file);
				errno = last_error;
				return -1;
			}
			assert(bytes_written == len);
		}
		close(file);
		return 0;
	}
}

ATTR_EXTERN int plc_file_write_csv_xy(const char *filename, enum csv_type_enum csv_type_enum_x,
		void *buffer_x, enum csv_type_enum csv_type_enum_y, void *buffer_y,
		uint32_t buffer_count, int append)
{
	uint32_t buffer_item_size_x, buffer_item_size_y;
	int (*buffer_item_to_text_x)(char *line, void *buffer_item);
	int (*buffer_item_to_text_y)(char *line, void *buffer_item);
	csv_get_buffer_type_data(csv_type_enum_x, &buffer_item_size_x, &buffer_item_to_text_x);
	csv_get_buffer_type_data(csv_type_enum_y, &buffer_item_size_y, &buffer_item_to_text_y);
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	int file = open(filename, O_CREAT | O_WRONLY | (append ? O_APPEND : O_TRUNC), mode);
	if (file < 0)
	{
		return -1;
	}
	else
	{
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
				int last_error = errno;
				close(file);
				errno = last_error;
				return -1;
			}
			assert(bytes_written == len);
		}
		close(file);
		return 0;
	}
}

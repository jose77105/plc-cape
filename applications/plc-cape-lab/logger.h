/**
 * @file
 * @brief	Logging functionality
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef LOGGER_H
#define LOGGER_H

struct logger;

struct logger *logger_create_log_text(void (*log_text)(void *data, const char *text), void *data);
struct logger *logger_create_stdout(void);
struct logger *logger_create_file(const char *filename);
void logger_release(struct logger *logger);
void logger_log_sequence(struct logger *logger, const char *text);
void logger_log_line(struct logger *logger, const char *text);
void logger_log_sequence_format(struct logger *logger, const char *format, ...);
void logger_log_sequence_format_va(struct logger *logger, const char *format, va_list args);
void logger_log_error(struct logger *logger, const char *text, int error);
void logger_log_last_error(struct logger *logger, const char *text);

#endif /* LOGGER_H */

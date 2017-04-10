/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#define _GNU_SOURCE		// Required for 'vasprintf' declaration
#include <errno.h>		// errno
#include <pthread.h>	// pthread_mutex_t
#include <stdarg.h>		// va_list
#include "+common/api/+base.h"
#include "common.h"

#include "logger.h"

struct logger
{
	pthread_mutex_t log_line_lock;
	void (*release)(struct logger *logger);
	void *data;
	void (*log_text)(void *data, const char *text);
};

static struct logger *logger_create_base(void)
{
	struct logger *logger = (struct logger*) calloc(1, sizeof(struct logger));
	int ret = pthread_mutex_init(&logger->log_line_lock, NULL);
	assert(ret == 0);
	return logger;
}

struct logger *logger_create_log_text(void (*log_text)(void *data, const char *text), void *data)
{
	struct logger *logger = logger_create_base();
	logger->data = data;
	logger->log_text = log_text;
	return logger;
}

struct logger *logger_create_stdout(void)
{
	struct logger *logger = logger_create_base();
	logger->data = stdout;
	logger->log_text = (void*) fprintf;
	return logger;
}

void logger_file_release(struct logger *logger)
{
	fclose(logger->data);
}

struct logger *logger_create_file(const char *filename)
{
	struct logger *logger = logger_create_base();
	FILE *file = fopen(filename, "w");
	logger->release = logger_file_release;
	logger->data = file;
	logger->log_text = (void*) fprintf;
	return logger;
}

void logger_release(struct logger *logger)
{

	if (logger->release)
		logger->release(logger);
	pthread_mutex_destroy(&logger->log_line_lock);
	free(logger);
}

static void logger_log(struct logger *logger, const char *text, int add_newline)
{
	// As this function is shared among different thread -> mutual exclusion
	pthread_mutex_lock(&logger->log_line_lock);
	logger->log_text(logger->data, text);
	if (add_newline)
		logger->log_text(logger->data, "\n");
	pthread_mutex_unlock(&logger->log_line_lock);
}

void logger_log_sequence(struct logger *logger, const char *text)
{
	logger_log(logger, text, 0);
}

void logger_log_line(struct logger *logger, const char *text)
{
	logger_log(logger, text, 1);
}

void logger_log_sequence_format_va(struct logger *logger, const char *format, va_list args)
{
	char *text;
	int ret = vasprintf(&text, format, args);
	assert(ret >= 0);
	logger_log(logger, text, 0);
	free(text);
}

void logger_log_sequence_format(struct logger *logger, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	logger_log_sequence_format_va(logger, format, args);
	va_end(args);
}

void logger_log_error(struct logger *logger, const char *text, int error)
{
	// On console mode we could use 'perror(text)' to have the same effects
	logger_log_sequence_format(logger, "%s: %s\n", text, strerror(error));
}

void logger_log_last_error(struct logger *logger, const char *text)
{
	logger_log_error(logger, text, errno);
}

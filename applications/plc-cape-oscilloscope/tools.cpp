/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include <math.h>		// fabs
#include "+common/api/+base.h"
#include "tools.h"

static int sprint_engineering_notation(char *text, float value, const char *units);

int sprint_custom_float_pair(char *text, uint16_t text_size, float time_us, float yunit,
		const char *yunit_units)
{
	// Basic format example to print the coordinates:
	//	sprintf(text, "[%.2f us; %.2f V] ", time_us, yunit);
	char *text_cur = text;
	*text_cur++ = '[';
	text_cur += sprint_custom_float(text_cur, time_us / 1000000.0, "s");
	*text_cur++ = ';';
	*text_cur++ = ' ';
	text_cur += sprint_custom_float(text_cur, yunit, yunit_units);
	*text_cur++ = ']';
	*text_cur = '\0';
	int chars = text_cur - text;
	assert(chars < text_size);
	return chars;
}

int sprint_custom_float_pair_fft(char *text, uint16_t text_size, float freq_hz, float yrange_factor)
{
	char *text_cur = text;
	*text_cur++ = '[';
	text_cur += sprint_custom_float(text_cur, freq_hz, "Hz");
	*text_cur++ = ';';
	*text_cur++ = ' ';
	text_cur += sprint_custom_float(text_cur, yrange_factor, NULL, 3);
	*text_cur++ = ']';
	*text_cur = '\0';
	int chars = text_cur - text;
	assert(chars < text_size);
	return chars;
}

int sprint_custom_float(char *text, float value, const char *units)
{
	// TODO: Set the precision in a better way
	return sprint_custom_float(text, value, units, ((units == NULL) || (*units == '\0')) ? 0 : 3);
}

int sprint_custom_float(char *text, float value, const char *units, uint8_t decimal_digits)
{
	if ((units == NULL) || (*units == '\0'))
	{
		if (decimal_digits == 0)
			return sprintf(text, "%d", (int) (value + .5));
		else
			return sprintf(text, "%.*f", decimal_digits, value);
	}
	else
	{
		static const int in_engineering_notation = true;
		if (in_engineering_notation)
			return sprint_engineering_notation(text, value, units);
		else
			return sprintf(text, "%.*g %s", decimal_digits, value, units);
	}
}

int sprint_engineering_notation(char *text, float value, const char *units)
{
	static const char si_char[] = {
		'p', 'n', 'u', 'm', ' ', 'k', 'M', 'G', 'T' };
	// Calculate the number of digits
	if (value == 0.0)
		return sprintf(text, "0 %s", units);
	float abs_value = fabs(value);
	static const int8_t zero_factor_index = 4;
	int8_t si_index = floor(log10(abs_value)) / 3 + zero_factor_index;
	if (si_index < 0)
		si_index = 0;
	else if ((uint8_t) si_index >= sizeof(si_char))
		si_index = sizeof(si_char) - 1;
	// NOTE: with the '%.Ng' flag the 'N' indicates the maximum number of significant digits
	// Examples of applied notation using 3 significant digits:
	// 	1234 = 1.23 k; 12345 = 12.3 k; 123456 = 123 k; 1234567 = 1.23 M
	if (si_index == zero_factor_index)
	{
		return sprintf(text, "%.3g %s", value, units);
	}
	else
	{
		return sprintf(text, "%.3g %c%s", value / pow(10, (si_index - zero_factor_index) * 3),
				si_char[si_index], units);
	}
}

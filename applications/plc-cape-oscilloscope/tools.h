/**
 * @file
 * @brief	Shared tooling functions
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef TOOLS_H
#define TOOLS_H

#define SAMPLES_MAX_RANGE 0x10000
#define SAMPLES_ZERO_REF 0x8000

extern class Configuration *app_cfg;

int sprint_custom_float_pair(char *text, uint16_t text_size, float time_us, float yunit,
		const char *yunit_units);
int sprint_custom_float_pair_fft(char *text, uint16_t text_size, float freq_hz,
		float yrange_factor);
int sprint_custom_float(char *text, float value, const char *units);
int sprint_custom_float(char *text, float value, const char *units, uint8_t decimal_digits);

#endif /* TOOLS_H */

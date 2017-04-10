/**
 * @file
 * @brief	Internal interface for AFE031 management
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef AFE_H
#define AFE_H

struct plc_afe;
struct plc_gpio;
struct spi;

struct plc_afe *plc_afe_create(struct plc_gpio *plc_gpio, struct spi *spi);
void plc_afe_release(struct plc_afe *plc_afe);
struct spi *plc_afe_get_spi(struct plc_afe *plc_afe);

#endif /* AFE_H */

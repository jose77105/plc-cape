/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include "+common/api/+base.h"
#include "afe_commands.h"

ATTR_INTERN const uint8_t AFEREG_GAIN_SELECT_RX_PGA1[] =
{ 0x0, 0x1, 0x2, 0x3 };

ATTR_INTERN const uint8_t AFEREG_GAIN_SELECT_RX_PGA2[] =
{ 0x0, 0x4, 0x8, 0xC };

ATTR_INTERN const uint8_t AFEREG_GAIN_SELECT_TX_PGA[] =
{ 0x00, 0x10, 0x20, 0x30 };

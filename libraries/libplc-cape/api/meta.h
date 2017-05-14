/**
 * @file
 * @brief	Meta-library functions
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016-2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef LIBPLC_CAPE_META_H
#define LIBPLC_CAPE_META_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief	Sets a provider for global singletons to be used by the library
 * @param	callback	Callback function to ask for singletons
 * @param	handle		Handler to _singletons_provider_
 */
void plc_libcape_set_singletons_provider(singletons_provider_get_t callback,
		singletons_provider_h handle);

#ifdef __cplusplus
}
#endif

#endif /* LIBPLC_CAPE_META_H */

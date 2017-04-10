/**
 * @file
 * @brief	Manage the singleton (global) objects that can be provided to libraries or plugins if
 *			supported by them
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef SINGLETONS_PROVIDER_H
#define SINGLETONS_PROVIDER_H

void singletons_provider_initialize(void);
void singletons_provider_configure_plugin(plugin_set_singletons_provider_t
		plugin_set_singletons_provider);

#endif /* SINGLETONS_PROVIDER_H */

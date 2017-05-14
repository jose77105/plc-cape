/**
 * @file
 * @brief	Application settings
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <libxml/xmlreader.h>
#include <gtk/gtk.h>
#include "libraries/libplc-tools/api/settings.h"	// plc_setting_named_list
#include "plot_widget.h"
#include "plot_area.h"

class Configuration_interface;

class Configuration
{
public:
	Configuration();
	virtual ~Configuration();
	int load_configuration(const char *filename);
	int set_configuration(const char *section, Configuration_interface *configuration);

private:
	struct setting_section
	{
		char *name;
		uint32_t capacity;
		uint32_t count;
		struct plc_setting *settings;
	};

	void release_setting_sections();
	int xml_parse_configuration(xmlTextReaderPtr reader);
	int xml_parse_section_settings(xmlTextReaderPtr reader, struct setting_section *section);
	int xml_parse_raw_setting(xmlTextReaderPtr reader, struct setting_section *section);
	int xml_skip_section(xmlTextReaderPtr reader);
	void add_setting(struct setting_section *setting_section, const struct plc_setting *setting);

	struct setting_section* setting_sections;
	uint32_t setting_sections_count;
};

#endif /* CONFIGURATION_H */

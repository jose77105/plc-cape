/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include "+common/api/+base.h"
#include "configuration.h"
#include "configuration_interface.h"

#define SETTING_SECTION_GRANULARITY 16

// TODO: Remove the requirement to define here the 'log_format' used by the 'libplc-tools' library
extern "C" void log_format(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	g_logv(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, format, args);
	va_end(args);
}

Configuration::Configuration()
{
	setting_sections = NULL;
	setting_sections_count = 0;
}

Configuration::~Configuration()
{
	release_setting_sections();
}

int Configuration::load_configuration(const char *filename)
{
	int ret;
	assert(filename != NULL);
	release_setting_sections();
	xmlTextReaderPtr reader = xmlReaderForFile(filename, NULL, 0);
	if (reader != NULL)
	{
		while ((ret = xmlTextReaderRead(reader)) == 1)
		{
			xmlReaderTypes type = (xmlReaderTypes) xmlTextReaderNodeType(reader);
			if (type == XML_READER_TYPE_ELEMENT)
			{
				const char *name = (const char *) xmlTextReaderConstName(reader);
				assert(xmlTextReaderDepth(reader) == 0);
				if (strcmp(name, "configuration") == 0)
					ret = xml_parse_configuration(reader);
				else
					ret = xml_skip_section(reader);
				if (ret != 1)
					break;
			}
		}
		xmlFreeTextReader(reader);
		if (ret != 0)
		{
			log_format("%s : failed to parse\n", filename);
			ret = -1;
		}
	}
	else
	{
		log_format("Unable to open %s\n", filename);
		ret = -1;
	}
	return ret;
}

int Configuration::set_configuration(const char *section, Configuration_interface *configuration)
{
	struct setting_section *setting_section = setting_sections;
	uint32_t n;
	int ret;
	for (n = 0; n < setting_sections_count; n++, setting_section++)
	{
		if (strcmp(setting_section->name, section) == 0)
			break;
	}
	if (n < setting_sections_count)
	{
		if ((ret = configuration->begin_configuration()) == 0)
		{
			struct plc_setting *setting = setting_section->settings;
			for (n = 0; n < setting_section->count; setting++, n++)
			{
				assert(setting->type == plc_setting_string);
				ret = configuration->set_configuration_setting(setting->identifier,
						setting->data.s);
				assert(ret == 0);
			}
			ret = configuration->end_configuration();
			assert(ret == 0);
		}
		else
		{
			assert(0);
			return ret;
		}
	}
	return 0;
}

void Configuration::release_setting_sections(void)
{
	struct setting_section* setting_section = setting_sections;
	for (; setting_sections_count > 0; setting_sections_count--, setting_section++)
		free(setting_section->name);
	free(setting_sections);
	setting_sections = NULL;
}

int Configuration::xml_parse_configuration(xmlTextReaderPtr reader)
{
	int ret;
	while ((ret = xmlTextReaderRead(reader)) == 1)
	{
		xmlReaderTypes type = (xmlReaderTypes) xmlTextReaderNodeType(reader);
		if (type == XML_READER_TYPE_ELEMENT)
		{
			const xmlChar *section_name = xmlTextReaderConstName(reader);
			assert(xmlTextReaderDepth(reader) == 1);
			++setting_sections_count;
			setting_sections = (struct setting_section*) realloc(setting_sections,
					setting_sections_count * sizeof(struct setting_section));
			struct setting_section *setting_section = &setting_sections[setting_sections_count - 1];
			memset(setting_section, 0, sizeof(*setting_section));
			setting_section->name = strdup((const char*) section_name);
			ret = xml_parse_section_settings(reader, setting_section);
			if (ret != 1)
				return ret;
		}
		else if (type == XML_READER_TYPE_END_ELEMENT)
		{
			return 1;
		}
	}
	return ret;
}

int Configuration::xml_parse_section_settings(xmlTextReaderPtr reader,
		struct setting_section *section)
{
	int ret;
	// Accept a section with the self-ending format like '<section-settings/>'
	if (xmlTextReaderIsEmptyElement(reader))
		return 1;
	while ((ret = xmlTextReaderRead(reader)) == 1)
	{
		xmlReaderTypes type = (xmlReaderTypes) xmlTextReaderNodeType(reader);
		if (type == XML_READER_TYPE_ELEMENT)
		{
			const xmlChar *name = xmlTextReaderConstName(reader);
			if (xmlStrcmp(name, BAD_CAST "setting") == 0)
				ret = xml_parse_raw_setting(reader, section);
			else
				ret = xml_skip_section(reader);
			if (ret != 1)
				return ret;
		}
		else if (type == XML_READER_TYPE_END_ELEMENT)
		{
			return 1;
		}
	}
	return ret;
}

int Configuration::xml_parse_raw_setting(xmlTextReaderPtr reader, struct setting_section *section)
{
	int ret;
	// Accept a section with the self-ending format like '<raw-setting id="setting1"/>'
	if (xmlTextReaderIsEmptyElement(reader))
		return 1;
	assert(xmlTextReaderAttributeCount(reader) == 1);
	ret = xmlTextReaderMoveToFirstAttribute(reader);
	if (ret != 1)
		return ret;
	const xmlChar *attribute_id = xmlTextReaderName(reader);
	assert(xmlStrcmp(attribute_id, BAD_CAST "id") == 0);
	const xmlChar *setting_identifier = xmlTextReaderValue(reader);
	while ((ret = xmlTextReaderRead(reader)) == 1)
	{
		xmlReaderTypes type = (xmlReaderTypes) xmlTextReaderNodeType(reader);
		if (type == XML_READER_TYPE_TEXT)
		{
			const xmlChar *setting_value = xmlTextReaderConstValue(reader);
			if (setting_value == NULL)
				setting_value = BAD_CAST "";
			plc_setting setting;
			// TODO: Make proper conversion from xmlChar (UTF8) to 'char *'
			setting.identifier = (char *) setting_identifier;
			setting.type = plc_setting_string;
			// TODO: Make proper conversion from xmlChar (UTF8) to 'char *'
			setting.data.s = (char *) setting_value;
			add_setting(section, &setting);
		}
		else if (type == XML_READER_TYPE_END_ELEMENT)
		{
			return 1;
		}
	}
	return ret;
}

int Configuration::xml_skip_section(xmlTextReaderPtr reader)
{
	int ret;
	while ((ret = xmlTextReaderRead(reader)) == 1)
	{
		xmlReaderTypes type = (xmlReaderTypes) xmlTextReaderNodeType(reader);
#ifdef DUMP_SKIPPED_XML
		if (type != XML_READER_TYPE_SIGNIFICANT_WHITESPACE)
		{
			const xmlChar *name = xmlTextReaderConstName(reader);
			const xmlChar *value = xmlTextReaderConstValue(reader);
			log_format("%d %d %s %d %d %s\n", xmlTextReaderDepth(reader), xmlTextReaderNodeType(reader),
					(name != NULL) ? name : BAD_CAST "--", xmlTextReaderIsEmptyElement(reader),
					xmlTextReaderHasValue(reader), (value != NULL) ? value : BAD_CAST "");
		}
#endif
		if (type == XML_READER_TYPE_ELEMENT)
		{
			if (!xmlTextReaderIsEmptyElement(reader))
			{
				ret = xml_skip_section(reader);
				if (ret != 1)
					return ret;
			}
		}
		else if (type == XML_READER_TYPE_END_ELEMENT)
		{
			return 1;
		}
	}
	return ret;
}

void Configuration::add_setting(struct setting_section *setting_section,
		const struct plc_setting *setting)
{
	if (setting_section->count == setting_section->capacity)
	{
		setting_section->capacity += SETTING_SECTION_GRANULARITY;
		setting_section->settings = (struct plc_setting *) realloc(setting_section->settings,
				setting_section->capacity * sizeof(struct plc_setting));
	}
	plc_setting_copy(&setting_section->settings[setting_section->count++], setting);
}


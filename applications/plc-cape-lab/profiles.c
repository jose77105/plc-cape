/**
 * @file
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#include <libxml/xmlreader.h>
#include "+common/api/+base.h"
#include "+common/api/setting.h"
#include "common.h"
#include "libraries/libplc-tools/api/settings.h"
#include "libraries/libplc-cape/api/tx.h"
#include "profiles.h"
#include "rx.h"
#include "settings.h"

#define PROFILES_VECTOR_GRANULARITY 16

#define SAMPLES_TO_FILE_DEFAULT 10000

struct setting_vector
{
	char *plugin;
	uint32_t capacity;
	uint32_t count;
	struct plc_setting *settings;
};

struct profile
{
	char *identifier;
	char *title;
	char *inherit;
	int hidden;
	struct setting_vector app_settings;
	struct setting_vector encoder_settings;
	struct setting_vector decoder_settings;
};

struct profile_list_item
{
	struct profile_list_item *next;
	struct profile profile;
};

struct profiles
{
	xmlTextReaderPtr reader;
	uint32_t profile_list_count;
	struct profile_list_item *profile_list_head;
	struct profile_list_item *profile_list_cur;
	struct setting_vector *setting_vector_cur;
	struct tree_node_vector *profile_tree;
};

static void profiles_add_setting(struct setting_vector *setting_vector,
		const struct plc_setting *setting)
{
	if (setting_vector->count == setting_vector->capacity)
	{
		setting_vector->capacity += PROFILES_VECTOR_GRANULARITY;
		setting_vector->settings = realloc(setting_vector->settings,
				setting_vector->capacity * sizeof(struct plc_setting));
	}
	plc_setting_copy(&setting_vector->settings[setting_vector->count], setting);
	setting_vector->count++;
}

static void profiles_add_setting_string(struct setting_vector *setting_vector,
		const char *setting_identifier, const char *setting_value)
{
	struct plc_setting setting;
	setting.identifier = (char*) setting_identifier;
	setting.type = plc_setting_string;
	setting.data.s = (char*) setting_value;
	profiles_add_setting(setting_vector, &setting);
}

static void profiles_add_profile(struct profiles *profiles, const char *profile_id,
		const char *profile_inherit, const char *profile_title, int hidden)
{
	struct profile_list_item *list_item;
	list_item = calloc(1, sizeof(struct profile_list_item));
	list_item->next = NULL;
	list_item->profile.identifier = strdup(profile_id);
	list_item->profile.title = strdup(profile_title ? profile_title : profile_id);
	list_item->profile.hidden = hidden;
	if (profile_inherit)
		list_item->profile.inherit = strdup(profile_inherit);
	if (profiles->profile_list_head == NULL)
		profiles->profile_list_head = list_item;
	else
		profiles->profile_list_cur->next = list_item;
	profiles->profile_list_cur = list_item;
	profiles->profile_list_count++;
}

static int profiles_xml_skip_section(struct profiles *profiles)
{
	int ret;
	while ((ret = xmlTextReaderRead(profiles->reader)) == 1)
	{
		xmlReaderTypes type = xmlTextReaderNodeType(profiles->reader);
#ifdef DUMP_SKIPPED_XML
		if (type != XML_READER_TYPE_SIGNIFICANT_WHITESPACE)
		{
			const xmlChar *name = xmlTextReaderConstName(profiles->reader);
			const xmlChar *value = xmlTextReaderConstValue(profiles->reader);
			printf("%d %d %s %d %d %s\n", xmlTextReaderDepth(profiles->reader),
					xmlTextReaderNodeType(profiles->reader),
					(name != NULL) ? name : BAD_CAST "--",
					xmlTextReaderIsEmptyElement(profiles->reader),
					xmlTextReaderHasValue(profiles->reader),
					(value != NULL) ? value : BAD_CAST "");
		}
#endif
		if (type == XML_READER_TYPE_ELEMENT)
		{
			if (!xmlTextReaderIsEmptyElement(profiles->reader))
			{
				ret = profiles_xml_skip_section(profiles);
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

static int profiles_xml_parse_setting(struct profiles *profiles)
{
	int ret;
	// Don't accept, for the time being, a self-ended setting with empty content as:
	//	<setting id="freq_bps"/>
	// On future we could support this giving a specific semantics, as use the recommended value
	// from the plugin
	assert(!xmlTextReaderIsEmptyElement(profiles->reader));
	assert(xmlTextReaderAttributeCount(profiles->reader) == 1);
	ret = xmlTextReaderMoveToFirstAttribute(profiles->reader);
	if (ret != 1)
		return ret;
	const xmlChar *attribute_id = xmlTextReaderName(profiles->reader);
	assert(xmlStrcmp(attribute_id, BAD_CAST "id") == 0);
	const xmlChar *setting_identifier = xmlTextReaderValue(profiles->reader);
	while ((ret = xmlTextReaderRead(profiles->reader)) == 1)
	{
		xmlReaderTypes type = xmlTextReaderNodeType(profiles->reader);
		if (type == XML_READER_TYPE_TEXT)
		{
			const xmlChar *setting_value = xmlTextReaderConstValue(profiles->reader);
			if (setting_value == NULL)
				setting_value = BAD_CAST "";
			// TODO: Make proper conversion from xmlChar (UTF8) to 'const char *'
			profiles_add_setting_string(profiles->setting_vector_cur,
					(const char *) setting_identifier, (const char *) setting_value);
		}
		else if (type == XML_READER_TYPE_END_ELEMENT)
		{
			return 1;
		}
	}
	return ret;
}

static int profiles_xml_parse_settings(struct profiles *profiles,
		struct setting_vector *setting_vector)
{
	profiles->setting_vector_cur = setting_vector;
	int ret;
	int self_ended_tag = xmlTextReaderIsEmptyElement(profiles->reader);
	const xmlChar *settings_plugin = NULL;
	for (ret = xmlTextReaderMoveToFirstAttribute(profiles->reader); ret != 0; ret =
			xmlTextReaderMoveToNextAttribute(profiles->reader))
	{
		const xmlChar *id = xmlTextReaderName(profiles->reader);
		if (xmlStrcmp(id, BAD_CAST "plugin") == 0)
			settings_plugin = xmlTextReaderValue(profiles->reader);
	}
	if (settings_plugin != NULL)
	{
		assert(setting_vector->plugin == NULL);
		setting_vector->plugin = strdup((const char *) settings_plugin);
	}
	// Accept a settings-section without settings with the self-ending format as:
	//	<decoder-settings plugin="decoder-ook"/>
	if (self_ended_tag)
		return 1;
	while ((ret = xmlTextReaderRead(profiles->reader)) == 1)
	{
		xmlReaderTypes type = xmlTextReaderNodeType(profiles->reader);
		if (type == XML_READER_TYPE_ELEMENT)
		{
			const xmlChar *name = xmlTextReaderConstName(profiles->reader);
			if (xmlStrcmp(name, BAD_CAST "setting") == 0)
				ret = profiles_xml_parse_setting(profiles);
			else
				ret = profiles_xml_skip_section(profiles);
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

static int profiles_xml_parse_profile(struct profiles *profiles)
{
	int ret;
	assert(!xmlTextReaderIsEmptyElement(profiles->reader));
	const xmlChar *profile_id = NULL;
	const xmlChar *profile_inherit = NULL;
	const xmlChar *profile_title = NULL;
	int hidden = 0;
	for (ret = xmlTextReaderMoveToFirstAttribute(profiles->reader); ret != 0; ret =
			xmlTextReaderMoveToNextAttribute(profiles->reader))
	{
		const xmlChar *id = xmlTextReaderName(profiles->reader);
		if (xmlStrcmp(id, BAD_CAST "id") == 0)
			profile_id = xmlTextReaderValue(profiles->reader);
		else if (xmlStrcmp(id, BAD_CAST "inherit") == 0)
			profile_inherit = xmlTextReaderValue(profiles->reader);
		else if (xmlStrcmp(id, BAD_CAST "title") == 0)
			profile_title = xmlTextReaderValue(profiles->reader);
		else if (xmlStrcmp(id, BAD_CAST "hidden") == 0)
			hidden = (xmlStrcmp(xmlTextReaderValue(profiles->reader), BAD_CAST "0") != 0);
	}
	assert(profile_id != NULL);
	// TODO: Make proper conversion from xmlChar (UTF8) to 'const char *'
	profiles_add_profile(profiles, (const char *) profile_id, (const char *) profile_inherit,
			(const char *) profile_title, hidden);
	while ((ret = xmlTextReaderRead(profiles->reader)) == 1)
	{
		xmlReaderTypes type = xmlTextReaderNodeType(profiles->reader);
		if (type == XML_READER_TYPE_ELEMENT)
		{
			const xmlChar *name = xmlTextReaderConstName(profiles->reader);
			assert(xmlTextReaderDepth(profiles->reader) == 3);
			if (xmlStrcmp(name, BAD_CAST "app-settings") == 0)
				ret = profiles_xml_parse_settings(profiles,
						&profiles->profile_list_cur->profile.app_settings);
			else if (xmlStrcmp(name, BAD_CAST "encoder-settings") == 0)
				ret = profiles_xml_parse_settings(profiles,
						&profiles->profile_list_cur->profile.encoder_settings);
			else if (xmlStrcmp(name, BAD_CAST "decoder-settings") == 0)
				ret = profiles_xml_parse_settings(profiles,
						&profiles->profile_list_cur->profile.decoder_settings);
			else
				ret = profiles_xml_skip_section(profiles);
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

static int profiles_xml_parse_profiles(struct profiles *profiles)
{
	int ret;
	if (xmlTextReaderIsEmptyElement(profiles->reader))
		return 1;
	while ((ret = xmlTextReaderRead(profiles->reader)) == 1)
	{
		xmlReaderTypes type = xmlTextReaderNodeType(profiles->reader);
		if (type == XML_READER_TYPE_ELEMENT)
		{
			const xmlChar *name = xmlTextReaderConstName(profiles->reader);
			assert(xmlTextReaderDepth(profiles->reader) == 2);
			if (xmlStrcmp(name, BAD_CAST "profile") == 0)
				ret = profiles_xml_parse_profile(profiles);
			else
				ret = profiles_xml_skip_section(profiles);
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

//
// XML: <tree>
//
// TODO: Release tree
static void profiles_add_tree_profile(struct profile_identifier_vector *profile_identifier_vector,
		const char *profile_identifier)
{
	if (profile_identifier_vector->count == profile_identifier_vector->capacity)
	{
		profile_identifier_vector->capacity += PROFILES_VECTOR_GRANULARITY;
		profile_identifier_vector->identifiers = realloc(profile_identifier_vector->identifiers,
				profile_identifier_vector->capacity * sizeof(const char*));
	}
	profile_identifier_vector->identifiers[profile_identifier_vector->count] = strdup(
			profile_identifier);
	profile_identifier_vector->count++;
}

static void profiles_add_tree_node(struct tree_node_vector *tree_node_vector, const char *title)
{
	if (tree_node_vector->nodes_count == tree_node_vector->nodes_capacity)
	{
		tree_node_vector->nodes_capacity += PROFILES_VECTOR_GRANULARITY;
		tree_node_vector->nodes = realloc(tree_node_vector->nodes,
				tree_node_vector->nodes_capacity * sizeof(struct tree_node_vector));
	}
	tree_node_vector->nodes[tree_node_vector->nodes_count].title = strdup(title);
	tree_node_vector->nodes_count++;
}

static int profiles_xml_parse_tree_node(struct profiles *profiles,
		struct tree_node_vector *tree_node)
{
	int ret;
	if (xmlTextReaderIsEmptyElement(profiles->reader))
		return 1;
	while ((ret = xmlTextReaderRead(profiles->reader)) == 1)
	{
		xmlReaderTypes type = xmlTextReaderNodeType(profiles->reader);
		if (type == XML_READER_TYPE_ELEMENT)
		{
			const xmlChar *name = xmlTextReaderConstName(profiles->reader);
			if (xmlStrcmp(name, BAD_CAST "node") == 0)
			{
				int ret = xmlTextReaderMoveToAttribute(profiles->reader, BAD_CAST "title");
				assert(ret == 1);
				const xmlChar *title = xmlTextReaderValue(profiles->reader);
				profiles_add_tree_node(tree_node, (const char *) title);
				ret = profiles_xml_parse_tree_node(profiles,
						&tree_node->nodes[tree_node->nodes_count - 1]);
			}
			else if (xmlStrcmp(name, BAD_CAST "profile") == 0)
			{
				assert(xmlTextReaderIsEmptyElement(profiles->reader));
				int ret = xmlTextReaderMoveToAttribute(profiles->reader, BAD_CAST "id");
				assert(ret == 1);
				const xmlChar *profile_id = xmlTextReaderValue(profiles->reader);
				profiles_add_tree_profile(&tree_node->profile_identifiers,
						(const char *) profile_id);
			}
			else
			{
				ret = -1;
			}
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

//
// XML: </tree>
//

static int profiles_xml_parse_profile_definitions(struct profiles *profiles)
{
	int ret;
	while ((ret = xmlTextReaderRead(profiles->reader)) == 1)
	{
		xmlReaderTypes type = xmlTextReaderNodeType(profiles->reader);
		if (type == XML_READER_TYPE_ELEMENT)
		{
			const xmlChar *name = xmlTextReaderConstName(profiles->reader);
			assert(xmlTextReaderDepth(profiles->reader) == 1);
			if (xmlStrcmp(name, BAD_CAST "profiles") == 0)
			{
				ret = profiles_xml_parse_profiles(profiles);
			}
			else if (xmlStrcmp(name, BAD_CAST "tree") == 0)
			{
				profiles->profile_tree = calloc(1, sizeof(struct tree_node_vector));
				ret = profiles_xml_parse_tree_node(profiles, profiles->profile_tree);
			}
			else
			{
				ret = profiles_xml_skip_section(profiles);
			}
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

static int profiles_xml_parse_file(struct profiles *profiles, const char *filename)
{
	int ret;
	profiles->reader = xmlReaderForFile(filename, NULL, 0);
	if (profiles->reader != NULL)
	{
		while ((ret = xmlTextReaderRead(profiles->reader)) == 1)
		{
			xmlReaderTypes type = xmlTextReaderNodeType(profiles->reader);
			if (type == XML_READER_TYPE_ELEMENT)
			{
				const char *name = (const char *) xmlTextReaderConstName(profiles->reader);
				assert(xmlTextReaderDepth(profiles->reader) == 0);
				if (strcmp(name, "plc-cape-lab.profile_definitions") == 0)
					ret = profiles_xml_parse_profile_definitions(profiles);
				else
					ret = profiles_xml_skip_section(profiles);
				if (ret != 1)
					break;
			}
		}
		xmlFreeTextReader(profiles->reader);
		if (ret != 0)
		{
			fprintf(stderr, "%s : failed to parse\n", filename);
			return -1;
		}
	}
	else
	{
		fprintf(stderr, "Unable to open %s\n", filename);
		return -1;
	}
	return 0;
}

// TODO: Revise why the use of 'const' (recommended) produces several warnings
//	typedef const struct plc_setting plc_setting_t;
typedef struct plc_setting plc_setting_t;

static plc_setting_t profile_loopback[] = {
	{
		"operating_mode", plc_setting_enum, {
			.u32 = operating_mode_calib_dac_txpga } }, {
		"tx_mode", plc_setting_enum, {
			.u32 = spi_tx_mode_ping_pong_dma } }, {
		"rx_mode", plc_setting_enum, {
			.u32 = rx_mode_kernel_buffering } }, {
		"data_offset", plc_setting_u16, {
			.u16 = 1100 } }, {
		"data_hi_threshold_detection", plc_setting_u16, {
			.u16 = 20 } }, {
		"samples_to_file", plc_setting_u16, {
			.u16 = SAMPLES_TO_FILE_DEFAULT } }, {
		"tx_buffers_len", plc_setting_u32, {
			.u32 = 1000 } }, };

static plc_setting_t profile_txpga[] = {
	{
		"operating_mode", plc_setting_enum, {
			.u32 = operating_mode_tx_dac_txpga_txfilter } }, {
		"rx_mode", plc_setting_enum, {
			.u32 = rx_mode_none } }, {
		"tx_mode", plc_setting_enum, {
			.u32 = spi_tx_mode_ping_pong_dma } }, {
		"freq_bps", plc_setting_u32, {
			.u32 = 750000 } }, {
		"tx_buffers_len", plc_setting_u32, {
			.u32 = 1000 } }, };

// NOTE:
// For a plugin enumeration we could use the numerical value if we know the corresponding id:
//	... "stream_type", plc_setting_enum, { .u32 = 10 } ...
// But, on plugins, only 'string' identifiers are required to be preserved for compatibility
// So, for future compatibility it is recommended to use the string value instead:
//	... "stream_type", plc_setting_string, { .s = "freq_sinus" } ...

static plc_setting_t profile_encoder_wave_2kHz_O800[] = {
	{
		"stream_type", plc_setting_string, {
			.s = "freq_sinus" } }, {
		"freq", plc_setting_float, {
			.f = 2000 } }, {
		"offset", plc_setting_u16, {
			.u16 = 801 } }, {
		"range", plc_setting_u16, {
			.u32 = 400 } }, };

static plc_setting_t profile_encoder_wave_2kHz_O500[] = {
	{
		"stream_type", plc_setting_string, {
			.s = "freq_sinus" } }, {
		"freq", plc_setting_float, {
			.f = 2000 } }, {
		"offset", plc_setting_u16, {
			.u16 = 500 } }, {
		"range", plc_setting_u16, {
			.u32 = 400 } }, };

static plc_setting_t profile_encoder_wave_3kHz_O500[] = {
	{
		"freq", plc_setting_float, {
			.f = 3000 } }, };

static plc_setting_t profile_encoder_wav_file[] = {
	{
		"filename", plc_setting_string, {
			.s = "beep.wav" } }, };

static const struct profile predefined_profiles[] = {
	{
		.identifier = "loop_2kHz", .title = "LOOP 2kHz", .app_settings = {
			.count = ARRAY_SIZE(profile_loopback), .settings = profile_loopback },
		.encoder_settings = {
			.plugin = "encoder-wave", .count = ARRAY_SIZE(profile_encoder_wave_2kHz_O800),
			.settings = profile_encoder_wave_2kHz_O800, } }, {
		.identifier = "tx_2kHz", .title = "TX 2kHz", .app_settings = {
			.count = ARRAY_SIZE(profile_txpga), .settings = profile_txpga }, .encoder_settings = {
			.plugin = "encoder-wave", .count = ARRAY_SIZE(profile_encoder_wave_2kHz_O500),
			.settings = profile_encoder_wave_2kHz_O500, } }, {
		.identifier = "tx_3kHz", .title = "TX 3kHz", .inherit = "tx_2kHz", .encoder_settings = {
			.count = ARRAY_SIZE(profile_encoder_wave_3kHz_O500), .settings =
					profile_encoder_wave_3kHz_O500, } }, {
		.identifier = "play_beep", .title = "Play Beep", .app_settings = {
			.count = ARRAY_SIZE(profile_txpga), .settings = profile_txpga }, .encoder_settings = {
			.plugin = "encoder-wav", .count = ARRAY_SIZE(profile_encoder_wav_file), .settings =
					profile_encoder_wav_file, } }, };

static void profiles_add_profile_struct_settings(struct setting_vector *dst,
		const struct setting_vector *src)
{
	uint32_t n;
	if (src->plugin)
	{
		assert(dst->plugin == NULL);
		dst->plugin = strdup(src->plugin);
	}
	const struct plc_setting *setting = src->settings;
	for (n = src->count; n > 0; n--, setting++)
		profiles_add_setting(dst, setting);
}

static void profiles_add_profile_struct(struct profiles *profiles, const struct profile *profile)
{
	profiles_add_profile(profiles, profile->identifier, profile->inherit, profile->title,
			profile->hidden);
	profiles_add_profile_struct_settings(&profiles->profile_list_cur->profile.app_settings,
			&profile->app_settings);
	profiles_add_profile_struct_settings(&profiles->profile_list_cur->profile.encoder_settings,
			&profile->encoder_settings);
	profiles_add_profile_struct_settings(&profiles->profile_list_cur->profile.decoder_settings,
			&profile->decoder_settings);
}

static void profiles_load_defaults(struct profiles *profiles)
{
	uint32_t n;
	for (n = 0; n < ARRAY_SIZE(predefined_profiles); n++)
		profiles_add_profile_struct(profiles, &predefined_profiles[n]);
}

static void profiles_load_xml(struct profiles *profiles)
{
	// Initialize the library and check that the loaded library is compatible with the version it
	// was compiled
	LIBXML_TEST_VERSION
	if (profiles_xml_parse_file(profiles, "profiles.xml") < 0)
		profiles_load_defaults(profiles);
	xmlCleanupParser();
	// Debug memory for regression tests
	xmlMemoryDump();
}

struct profiles *profiles_create(void)
{
	struct profiles *profiles = calloc(1, sizeof(struct profiles));
	profiles_load_xml(profiles);
	return profiles;
}

void profiles_release_tree_node(struct tree_node_vector *node)
{
	uint32_t n;
	if (node->title)
		free(node->title);
	if (node->profile_identifiers.identifiers)
		free(node->profile_identifiers.identifiers);
	for (n = 0; n < node->nodes_count; n++)
		profiles_release_tree_node(&node->nodes[n]);
}

void profiles_release(struct profiles *profiles)
{
	uint32_t n;
	struct profile_list_item *item = profiles->profile_list_head;
	for (n = profiles->profile_list_count; item != NULL; n--)
	{
		if (item->profile.inherit)
			free(item->profile.inherit);
		free(item->profile.title);
		free(item->profile.identifier);
		uint32_t i;
		struct plc_setting *setting = item->profile.encoder_settings.settings;
		for (i = item->profile.encoder_settings.count; i > 0; i--, setting++)
			plc_setting_clear(setting);
		free(item->profile.encoder_settings.settings);
		if (item->profile.encoder_settings.plugin)
			free(item->profile.encoder_settings.plugin);
		struct profile_list_item *item_prev = item;
		item = item->next;
		free(item_prev);
	}
	assert(n == 0);
	profiles_release_tree_node(profiles->profile_tree);
	free(profiles);
}

typedef struct profile_list_item profiles_iterator;
uint32_t profiles_get_count(struct profiles *profiles)
{
	return profiles->profile_list_count;
}

profiles_iterator *profiles_move_to_first_profile(struct profiles *profiles)
{
	return profiles->profile_list_head;
}

const char *profiles_current_profile_get_identifier(profiles_iterator *iterator)
{
	return iterator->profile.identifier;
}

const char *profiles_current_profile_get_title(profiles_iterator *iterator)
{
	return iterator->profile.title;
}

int profiles_current_profile_is_hidden(profiles_iterator *iterator)
{
	return iterator->profile.hidden;
}

profiles_iterator *profiles_move_to_next_profile(profiles_iterator *iterator)
{
	return iterator->next;
}

struct push_profile_params
{
	struct settings *settings;
	struct setting_list_item **encoder_setting_list;
	struct setting_list_item **decoder_setting_list;
	char **encoder_plugin_name;
	char **decoder_plugin_name;
};

static void profiles_push_profile(struct profiles *profiles,
		struct push_profile_params *push_profile_params, const char *profile_identifier)
{
	struct profile_list_item *item;
	for (item = profiles->profile_list_head; item != NULL; item = item->next)
	{
		if (strcmp(profile_identifier, item->profile.identifier) == 0)
		{
			// TODO: Check for circular references (with a depth counter for example)
			if (item->profile.inherit != NULL)
				profiles_push_profile(profiles, push_profile_params, item->profile.inherit);
			settings_set_app_settings(push_profile_params->settings,
					item->profile.app_settings.count, item->profile.app_settings.settings);
			plc_setting_set_settings(item->profile.encoder_settings.count,
					item->profile.encoder_settings.settings,
					push_profile_params->encoder_setting_list);
			plc_setting_set_settings(item->profile.decoder_settings.count,
					item->profile.decoder_settings.settings,
					push_profile_params->decoder_setting_list);
			if (item->profile.encoder_settings.plugin)
			{
				if (*push_profile_params->encoder_plugin_name == NULL)
					*push_profile_params->encoder_plugin_name = strdup(
							item->profile.encoder_settings.plugin);
				else if (strcmp(*push_profile_params->encoder_plugin_name,
						*push_profile_params->encoder_plugin_name) != 0)
					log_format("Invalidad mix of profiles using a different encoder: "
							"'%s'ignored (expected '%s')\n",
							push_profile_params->encoder_plugin_name,
							*push_profile_params->encoder_plugin_name);
			}
			if (item->profile.decoder_settings.plugin)
			{
				if (*push_profile_params->decoder_plugin_name == NULL)
					*push_profile_params->decoder_plugin_name = strdup(
							item->profile.decoder_settings.plugin);
				else if (strcmp(*push_profile_params->decoder_plugin_name,
						*push_profile_params->decoder_plugin_name) != 0)
					log_format("Invalidad mix of profiles using a different decoder: "
							"'%s'ignored (expected '%s')\n",
							push_profile_params->decoder_plugin_name,
							*push_profile_params->decoder_plugin_name);
			}
			return;
		}
	}
	log_format("Requested profile '%s' doesn't have a definition in the XML -> ignored\n",
			profile_identifier);
}

void profiles_push_profile_settings(struct profiles *profiles, const char *profile_identifier,
		struct settings *settings, char **encoder_plugin_name,
		struct setting_list_item **encoder_setting_list, char **decoder_plugin_name,
		struct setting_list_item **decoder_setting_list)
{
	struct push_profile_params push_profile_params;
	*encoder_plugin_name = NULL;
	*decoder_plugin_name = NULL;
	push_profile_params.settings = settings;
	push_profile_params.encoder_plugin_name = encoder_plugin_name;
	push_profile_params.decoder_plugin_name = decoder_plugin_name;
	push_profile_params.encoder_setting_list = encoder_setting_list;
	push_profile_params.decoder_setting_list = decoder_setting_list;
	profiles_push_profile(profiles, &push_profile_params, profile_identifier);
}

const struct tree_node_vector *profiles_get_tree(struct profiles *profiles)
{
	return profiles->profile_tree;
}

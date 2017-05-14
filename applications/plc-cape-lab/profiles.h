/**
 * @file
 * @brief	Application profiles management
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2017 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef PROFILES_H
#define PROFILES_H

struct profiles;
struct settings;
struct setting_list_item;
struct plc_plugin_list;
struct plc_setting_named_list;

struct profiles *profiles_create(void);
void profiles_release(struct profiles *profiles);

struct profile_list_item;
typedef struct profile_list_item profiles_iterator;

uint32_t profiles_get_count(struct profiles *profiles);
profiles_iterator *profiles_move_to_first_profile(struct profiles *profiles);
const char *profiles_current_profile_get_identifier(profiles_iterator *iterator);
const char *profiles_current_profile_get_title(profiles_iterator *iterator);
int profiles_current_profile_is_hidden(profiles_iterator *iterator);
profiles_iterator *profiles_move_to_next_profile(profiles_iterator *iterator);
void profiles_push_profile_settings(struct profiles *profiles, const char *profile_identifier,
		struct settings *settings, char **encoder_plugin_name,
		struct setting_list_item **encoder_setting_list, char **decoder_plugin_name,
		struct setting_list_item **decoder_setting_list);

struct profile_identifier_vector
{
	uint32_t capacity;
	uint32_t count;
	char **identifiers;
};

struct tree_node_vector
{
	char *title;
	struct profile_identifier_vector profile_identifiers;
	uint32_t nodes_capacity;
	uint32_t nodes_count;
	struct tree_node_vector *nodes;
};

const struct tree_node_vector *profiles_get_tree(struct profiles *profiles);

#endif /* PROFILES_H */

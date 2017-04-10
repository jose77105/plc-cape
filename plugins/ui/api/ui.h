/**
 * @file
 * @brief	Public API of _ui_ plugins
 *
 * @see		@ref plugins-ui
 * 
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef PLUGINS_UI_UI_H
#define PLUGINS_UI_UI_H

#include "+common/api/ui.h"

/**
 * @brief PLUGIN API duality. See @ref plugins for more info
 */
#ifndef PLUGINS_API_HANDLE_EXPLICIT_DEF
typedef void *ui_api_h;
#endif

enum ui_info_enum
{
	ui_info_none = 0,
	ui_info_settings_text,		// const char*
	ui_info_status_bar_text,	// const char*
};

/**
 * @brief	UI plugin interface
 * @details
 *			Interace that any _UI_-type plugin must obey
 * @warning
 *			* The _struct ui_api_ can only have functions (pure **interface** concept)\n
 *			* To guarantee backwards compatibility add always new functions to the end of the struct
 *			and never modify the behavior of the existing ones
 */
struct ui_api {
	int (*init)(int argc, char *argv[]);
	/**
	 * @brief	Constructor of a plugin-specific ui object
	 * @param	title					Title of the main menu
	 * @param	main_menu_items			List of main menu items and associated operations
	 * @param	main_menu_items_count	Number of main menu items
	 * @param	callbacks_handle		Handle sent to the menu operations for custom usage
	 * @return	A handle to the created object
	 */
	ui_api_h (*create)(const char *title, const struct ui_menu_item *main_menu_items,
		uint32_t main_menu_items_count, ui_callbacks_h callbacks_handle);
	/**
	 * @brief	Destructor of a previously created plugin-specific ui object
	 * @param	handle	Handle to the ui-plugin
	 */
	void (*release)(ui_api_h handle);
	/**
	 * @brief	Starts the menu loop in charge of capturing the key pressed and executing the
	 *			corresponding menu operation. It's a blocking function until 'quit' is called from
	 *			a different thread
	 * @param	handle	Handle to the ui-plugin
	 */
	void (*do_menu_loop)(ui_api_h handle);
	/**
	 * @brief	Forces a refreshment of the whole user interface
	 * @param	handle	Handle to the ui-plugin
	 */
	void (*refresh)(ui_api_h handle);
	/**
	 * @brief	Specifies a text to be logged in the dedicated window
	 * @param	handle	Handle to the ui-plugin
	 * @param	text	The text to be displayed
	 */
	void (*log_text)(ui_api_h handle, const char *text);
	/**
	 * @brief	Notifies the user interface that one of the predefined events (asynchronous
	 *			notifications) has occurred in order the user interface to be updated accordingly
	 * @param	handle	Handle to the ui-plugin
	 * @param	id		Identifier of the event
	 * @param	data	Extra data associated to the event
	 */
	void (*set_event)(ui_api_h handle, uint32_t id, uint32_t data);
	/**
	 * @brief	Closes the active panel (popup menu, dialog, etc.) and then displays the parent 
	 *			one. Used for example when typing ESC on a menu to go back
	 * @param	handle	Handle to the ui-plugin
	 */
	void (*active_panel_close)(ui_api_h handle);
	/**
	 * @brief	Closes the menu
	 * @param	handle	Handle to the ui-plugin
	 */
	void (*quit)(ui_api_h handle);
	/**
	 * @brief	Opens a sub-menu
	 * @param	handle				Handle to the ui-plugin
	 * @param	menu_items			List of menu items and associated operations
	 * @param	menu_items_count	Number of menu items
	 * @param	title				Title of the menu window
	 * @param	on_cancel			Function to be called by the plugin if the user cancels the
	 *								menu (by pressing ESC or similar)
	 * @return	A handle to the created object
	 */
	void (*open_menu)(ui_api_h handle, const struct ui_menu_item *menu_items,
		uint32_t menu_items_count, const char *title, void (*on_cancel)(ui_callbacks_h));
	/**
	 * @brief	Opens a dialog box with a list of user-editable fields
	 * @param	handle			Handle to the ui-plugin
	 * @param	dialog_items	List of dialog fields
	 * @param	dialog_items_count	Number of dialog fields
	 * @param	title			Title of the dialog window
	 * @param	on_cancel		Function to be called by the plugin if the user cancels the dialog
	 *							(by pressing ESC or similar)
	 * @param	on_ok			Function to be called by the plugin if the user validates the dialog
	 *							(by pressing RETURN or similar)
	 */
	void (*open_dialog)(ui_api_h handle, const struct ui_dialog_item *dialog_items,
		uint32_t dialog_items_count, const char *title, void (*on_cancel)(ui_callbacks_h),
		void (*on_ok)(ui_callbacks_h));
	/**
	 * @brief	Sets a predefined-type information in order the user interface decides if rendering
	 *			it and how
	 * @param	handle		Handle to the ui-plugin
	 * @param	info_type	The category of the information to show
	 * @param	info_data	Data associated to the 'info_type'
	 */
	void (*set_info)(ui_api_h handle, enum ui_info_enum info_type, const void *info_data);
};

#endif /* PLUGINS_UI_UI_H */

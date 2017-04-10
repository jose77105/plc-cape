/**
 * @file
 * @brief	Global declarations and inclusions for any project within the _plc-cape_ framework
 * @details
 *	Some subjects covered in this common file are:\n
 *	## Global constants
 *		- As @ref PLC_MAX_PATH for the max lenght of the paths considered within the project
 *
 *	## Macros for libraries and plugins
 *		- As #ATTR_EXTERN to declare the visibility of the API functions
 *
 *	## Tracing helper macros
 *		- As #plc_debug_level, #TRACE, #TRACE1
 *
 *	## _typedefs_ used on DAC and ADC
 *		- As _sample_rx_t_, _sample_tx_t_ for the definition of the type used on TX (DAC) and RX
 *			(ADC)
 *
 *	## Singletons
 *		- Definition of the available singletons in #singleton_id_enum and the declaration of the
 *			function offered by the singleton-providers (#singletons_provider_get_t)
 *
 *	@anchor common_base_stringification
 *	## Macro Stringification
 *		- https://gcc.gnu.org/onlinedocs/cpp/Stringification.html#Stringification
 *		- http://stackoverflow.com/questions/3419332/c-preprocessor-stringify-the-result-of-a-macro
 *
 * @cond COPYRIGHT_NOTES @copyright
 *	Copyright (C) 2016 Jose Maria Ortega\n
 *	Distributed under the GNU GPLv3. For full terms see the file LICENSE
 * @endcond
 */

#ifndef COMMON_BASE_H
#define COMMON_BASE_H

//
// GLOBAL INCLUDES
//

// Typical common includes to simplify maintenance
#include <assert.h>		// assert
#include <stddef.h>		// offsetof
#include <stdint.h>		// uint32_t
#include <stdio.h>		// sprintf
#include <stdlib.h>		// calloc
#include <string.h>		// memset

//
// GLOBAL CONSTANT
//

/**
 * @brief	A convenient value to simplify the management of paths
 * @details
 *	If longer paths are expected consider a different management
 *	The PLC prefix has been added to don't confuse with _POSIX_PATH_MAX or the MAX_PATH in Windows
 */
#define PLC_MAX_PATH 260

//
// GLOBAL GENERIC-HELPER-MACROS
//

/// @brief	Macro to get the items of an array
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
/**
 * @brief	Declares a symbol **externally** public
 * @details	Declares a symbol **externally** public, even if the option '-fwhole-program' is set
 */
#define ATTR_EXTERN __attribute__((externally_visible))
/** 
 * @brief	Declares a symbol **internally** public
 * @details Declares a symbol **internally** public, even if the option '-fwhole-program' is set
 * @note	No way has been found up to date to declare a method public at project level but
 *		externally private (like the _internal_ declaration available in C#)
 */
#define ATTR_INTERN __attribute__((externally_visible))
/**
 * @brief	Gets the pointer to the parent object given the pointer to a member
 * @details
 *	Definition of 'container_of' copied from kernel source code:
 *		http://lxr.free-electrons.com/source/tools/virtio/linux/kernel.h
 */
#define container_of(ptr, type, member) ({ \
	const typeof( ((type *)0)->member ) *__mptr = (ptr); \
	(type *)( (char *)__mptr - offsetof(type,member) );})
/**
 * @brief	Macro to double-quote an item at compiling-time
 * @sa		@ref common_base_stringification "Stringification"
 */
#define QUOTE(x) #x
/**
 * @brief	Macro required to double-quote an item at preprocessor-time
 * @sa		@ref common_base_stringification "Stringification"
 */
#define EXPAND_AND_QUOTE(x) QUOTE(x)
/**
 * @brief	Macro to check a condition at compile-time when after the preprocessor phase
 * @pre		Must be used within a function. It doesn't work declared at global level
 * @details
 *	Typical usage: to test that a 'struct' has the expected size\n
 *	@code
 *	// Check that the 'my_stuct_with_12_bytes' has exactly 12 bytes
 *	REPORT_COMPILER_ERROR_IF(sizeof(my_stuct_with_12_bytes) != 12);
 *	@endcode
 * @sa
 *	http://stackoverflow.com/questions/4079243/how-can-i-use-sizeof-in-a-preprocessor-macro\n
 *	https://scaryreasoner.wordpress.com/2009/02/28/checking-sizeof-at-compile-time/
 */
#define REPORT_COMPILER_ERROR_IF(condition) ((void)sizeof(char[1 - 2*!!(condition)]))
/**
 * @brief	Check that the interface size is the expected one
 * @pre		Must be used within a function. It doesn't work declared at global level
 * @details
 *	Check that the interface has not changed from last build and has the expected members.
 *	Otherwise report a compiler error to request for the required adaptation
 *	Typical usage:
 *	@code
 *	// Check that the 'struct ui_api' has exactly 15 members
 *	CHECK_INTERFACE_MEMBERS_COUNT(ui_api, 15);
 *	@endcode
 * @sa	#REPORT_COMPILER_ERROR_IF
 */
#define CHECK_INTERFACE_MEMBERS_COUNT(api_struct_name, members) \
	REPORT_COMPILER_ERROR_IF(sizeof(struct api_struct_name) != (members) * 4)

//
// DEBUGGING AND TRACING SUPPORT
//

/**
 * @brief	Debugging level tuning
 * @details
 *	For comfortable adjustment and maximum versatility the `plc_debug_level` has been defined
 *	as an external variable. If a component want to use the TRACE macros the `plc_debug_level` must
 *	be declared somewhere as variable or constant.
 *	This system has the following benefits:
 *	* it allows changing the debugging level at building-time with minor rebuilding
 *	* it also offers the possibility of changind the debuggin level on-the-fly at run-time
 *	* tracing can be isolately boosted or reduced for a specific file or even for a specific block.
 *		This can be useful to find a bug on a specific area.
 *		To do that override the variable behavior of `plc_debug_level` by a `define` macro:
 *			@code{.c}
 *			// ... Some code with 'plc_debug_level' basic behavior
 *			// For the following block boost 'plc_debug_level' to maximum
 *			#define plc_debug_level 3
 *			// ... Some code with TRACEs boosted to level 3 ...
 *			#undef plc_debug_level
 *			// ... Revert back to default 'plc_debug_level'
 *			@endcode
 */
extern int plc_debug_level;

#ifndef DEBUG
/**
 * @brief	Macro for tracing
 * @param	level Verbosity level:
 *			* 1 = minimum trancing, reserved for critical messages (as unexepected errors).
 *				They will appear in any debugging session even if tracing set to minimum
 *			* 2 = normal tracing, for helpful messages in default debugging sessions.
 *				They will appear in usual debugging sessions
 *			* 3 = maximum tracing, for intensive debugging messages, only shown with extra-verbosity
 *				level. They will appear in bug-finding debugging sessions
 * @param	text Message to be logged
 * @details
 *	The macro is only expanded in the binaries if **DEBUG** is defined\n
 *	The macro relies on two symbols that must be present:
 *	* a function `plc_trace(const char *function_name, const char *format, ...)` in charge of the
 *		logging
 *	* a variable `plc_debug_level` in charge of the verbosity level
 */
#define TRACE(level, text)
/// @brief	Like #TRACE but allowing a variable parameter (in _printf_-format)
#define TRACE1(level, format, arg1)
/// @brief	Like #TRACE but allowing two variable parameters (in _printf_-format)
#define TRACE2(level, format, arg1, arg2)
#else
// NOTE: do..while(0) used for TRACE usage satety. More information on:
//	http://stackoverflow.com/questions/154136/...
//		...why-use-apparently-meaningless-do-while-and-if-else-statements-in-c-c-macros
// An official usage example is also found in the man pages for _timers_:
//	http://man7.org/linux/man-pages/man2/timer_create.2.html
#define TRACE(level, text) do { if (level <= plc_debug_level) plc_trace(__func__, text); } while(0)
#define TRACE1(level, format, arg1) \
		do { if (level <= plc_debug_level) plc_trace(__func__, format, arg1); } while(0)
#define TRACE2(level, format, arg1, arg2) \
		do { if (level <= plc_debug_level) plc_trace(__func__, format, arg1, arg2); } while(0)
#endif

//
// PLC DRIVERS
//

// PLC_SIGNUM_APP_WARNING is the signal used by our customized 'edma_plc' driver
#define PLC_SIGNUM_APP_WARNING 44

//
// TX AND RX GENERIC TYPES
//

// Macros encapsulating some of the global types used to simplify portability to other platforms
typedef uint16_t sample_rx_t;
typedef uint16_t sample_tx_t;
typedef uint8_t data_tx_rx_t;
#define sample_rx_csv_enum csv_u16

//
// SINGLETONS
//

/**
 * @brief	Type of identified singletons
 * @warning	For compatibility new interfaces must be **always** added to the end
 */
enum singleton_id_enum
{
	/// Interface for error management: @ref plc_error_api
	singleton_id_error = 1,
	///	Interface for logging functionality: @ref plc_logger_api
	singleton_id_logger,
};
/**
 * @brief	Encapsulation of the handler used to identify a singletons-provider object
 */
typedef void *singletons_provider_h;
/**
 * @brief	Definition of the calling format of the function that singleton providers must implement
 */
typedef int (*singletons_provider_get_t)(singletons_provider_h handle,
		enum singleton_id_enum interface_id, void **interface_vtbl, void **interface_handle,
		uint32_t *interface_version);

//
// PLUGIN MANAGEMENT
//

/**
 * @brief	Macro encapsulating the name of one of public functions exported by the plugins
 * @note	'define' statements are used on public API function names to simplify maintenance
 *			(global renaming, location of public entry points by global search, etc.)
 */
#define PLUGIN_API_SET_SINGLETON_PROVIDER set_singletons_provider
#define PLUGIN_API_SET_SINGLETON_PROVIDER_STRING \
	EXPAND_AND_QUOTE(PLUGIN_API_SET_SINGLETON_PROVIDER)
typedef void (*plugin_set_singletons_provider_t)(singletons_provider_get_t, singletons_provider_h);

#define PLUGIN_API_LOAD plugin_api_load
#define PLUGIN_API_LOAD_STRING EXPAND_AND_QUOTE(PLUGIN_API_LOAD)
typedef void *(*plugin_api_load_t)(uint32_t *plugin_api_version, uint32_t *plugin_api_size);

#define PLUGIN_API_UNLOAD plugin_api_unload
#define PLUGIN_API_UNLOAD_STRING EXPAND_AND_QUOTE(PLUGIN_API_UNLOAD)
typedef void (*plugin_api_unload_t)(void *plugin_api);

#endif /* COMMON_BASE_H */

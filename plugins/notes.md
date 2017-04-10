PLUGINS {#plugins}
=======

@brief Dynamic library objects loaded at run-time

## DETAILS

Plugins are dynamic library objects (*.so) that can be loaded at runtime on demand

Plugins are classified in three categories according to the interface they obey:

<table>
<tr bgcolor="Lavender">
	<td><b>Category</b><td><b>Public API</b><td><b>Source code</b><td><b>Brief description</b>
<tr>
	<td><b>@subpage plugins-encoder</b>
	<td>@ref encoder_api
	<td>@link ./plugins/encoder @endlink
	<td>@copybrief plugins-encoder
<tr>
	<td><b>@subpage plugins-decoder</b>
	<td>@ref decoder_api
	<td>@link ./plugins/decoder @endlink
	<td>@copybrief plugins-decoder
<tr>
	<td><b>@subpage plugins-ui</b>
	<td>@ref ui_api
	<td>@link ./plugins/ui @endlink
	<td>@copybrief plugins-ui
</table>

You can create a new plugin by just copying an existing folder and modifying it according to your
needs

## CREATING A PLUGIN

For convenience, when creating a new plugin declare the symbol PLUGINS_API_HANDLE_EXPLICIT_DEF and
redefine the plugin-based-handle to the struct you are going to use in your implementation.
This simplifies the coding and is more robust because avoids the typical unsafe _void *_
hard-castings

Example from @link ./plugins/encoder/encoder-pwm/encoder-pwm.c @endlink:

	...
	#include "+common/api/logger.h"
	#define PLUGINS_API_HANDLE_EXPLICIT_DEF
	typedef struct encoder *encoder_api_h;
	#include "plugins/encoder/api/encoder.h"
	...
	struct encoder
	{
		float freq_dac_sps;
		float samples_per_bit;
		...
	};
	...
	struct encoder *encoder_create(void)
	{
		...
	}

@dir plugins
@see @ref plugins
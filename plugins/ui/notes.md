User Interfaces {#plugins-ui}
===============

@brief Implement a specific user interface mechanism

## SUMMARY

<table>
<tr bgcolor="Lavender">
	<td><b>Plugin</b><td><b>Source code</b><td><b>Brief description</b>
<tr>
	<td><b>@subpage plugin-ui-ncurses</b>
	<td>@link ./plugins/ui/ui-ncurses @endlink
	<td>@copybrief plugin-ui-ncurses
<tr>
	<td><b>@subpage plugin-ui-console</b>
	<td>@link ./plugins/ui/ui-console @endlink
	<td>@copybrief plugin-ui-console
</table>

## DETAILS

_UI_ plugins are dynamic libraries that provide user interface interactions through a predefined set
of functionality

Encapsulating the UI in a plugin allows the client applications to adapt to different scenarios:
* **console-based**: when minimum interface is required or when third party packages required by
	other UIs are not available in the target platform
* **ncurses-based**: for a richer interface based on the _ncurses_ package
* **graphical**: if we have a graphiical display we can explore even more possibilities in the UI,
	as display captured graphs in real-time
* ...

## PUBLIC API

<table border=0>
<tr><td><b>API help</b>: @ref ui_api
<tr><td><b>API header</b><br><pre>@include plugins/ui/api/ui.h</pre>
</table>

@dir plugins/ui
@see @ref plugins-ui
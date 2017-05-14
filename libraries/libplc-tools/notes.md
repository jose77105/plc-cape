libplc-tools {#library-libplc-tools}
============

@brief Generic functionality to be reused by any _plc-cape_-based component

## SUMMARY

<table>
<tr>
	<td><b>Target</b><td><i>libplc-tools.a</i>
<tr>
	<td><b>Purpose</b><td>
	This library offers generic functionality to be used by any project belonging to the
	<i>plc-cape</i> framework
<tr>
	<td><b>Details</b><td>
	This library relies only on functionality provided by the Linux Operating system. Therefore,
	it could be used in other Linux-based platforms besides the BeagleBone Black platform\n
	This version of the library covers these areas:
	<ul>
		<li><b>application</b>: @copybrief libplc-tools/api/application.h
		<li><b>cmdline</b>: @copybrief libplc-tools/api/cmdline.h
		<li><b>file</b>: @copybrief libplc-tools/api/file.h
		<li><b>plugin</b>: @copybrief libplc-tools/api/plugin.h
		<li><b>settings</b>: @copybrief libplc-tools/api/settings.h
		<li><b>signal</b>: @copybrief libplc-tools/api/signal.h
		<li><b>terminal_io</b>: @copybrief libplc-tools/api/terminal_io.h
		<li><b>time</b>: @copybrief libplc-tools/api/time.h
		<li><b>trace</b>: @copybrief libplc-tools/api/trace.h
	</ul>
<tr>
	<td><b>Dependencies</b><td>
	<b>librt</b>: time.h
	<b>libm</b>: signal.h
<tr>
	<td><b>API help</b>
	<td>@link ./libraries/libplc-tools/api @endlink
<tr>
	<td><b>Source code</b>
	<td>@link ./libraries/libplc-tools @endlink
</table>

@dir libraries/libplc-tools
@see @ref library-libplc-tools

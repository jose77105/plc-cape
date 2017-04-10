libplc-gpio {#library-libplc-gpio}
============

@brief GPIO helpful extra functionality

## SUMMARY

<table>
<tr>
	<td><b>Target</b><td><i>libplc-gpio.a</i>
<tr>
	<td><b>Purpose</b><td>
 	It encapsulates some useful GPIO extended functionality like callbacks-on-toggling.
<tr>
	<td><b>Details</b><td>
	This library doesn't rely on the PlcCape board but only on the BBB<br>
	It implements two sets of functions to access the GPIO:
	* using the standard file system '/sys/class/gpio/...'
		* **PROS**: encapsulates the access to 'gpio' through a driver. This allows a better use of
			shared resources. For example if a GPIO pin is already used, a cape could
			automatically (or manually through jumper) use a different one
	* using direct access to memory
		* **PROS**: optimum performance because not intermediate wrappers
		* **CONS**: requires 'root' access
<tr>
	<td><b>Dependencies</b><td>
	<b>pthread</b>: some gpio functions spawn new threads<br>
	<b>libplc-tools</b>: used to get some information from device-based files
<tr>
	<td><b>API help</b>
	<td>@link ./libraries/libplc-gpio/api @endlink
<tr>
	<td><b>Source code</b>
	<td>@link ./libraries/libplc-gpio @endlink
</table>

@dir libraries/libplc-gpio
@see @ref library-libplc-gpio

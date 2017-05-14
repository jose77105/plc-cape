Decoders {#plugins-decoder}
========

@brief Generate output data from RAW samples coming from an ADC

## SUMMARY

<table>
<tr bgcolor="Lavender">
	<td><b>Name</b><td><b>Source code</b><td><b>Brief description</b>
<tr>
	<td><b>@subpage plugin-decoder-raw</b>
	<td>@link ./plugins/decoder/decoder-raw @endlink
	<td>@copybrief plugin-decoder-raw
<tr>
	<td><b>@subpage plugin-decoder-pwm</b>
	<td>@link ./plugins/decoder/decoder-pwm @endlink
	<td>@copybrief plugin-decoder-pwm
<tr>
	<td><b>@subpage plugin-decoder-ook</b>
	<td>@link ./plugins/decoder/decoder-ook @endlink
	<td>@copybrief plugin-decoder-ook
<tr>
	<td><b>@subpage plugin-decoder-morse</b>
	<td>@link ./plugins/decoder/decoder-morse @endlink
	<td>@copybrief plugin-decoder-morse
</table>

## DETAILS

_Decoder_ plugins are dynamic libraries that convert an incoming signal of raw samples from an ADC
to the corresponding data

## PUBLIC API

<table border=0>
<tr><td><b>API help</b>: @ref decoder_api
<tr><td><b>API header</b><br><pre>@include plugins/decoder/api/decoder.h</pre>
</table>

@dir plugins/decoder
@see @ref plugins-decoder

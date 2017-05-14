Encoders {#plugins-encoder}
========

@brief Generate RAW samples to feed a DAC from input data or given settings

## SUMMARY

<table>
<tr bgcolor="Lavender">
	<td><b>Name</b><td><b>Source code</b><td><b>Brief description</b>
<tr>
	<td><b>@subpage plugin-encoder-wave</b>
	<td>@link ./plugins/encoder/encoder-wave @endlink
	<td>@copybrief plugin-encoder-wave
<tr>
	<td><b>@subpage plugin-encoder-pwm</b>
	<td>@link ./plugins/encoder/encoder-pwm @endlink
	<td>@copybrief plugin-encoder-pwm
<tr>
	<td><b>@subpage plugin-encoder-ook</b>
	<td>@link ./plugins/encoder/encoder-ook @endlink
	<td>@copybrief plugin-encoder-ook
<tr>
	<td><b>@subpage plugin-encoder-morse</b>
	<td>@link ./plugins/encoder/encoder-morse @endlink
	<td>@copybrief plugin-encoder-morse
<tr>
	<td><b>@subpage plugin-encoder-wav</b>
	<td>@link ./plugins/encoder/encoder-wav @endlink
	<td>@copybrief plugin-encoder-wav
</table>

## DETAILS

_Encoder_ plugins are dynamic libraries that receive plugin-specific settings as the input and
generates output samples to be sent by the DAC

## PUBLIC API

<table border=0>
<tr><td><b>API help</b>: @ref encoder_api
<tr><td><b>API header</b><br><pre>@include plugins/encoder/api/encoder.h</pre>
</table>

@dir plugins/encoder
@see @ref plugins-encoder

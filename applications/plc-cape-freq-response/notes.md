plc-cape-freq-response {#application-plc-cape-freq-response}
======================

@brief Frequency response analyzer

## SUMMARY

<table>
<tr>
	<td><b>Target</b><td><i>plc-cape-freq-response</i>
<tr>
	<td><b>Purpose</b><td>
	Analyze the frequency response of a DAC to ADC path
<tr>
	<td><b>Details</b><td>
	This application generates sine waves sweeping over a predefined range of
	frequencies and analyzing the captured signal to get the attenuation occurred per
	frequency. Then, it stores in a file the data representing the frequency response of
	the DAC to ADC loop
	
	Note: this version only analyzes the effects on magnitude (not phase)
<tr>
	<td><b>Source code</b>
	<td>@link ./applications/plc-cape-freq-response @endlink
</table>

@dir applications/plc-cape-freq-response
@see @ref application-plc-cape-freq-response

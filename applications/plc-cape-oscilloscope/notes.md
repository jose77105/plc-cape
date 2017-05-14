plc-cape-oscilloscope {#application-plc-cape-oscilloscope}
=====================

@brief Mini oscilloscope tool to monitor the data captured by the ADC in real time

## SUMMARY

<table>
<tr>
	<td><b>Target</b><td><i>plc-cape-oscilloscope</i>
<tr>
	<td><b>Purpose</b><td>
	Build a tool that works as a cheap oscilloscope on the BBB to easily and quickly check the data
	captured by the ADC
<tr>
	<td><b>Details</b><td>
	It simplifies the task of analyzing the received data without requiring an external
	oscilloscope. Moreover, this tools exposes what the ADC of the BBB really gets, which can be
	different from the corresponding analog signal.
	
	For example: when measuring a 110 kHz sinusoid with external equipment, we measure a real
	frequency of 110 kHz but when measuring it with the 'plc-cape-oscilloscope' tool we get a signal
	of 90 kHz, which is the aliased version due to the max sampling rate of the ADC (= 200 ksps).
<tr>
	<td><b>Source code</b>
	<td>@link ./applications/plc-cape-oscilloscope @endlink
</table>

@dir applications/plc-cape-oscilloscope
@see @ref application-plc-cape-oscilloscope
plc-cape-lab {#application-plc-cape-lab}
============

@brief Laboratory tool to work with the PlcCape board

## SUMMARY

<table>
<tr>
	<td><b>Target</b><td><i>plc-cape-lab</i>
<tr>
	<td><b>Purpose</b><td>
	Laboratory tool to explore the main features offered by the <i>PlcCape</i> board
<tr>
	<td><b>Details</b><td>
	It is the main application for "laboratory work". It offers access to the different configurable
	options of the AFE031.<br>
	Features:
	<ul>
		<li>Multi wave generation
		<li>Real-time and deferred DAC transmission
		<li>Real-time and deferred ADC reception
		<li>Capturing to file in Octave-compatible format for post-analysis
		<li>Configure main AFE031 parameters: CENELEC band, gains, calibration modes, etc
		<li>Time measurements
	</ul>
<tr>
	<td><b>Source code</b>
	<td>@link ./applications/plc-cape-lab @endlink
</table>

## COMMAND LINE USAGE

	Usage: plc-cape-lab [OPTION]...
	"Laboratory" to experiment with the PlcCape board

	  -A:MODE       Select ADC receiving mode
	  -d            Forces the application to use the standard drivers
	  -D:id=value   Speficy a DECODER 'value' for a setting identified as 'id'
	  -E:id=value   Speficy an ENCODER 'value' for a setting identified as 'id'
	  -F:SPS        SPI sampling rate [sps]
	  -I:INTERVAL   Repetitive test interval [ms]
	  -J:INTERVAL   Max duration for the test [ms]
	  -L:DELAY      Samples delay [us]
	  -N:SIZE       Buffer size [samples]
	  -P:PROFILE    Select a predefined profile
	  -q            Quiet mode
	  -S:MODE       SPI transmitting mode
	  -T:MODE       Operating mode
	  -U:NAME       UI plugin name (without extension)
	  -W:SAMPLES    Received samples to be stored in a file
	  -x            Auto start
	  -Y:TYPE       Stream type
		 --help     display this help and exit

	For the arguments requiring an index from a list of options you can get more
	information specifiying the parameter followed by just a colon

@dir applications/plc-cape-lab
@see @ref application-plc-cape-lab

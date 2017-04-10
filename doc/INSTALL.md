BUILDING PROCESS {#building_process}
================

This project is composed of different applications, plugins, libraries and drivers.

To compile them all just execute _make_ from the root folder (see @ref common for information on
makefiles).

To execute the applications (e.g. _plc-cape-lab_):

  1. Go to the corresponding folder (e.g. _cd applications/plc-cape-lab_)
  2. Execute the application (e.g. _./plc-cape-lab_)

Note however that most applications are based on the customized drivers for SPI and ADC. So, they
should be loaded first. For convenience a _run.sh_ script has been included which does the job.
The _run.sh_ script expects two parameters:

  * The version of the PlcCape, to properly configure the SPI lines
  * The version of the BBB, to properly set the ADC preconfigured major number

Typical execution:

	$ ./run.sh 2 2


DOXYGEN DOCUMENTATION
---------------------

To build Doxygen's documentation follow these steps

Steps:
	1. Open 'doyxgen.conf'
	2. Click on 'Run' > HTML documentation will be created in '$DEV_BIN_DIR/doc'
	
@sa @subpage doxygen-notes
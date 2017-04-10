COMMON FOLDER {#common}
=============

@brief	Common resources (*.h headers with public declarations, main *.mk makefiles, etc.) to be
		used by any application, library or plugin belonging to the _plc-cape_ project

[TOC]
## COMMON HEADERS

*.h headers with common helpful declarations available to any project belonging to the _plc-cape_
project

## COMMON MAKEFILES

### RECURSIVE BUILDING

To make the building process easier a _makefile_ is included per area providing recursive building:
	* @link ./drivers/makefile @endlink
	* @link ./libraries/makefile @endlink
	* @link ./plugins/makefile @endlink
	* @link ./applications/makefile @endlink

All these _makefile_ are based in **make_group.mk**.

With this system you can build all the _libraries_ just going to the _libraries_ folder and
executing _make_ there:

	$ cd $DEV_SRC_DIR/libraries
	$ make

If you want to avoid using _cd_ to change you active working folder you can execute _make_ with the
-C flag:

	$ make -C $DEV_SRC_DIR/libraries


### COMMON MAKEFILE CONTENT

Any _makefile_ that builds a user-space-component include the **make_object.mk**.
There is a _makefile_ in the root of each user-space-component.


### COMMON MAKEFILE CONTENT FOR DRIVERS

Kernel drivers require a special _makefile_ to compile. The common instructions are stored in the
**make_driver.mk** under the _+common_ folder

These are the makefiles currently used on this project:
	* @link ./drivers/spi/makefile @endlink
	* @link ./drivers/adc/makefile @endlink

For reference it is also provided the basic typical _makefile_ which generates the binaries in the
same folder as the source code:
	* @link ./drivers/spi/makefile_basic.mk @endlink


### DOXYGEN AND MAKEFILES

Doxygen reveals some problems when parsing makefiles because they can contain expressions that
are not properly parsed (e.g. '/*' is wrongly understood as the beginning of a comment whereas
it can be just a wildcard)


@file	make_driver.mk
@brief	Common makefile used by any kernel-module (called _driver_ in this project) within the
		_plc-cape_ framework

@file	make_group.mk
@brief	Common makefile for recursive _make_

@file	make_object.mk
@brief	Common makefile used by any user-space component within the _plc-cape_ framework

@dir	+common
@see	@ref common
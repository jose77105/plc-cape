#!/bin/bash

$DEV_SRC_DIR/env/bbb/setup/load_drivers.sh $@
if [ $? != 0 ]
then
	exit $?
fi

# Change folder in the subshell to execute the testing app from the correct BIN folder
cd $DEV_BIN_DIR/applications/plc-cape-lab

./plc-cape-lab -P:tx_2kHz

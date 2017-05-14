#!/bin/bash

# Load required drivers
$DEV_SRC_DIR/env/bbb/setup/load_drivers.sh $@
if [ $? != 0 ] ; then exit $? ; fi

APP_NAME=$(basename $PWD)

# Change folder in the subshell to execute the testing app from the correct BIN folder
cd $DEV_BIN_DIR/applications/$APP_NAME
./$APP_NAME

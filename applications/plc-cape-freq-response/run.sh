#!/bin/bash

# Load required drivers
$DEV_SRC_DIR/env/bbb/setup/load_drivers.sh $@
if [ $? != 0 ] ; then exit $? ; fi

APP_NAME=$(basename $PWD)
# NOTE:
# We could directly use '$DEV_BIN_DIR/applications/$APP_NAME/$APP_NAME' to execute the application
# However this won't change the current folder which is used to store the resulting CSV
cd $DEV_BIN_DIR/applications/$APP_NAME
./$APP_NAME


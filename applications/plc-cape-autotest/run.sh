#!/bin/bash

# Load required drivers
$DEV_SRC_DIR/env/bbb/setup/load_drivers.sh $@
if [ $? != 0 ] ; then exit $? ; fi

APP_NAME=$(basename $PWD)
$DEV_BIN_DIR/applications/$APP_NAME/$APP_NAME

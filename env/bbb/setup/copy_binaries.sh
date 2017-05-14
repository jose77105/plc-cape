#!/bin/bash

echo Copying binaries...

echo Create target directory 'plc-cape-bin'
mkdir ~/plc-cape-bin

echo Copying drivers...
cp -r -t ~/plc-cape-bin $DEV_BIN_DIR/drivers

echo Copying plugins...
cp -r -t ~/plc-cape-bin $DEV_BIN_DIR/plugins

echo Copying applications...
cp -r -t ~/plc-cape-bin $DEV_BIN_DIR/applications

echo Copying 'run.sh'...
cp $DEV_SRC_DIR/applications/plc-cape-lab/run.sh ~/plc-cape-bin/applications/plc-cape-lab

echo Copying 'env' folder...
mkdir -p ~/plc-cape-bin/env/bbb/
cp -r -t ~/plc-cape-bin/env/bbb $DEV_SRC_DIR/env/bbb/setup

echo Binaries copied

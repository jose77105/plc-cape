#!/bin/bash

# First allow GUI remote access executing 'xhost +' on a terminal window in the GUI
# Then in the local remote session export the 'DISPLAY' variable
# NOTE: to propery 'export' the variable in the caller shell, execute this script with 'source'
export DISPLAY:=0

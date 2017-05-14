#!/bin/bash

# NOTE: For invocation use one of:
#	$ . ./set_gui.sh
#	$ source ./set_gui.sh
# Otherwise another shell is spawned and exports lost
# More info: http://unix.stackexchange.com/questions/30189/how-can-i-make-variables-exported-in-a-bash-script-stick-around

export DISPLAY=:0.0
setxkbmap -layout "es"
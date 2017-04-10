#!/bin/bash

# Use 'soft' symbols links. 'Hard' link is not allowed if 'x' is not a linux-based filesystem
echo "Creating '/media/x' and '/media/y' links from '/media/j' and '/media/k'"
sudo ln -s /media/j/dev/src/beaglebone/plc-cape /media/x
sudo ln -s /media/k/dev/bin/beaglebone/plc-cape-x86/ /media/y
echo done

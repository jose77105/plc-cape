#!/bin/bash

# Use 'soft' symbols links. 'Hard' link is not allowed if 'x' is not a linux-based filesystem
echo "Creating '/media/x' link from '/media/j'"
sudo ln -s /media/j/dev/src/beaglebone/plc-cape /media/x
echo done

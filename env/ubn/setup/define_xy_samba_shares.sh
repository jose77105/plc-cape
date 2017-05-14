#!/bin/bash

# SAMBA commands:
#	$ net usershare list 			# list all the SAMBA shared names
#	$ net usershare info			# list all the SAMBA shares with extra info
#	$ net usershare delete <name>	# delete a specific SAMBA share
echo "Creating SAMBA shares 'plc-cape-src' and 'plc-cape-bin'"
# 'guest_ok=y' required for simple sharing without permission control
net usershare add plc-cape-src /media/x "" "" guest_ok=y
net usershare add plc-cape-bin /media/y "" "Everyone:f" guest_ok=y
# Optional shares if we want to support compiling on both platforms at same time
net usershare add plc-cape-bin-x86 /media/k/dev/bin/beaglebone/plc-cape-x86 "" "Everyone:f" guest_ok=y
net usershare add plc-cape-bin-bbb /media/k/dev/bin/beaglebone/plc-cape-bbb "" "Everyone:f" guest_ok=y
echo done

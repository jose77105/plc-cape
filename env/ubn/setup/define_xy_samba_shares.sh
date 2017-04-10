#!/bin/bash

# SAMBA commands:
#	$ net usershare list 			# list all the SAMBA shared names
#	$ net usershare info			# list all the SAMBA shares with extra info
#	$ net usershare delete <name>	# delete a specific SAMBA share
echo "Creating SAMBA shares 'plc-cape-src' and 'plc-cape-bin'"
# 'guest_ok=y' required for simple sharing without permission control
net usershare add plc-cape-src /media/x "" "" guest_ok=y
net usershare add plc-cape-bin /media/y "" "" guest_ok=y
echo done

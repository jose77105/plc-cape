#!/bin/bash

echo Adding NFS share resources to exports...
# Note: 'echo text >> /etc/exports' cannot be used because 'root' access required
#	The 'sudo echo... >> ...' doesn't work
#	Another possibility 'sudo bash -c "somecommand >> somefile"'
#	More info in: http://superuser.com/questions/136646/how-to-append-to-a-file-as-sudo
echo "# [CUSTOM] Share development directories by NFS" | sudo tee -a /etc/exports
echo "/media/x 192.168.7.0/255.255.255.0(ro,sync,no_subtree_check)" | sudo tee -a /etc/exports
echo "/media/y 192.168.7.0/255.255.255.0(rw,sync,no_subtree_check)" | sudo tee -a /etc/exports
echo Refreshing exported data...
sudo exportfs -a
echo done
exit

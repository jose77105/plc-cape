#!/bin/bash

PROJECT_SRC=plc-cape-src
PROJECT_BIN=plc-cape-bin

# Remember to execute this batch with '. ./init_env_*.sh' or 'source init_env_*.sh' in order the
# exported variables to become global
export DEV_SRC_DIR=/mnt/${PROJECT_SRC}
export DEV_BIN_DIR=/mnt/${PROJECT_BIN}

mount 192.168.7.1:/media/x ${DEV_SRC_DIR}
mount 192.168.7.1:/media/y ${DEV_BIN_DIR}

# Force the 'usb0' configuration to be loaded from /etc/network/interfaces in order to wake up
# the Internet connection (if available) + NTP synchro
ifup usb0
# route add default gw 192.168.7.1
# ntpdate europe.pool.ntp.org

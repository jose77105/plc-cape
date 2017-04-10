export SLOTS=/sys/devices/bone_capemgr.*/slots
echo PLC_CAPE_V2-SPI0 > $SLOTS
dmesg | tail

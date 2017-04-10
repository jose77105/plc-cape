export SLOTS=/sys/devices/bone_capemgr.*/slots
echo PLC_CAPE_V1-SPI0 > $SLOTS
dmesg | tail

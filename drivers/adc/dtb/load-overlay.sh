export SLOTS=/sys/devices/bone_capemgr.*/slots
echo PLC_CAPE-ADC > $SLOTS
dmesg | tail
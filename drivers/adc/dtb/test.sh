# Load-overlay
export SLOTS=/sys/devices/bone_capemgr.*/slots
echo PLC_CAPE-ADC > $SLOTS

# Load-modules
insmod ti_am335x_tscadc_plc.ko
insmod adc_hs.ko

# Create device node
mknod /dev/adchs_plc c 241 0

# Test a capture
cat /dev/adchs_plc

# Show debug messages
dmesg | tail
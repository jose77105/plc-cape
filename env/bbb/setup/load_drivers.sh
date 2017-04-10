#!/bin/bash

if [ $# != 2 ]
then
	echo "Error: incorrect params"
	echo "Usage: $(basename "${0}") PlcCapeVersion BBBVersion"
	exit 1
fi

# Symbol definition for overlays
export SLOTS=/sys/devices/bone_capemgr.*/slots

# Load modules
if ! (lsmod | grep -q spidev_plc)
then
	#
	# SPI
	#
	
	# Load SPI modules
	insmod $DEV_BIN_DIR/drivers/spi/edma_plc.ko
	insmod $DEV_BIN_DIR/drivers/spi/spi-omap2-mcspi_plc.ko
	insmod $DEV_BIN_DIR/drivers/spi/spidev_plc.ko

	# Oveerlay to activate the standard SPI0
	#  echo BB-SPIDEV0 > $SLOTS

	if [ $1 = 1 ]; then 
		# Overlay for PLC_CAPE Version 1 (custom SPI+DMA)
		echo PLC_CAPE_V1-SPI0 > $SLOTS
	elif [ $1 = 2 ]; then
		# Overlay for PLC_CAPE Version 2 (SPI with D0 & DI std + DMA)
		echo PLC_CAPE_V2-SPI0 > $SLOTS
	else
		echo "Unknown PlcCapeVersion param"
		exit 1
	fi
fi

if ! (lsmod | grep -q adc_hs)
then
	#
	# ADC
	#
	
	# Load ADC modules
	insmod $DEV_BIN_DIR/drivers/adc/ti_am335x_tscadc_plc.ko
	insmod $DEV_BIN_DIR/drivers/adc/adc_hs.ko
	
	# Overlay to enable std ADC
	#	echo cape-bone-iio > $SLOTS

	# Overlay to enable custom ADC
	echo PLC_CAPE-ADC > $SLOTS

	# Create device node
	# NOTE:
	#	* In BBB1 assigned node to ADC = 241
	#	* In BBB2 assigned node to ADC = 239
	if [ $2 = 1 ]; then
		mknod /dev/adchs_plc c 241 0
	elif [ $1 = 2 ]; then
		mknod /dev/adchs_plc c 239 0
	else
		echo "Unknown BBBVersion param"
		exit 1
	fi

	# To get a single ADC capture this code could be used
	# cat /dev/adchs_plc
fi

#
# LOG
#

# Lectura log kernel
# dmesg | tail

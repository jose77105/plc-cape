obj-m += spidev_plc.o spi-omap2-mcspi_plc.o edma_plc.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

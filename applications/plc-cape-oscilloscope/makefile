CC=g++
ADDITIONAL_CFLAGS = `pkg-config --cflags cairo gtk+-3.0` `xml2-config --cflags`
ADDITIONAL_LIBS = `pkg-config --libs cairo gtk+-3.0 fftw3` `xml2-config --libs` -lasound
ADDITIONAL_PLC_LIBS = plc-cape plc-gpio plc-adc plc-tools  
ADDITIONAL_HEADERS = $(DEV_SRC_DIR)/+common/api/*.h

TARGET = $(notdir $(CURDIR))
include $(DEV_SRC_DIR)/+common/make_object.mk
include makefile.targets

ADDITIONAL_CFLAGS = `xml2-config --cflags`
ADDITIONAL_LIBS = -lrt -lm -ldl -lpthread `xml2-config --libs` -lasound
ADDITIONAL_PLC_LIBS = plc-cape plc-gpio plc-adc plc-tools
ADDITIONAL_PLC_PLUGIN_CATEGORIES = ui decoder encoder
ADDITIONAL_HEADERS = $(DEV_SRC_DIR)/+common/api/*.h

TARGET = $(notdir $(CURDIR))
include $(DEV_SRC_DIR)/+common/make_object.mk
include makefile.targets

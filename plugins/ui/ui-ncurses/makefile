ADDITIONAL_LIBS = `pkg-config --libs ncurses menu form panel`
ADDITIONAL_HEADERS = \
	$(DEV_SRC_DIR)/+common/api/*.h \
	$(DEV_SRC_DIR)/plugins/ui/api/*.h

TARGET = $(notdir $(CURDIR)).so
include $(DEV_SRC_DIR)/+common/make_object.mk

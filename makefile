GROUP_DESCRIPTION = PROJECT

# NOTE: Compilation of drivers takes a while. As they are rarely modified
#	they are not included by default in the compilation list
#	Add a 'drivers' lines to 'MODULES' to include them in the loop or go
#	directly to the 'drivers' directory and execute 'make' from there

MODULES = \
	libraries \
	plugins \
	applications
	
include $(DEV_SRC_DIR)/+common/make_group.mk

# PURPOSE
#	Common make file to build drivers in 'out-of-tree' mode
#
# EXTERNAL VARIABLES
#	TARGET: [Required] Name of the folder where the customized driver resides
#

.PHONY: default all on-the-fly-makefile clean clean-build-objects 

ifndef TARGET
$(error 'TARGET' undefined)
endif

KDIR := /lib/modules/`uname -r`/build
DRIVER_BIN_DIR = $(DEV_BIN_DIR)/drivers/$(TARGET)
SOURCES = $(wildcard *.c *.h)
OBJECTS = $(patsubst %.c,%.o,$(wildcard *.c))
TMP_SOURCES_AT_BIN_DIR = $(addprefix $(DRIVER_BIN_DIR)/,$(SOURCES))

default: all

$(DRIVER_BIN_DIR):
	@echo "  Creating target dir '$@'"
	@mkdir -p $@

# Note: the kernel build system requires a 'makefile' on the source code directory with the 'obj-m'
# declaration
on-the-fly-makefile:
	@echo Building temporary '$(DRIVER_BIN_DIR)/makefile'
ifneq ("$(wildcard $(DRIVER_BIN_DIR)/makefile)","")
	@echo "WARNING! There is a makefile in the binary folder '$(DRIVER_BIN_DIR)' that is going " \
			"to be overwritten. It will be renamed to makefile.bck"
	@mv --interactive $(DRIVER_BIN_DIR)/makefile $(DRIVER_BIN_DIR)/makefile.bck
endif
	@echo obj-m += $(OBJECTS) > $(DRIVER_BIN_DIR)/makefile

all: $(DRIVER_BIN_DIR) on-the-fly-makefile
# 'ifneq' is a makefile command (vs shell command) -> must not be indented
ifneq ("$(wildcard $(TMP_SOURCES_AT_BIN_DIR))","")
	@echo "WARNING! There are some source code files (*.c, *.h) in the binary folder " \
			"'$(DRIVER_BIN_DIR)' that will be overwritten"
endif
	@echo Copying source code files to '$(DRIVER_BIN_DIR)'
	@cp --interactive --preserve=timestamps $(SOURCES) $(DRIVER_BIN_DIR)
	@echo obj-m += $(OBJECTS) > $(DRIVER_BIN_DIR)/makefile
# Note 1: The typical 'M=$(PWD)' points to the Makefile's location instead of '$(DRIVER_BIN_DIR)'
# Note 2: Notice the '-' at the beginning of the command. It means 'continue even if error'
# This is required to clean the temporary duplicated files even in case of compilation errors
	-@make -C $(KDIR) M=$(DRIVER_BIN_DIR) modules
	@echo Cleaning duplicated source code files in '$(DRIVER_BIN_DIR)'
	@rm $(TMP_SOURCES_AT_BIN_DIR)
	@rm $(DRIVER_BIN_DIR)/makefile

clean: on-the-fly-makefile
	@echo Full cleaning of $(DRIVER_BIN_DIR)
	@make -C $(KDIR) M=$(DRIVER_BIN_DIR) clean
	@rm $(DRIVER_BIN_DIR)/makefile
	
clean-build-objects: on-the-fly-makefile
	@echo Cleaning intermediate objects in $(DRIVER_BIN_DIR)
	@rename 's/\.ko/\.koo/' $(DRIVER_BIN_DIR)/*.ko
	@make -C $(KDIR) M=$(DRIVER_BIN_DIR) clean
	@rename 's/\.koo/\.ko/' $(DRIVER_BIN_DIR)/*.koo
	@rm $(DRIVER_BIN_DIR)/makefile

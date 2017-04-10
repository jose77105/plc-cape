# PURPOSE
#	Common makefile to be included by plc-cape based makefiles to build executables
#	or libraries (dynamic or static)
#
# EXTERNAL VARIABLES
#	TARGET: [Required] Compiler to be used (e.g. gcc, g++)
#	PROFILE: [Option, default: debug.bbb] Predefined profiles to configure some settings
#		(e.g. release.bbb)
#	CC: [Optional, default defined by make] Compiler to be used (e.g. gcc, g++)
#	ADDITIONAL_CFLAGS: [Optional] Flags to be added to effective CFLAGS
#		(e.g. `pkg-config --cflags gtk+-3.0`)
#	ADDITIONAL_LIBS: [Optional] Libraries to be included at linking (e.g. -lrt -lm -ldl -lpthread)
#	ADDITIONAL_PLC_LIBS: [Optional] Additional plc-cape libraries (e.g. plc-tools plc-adc)
#	ADDITIONAL_PLC_PLUGIN_CATEGORIES: [Optional] Dependencies to plug-in categories (e.g. decoder)
#	ADDITIONAL_HEADERS: [Optional] Other dependencies on compiling (e.g. api/*)
#	SOURCE_PATH: [Optional] If the source code and the 'makefile' are in different folders
#		this variable indicates the relativa path to source (e.g. for makefile in subdir -> ../)
#
# ENVIRONMENT VARIABLES
#	DEV_BIN_DIR: [Optional] absolute path for resulting objects and executables
#	DEV_SRC_DIR: [Optional, but required if DEV_BIN_DIR declared] absolute path of the
#		root source of the 'plc-cape' project
#
#	Note: if DEV_BIN_DIR is not defined in-place compilation is applied -> compilation
#		is done in the folder where the code resides
# 

# Make typical targets not to be related to any file
.PHONY: default all display-vars clean clean-build-objects

ifndef TARGET
$(error 'TARGET' undefined)
endif

ifndef CC
$(error 'CC' undefined)
endif

# This makefile is based on the presence of some environment variables. Check it:
#	* if 'DEV_BIN_DIR' exists 'DEV_SRC_DIR' must exists and the output-path is composed
#	* if 'DEV_BIN_DIR' doesn't exist set current dir as the default dir
MAKEFILE_DIR = $(CURDIR)
ifdef DEV_BIN_DIR
ifdef DEV_SRC_DIR
# Set build folder to '$(DEV_BLD_DIR)' using the tree structure of '$(DEV_SRC_DIR)'
BUILD_DIR = $(subst $(DEV_SRC_DIR), $(DEV_BIN_DIR), $(MAKEFILE_DIR))
else	# ifndef DEV_SRC_DIR
$(error 'DEV_SRC_DIR' environment variable required)
endif	# ifdef DEV_SRC_DIR
else	# ifndef DEV_BIN_DIR
BUILD_DIR = $(MAKEFILE_DIR)
endif	# ifdef DEV_BIN_DIR

BUILD_TARGET = $(BUILD_DIR)/$(TARGET)
BUILD_TARGET_NO_EXT = $(basename $(BUILD_TARGET))
BUILD_TARGET_SUFFIX = $(suffix $(BUILD_TARGET))

# CFLAGS tuning
# CFLAGS defaults = "debug.bbb"
CFLAGS = -g -Wall -DDEBUG
ifdef PROFILE
ifeq ($(PROFILE), release.bbb)
CFLAGS = -Wall -O3
else ifneq ($(PROFILE), debug.bbb)
$(error 'PROFILE=$(PROFILE)' unknown)
endif
endif	# ifdef PROFILE

# Extra CFLAGS depending on target
ifeq ($(BUILD_TARGET_SUFFIX), .so)
CFLAGS += -fwhole-program -fPIC
LIBS += -shared
else ifeq ($(BUILD_TARGET_SUFFIX), .a)
# In static libraries it is convenient to declare all the functions static by default to avoid
# conflicts between shared public symbols
CFLAGS += -fwhole-program -fPIC
endif

# Additional cutom CFLAGS
CFLAGS += $(ADDITIONAL_CFLAGS)

# Consider dependent plc-cape libraries if defined
LIBS += \
	$(ADDITIONAL_LIBS) \
	$(foreach library,$(ADDITIONAL_PLC_LIBS), \
			-L"$(DEV_BIN_DIR)/libraries/lib$(library)" -l$(library))

# Declare external header libraries to trigger the compiler when updated
ADDITIONAL_DEPENDENCIES = \
	$(foreach library,$(ADDITIONAL_PLC_LIBS),$(DEV_BIN_DIR)/libraries/lib$(library)/lib$(library).a)

HEADERS = \
	$(wildcard $(SOURCE_PATH)*.h) \
	$(ADDITIONAL_HEADERS) \
	$(foreach library,$(ADDITIONAL_PLC_LIBS),$(DEV_SRC_DIR)/libraries/lib$(library)/api/*.h) \
	$(foreach category,$(ADDITIONAL_PLC_PLUGIN_CATEGORIES), \
			$(DEV_SRC_DIR)/plugins/$(category)/api/*.h)

BUILD_OBJECTS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(notdir $(wildcard $(SOURCE_PATH)*.c)))

default: all

# The 'all: default' clause is not strictly required but it's convenient to simplify the importing
# of makefiles into tools as Eclipse executing a 'make all' by default to build a project
all: $(BUILD_TARGET)

display-vars:
	@echo ENVIRONMENT VARIABLES
	@echo DEV_BIN_DIR: $(DEV_BIN_DIR)
	@echo DEV_SRC_DIR: $(DEV_SRC_DIR)
	@echo 
	@echo EXTERNAL VARIABLES
	@echo TARGET: $(TARGET)
	@echo PROFILE: $(PROFILE)
	@echo CC: $(CC)
	@echo ADDITIONAL_CFLAGS: $(ADDITIONAL_CFLAGS)
	@echo ADDITIONAL_LIBS: $(ADDITIONAL_LIBS)
	@echo ADDITIONAL_PLC_LIBS: $(ADDITIONAL_PLC_LIBS)
	@echo ADDITIONAL_PLC_PLUGIN_CATEGORIES: $(ADDITIONAL_PLC_PLUGIN_CATEGORIES)
	@echo ADDITIONAL_HEADERS: $(ADDITIONAL_HEADERS)
	@echo 
	@echo INTERNAL VARIABLES
	@echo BUILD_TARGET: $(BUILD_TARGET)
	@echo CFLAGS: $(CFLAGS)
	@echo BUILD_OBJECTS: $(BUILD_OBJECTS)
	@echo LIBS: $(LIBS)
	@echo HEADERS: $(HEADERS)
	@echo ADDITIONAL_DEPENDENCIES: $(ADDITIONAL_DEPENDENCIES)

# If the SOURCE_PATH is not used 'BUILD_OBJECTS' could be declared as:
# BUILD_OBJECTS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(wildcard *.c))

# For simplicity check for changes on any header $(HEADERS)
# Also check for other explicit external dependencies as static libraries, api headers, etc.
# Double-quotes are used on 'echo' to preserve the leading space

$(BUILD_DIR)/%.o: $(SOURCE_PATH)%.c $(HEADERS)
	@echo "   $@"
	@$(CC) $(CFLAGS) -I"$(DEV_SRC_DIR)" -c $< -o $@

$(BUILD_DIR):
	@echo "  Creating target dir '$@'"
	@mkdir -p $@

$(BUILD_TARGET_NO_EXT): $(BUILD_DIR) $(BUILD_OBJECTS) $(ADDITIONAL_DEPENDENCIES)
	@echo "-> $@"
	@$(CC) $(BUILD_OBJECTS) -L"$(DEV_BIN_DIR)" -Wall $(LIBS) -o $@
	
$(BUILD_TARGET_NO_EXT).so: $(BUILD_DIR) $(BUILD_OBJECTS) $(ADDITIONAL_DEPENDENCIES)
	@echo "-> $@"
	@$(CC) $(BUILD_OBJECTS) -L"$(DEV_BIN_DIR)" -Wall $(LIBS) -o $@

$(BUILD_TARGET_NO_EXT).a: $(BUILD_DIR) $(BUILD_OBJECTS) $(ADDITIONAL_DEPENDENCIES)
	@echo "-> $@"
	@ar -r $(BUILD_TARGET) $(BUILD_OBJECTS)

clean:
	@echo $@ $(BUILD_TARGET)
	@-rm -f $(BUILD_OBJECTS)
	@-rm -f $(BUILD_TARGET)

clean-build-objects:
	@echo $@ $(BUILD_TARGET)
	@-rm -f $(BUILD_OBJECTS)
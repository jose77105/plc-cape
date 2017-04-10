#
# PURPOSE
#	Compile a group of projects together
#
# EXTERNAL VARIABLES
#	MODULES: [Required] List of modules to be compiled
#	GROUP_DESCRIPTION: [Optional] Category identifying the modules
#

# Make typical targets not to be related to any file
.PHONY: all clean

ifndef MODULES
$(error 'MODULES' undefined)
endif

all:
	@echo $(GROUP_DESCRIPTION) building started
	@for module in $(MODULES); do \
		${MAKE} all --no-print-directory -C $$module; \
	done
	@echo $(GROUP_DESCRIPTION) building finished
	
clean:
	@echo $(GROUP_DESCRIPTION) cleaning started
	@for module in $(MODULES); do \
		${MAKE} clean --no-print-directory -C $$module; \
	done
	@echo $(GROUP_DESCRIPTION) cleaning finished

clean-build-objects:
	@echo $(GROUP_DESCRIPTION) cleaning-build-objects started
	@for module in $(MODULES); do \
		${MAKE} clean-build-objects --no-print-directory -C $$module; \
	done
	@echo $(GROUP_DESCRIPTION) cleaning finished


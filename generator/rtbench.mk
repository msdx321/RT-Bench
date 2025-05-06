# force the shell to be bash
SHELL:=bash
.SHELLFLAGS := -eu -o pipefail -c
# make sure all the commands run in the same shell
.ONESHELL: #so that we stay in the same python virtualenv

# check for staticx
STATICX_PATH:=$(shell command -v staticx)

# If no target compiler specified, compiled with default gcc
CC ?= gcc

# Paths
RTBENCH_GENERATOR_DIR=$(strip $(dir $(abspath $(filter %rtbench.mk,$(MAKEFILE_LIST)))))
PROJ_ROOT=$(realpath $(RTBENCH_GENERATOR_DIR)/..)
RTBENCH_OBJ_FLDR=$(RTBENCH_GENERATOR_DIR)/object/$(notdir $(CROSS_COMPILE)$(CC))
RTBENCH_H_FLDR=$(RTBENCH_GENERATOR_DIR)/include
RTBENCH_SRC_FLDR=$(RTBENCH_GENERATOR_DIR)/src

# Basic compilation flags for rt-bench
override CFLAGS+=-O2 -Wall -g -I$(RTBENCH_H_FLDR) -DGCC

# Add linker's flags
override LDFLAGS+=-lrt -lm -pthread  -Wl,--wrap=free -Wl,--wrap=malloc -Wl,--wrap=mmap

#optional features

# try to include a local config file with all the optional feature variables
sinclude $(RTBENCH_GENERATOR_DIR)/../options.mk

#message to the user so that he knows what features are enabled
$(info )
$(info Features enabled:)
$(info )
$(info Using $(CC) as compiler)

ifneq ($(CROSS_COMPILE),)
  $(info Cross compiling with $(CROSS_COMPILE))
endif

# variables to avoid confusion between enabled and disabled features
MACRO_FEAT_ENABLED=1
MACRO_FEAT_DISABLED=0
FEAT_ENABLED=y
FEAT_DISABLED=n

# deadline scheduler options
ifeq ($(subst 1,$(FEAT_ENABLED),$(FEAT_SCHED_DEADLINE)),$(FEAT_ENABLED))
 $(info SCHED_DEADLINE support enabled)
 override CFLAGS += -DFEAT_SCHED_DEADLINE_SUPPORT=$(MACRO_FEAT_ENABLED)
else
 ifeq ($(subst 0,$(FEAT_DISABLED),$(FEAT_SCHED_DEADLINE)),$(FEAT_DISABLED))
 $(info SCHED_DEADLINE support disabled)
  override CFLAGS += -DFEAT_SCHED_DEADLINE_SUPPORT=$(MACRO_FEAT_DISABLED)
 endif
endif

# perf counters options
ifeq ($(subst 1,$(FEAT_ENABLED),$(FEAT_PERF)),$(FEAT_ENABLED))
 $(info Perf counters support enabled)
 override CFLAGS += -DFEAT_PERF_SUPPORT=$(MACRO_FEAT_ENABLED)
 ifeq ($(CORE),CORTEX_A53)
 $(info Using ARM Cortex A53 counters)
  override CFLAGS +=-D__aarch64__ -DCORTEX_A53
 endif
 ifeq ($(CORE),CORE_I7)
 $(info Using Intel Core i7 counters)
  override CFLAGS +=-D__x86_64__ -DCORE_I7
 endif
else
 ifeq ($(subst 0,$(FEAT_DISABLED),$(FEAT_PERF)),$(FEAT_DISABLED))
  $(info Perf counters support disabled)
  override CFLAGS += -DFEAT_PERF_SUPPORT=$(MACRO_FEAT_DISABLED)
 endif
endif

# json parser options
ifeq ($(subst 1,$(FEAT_ENABLED),$(FEAT_JSON)),$(FEAT_ENABLED))
 $(info JSON support enabled)
 override CFLAGS+=-DFEAT_JSON_SUPPORT=$(MACRO_FEAT_ENABLED)
 override LDFLAGS+=-ljson-c
else
 ifeq ($(subst 0,$(FEAT_DISABLED),$(FEAT_JSON)),$(FEAT_DISABLED))
  $(info JSON support disabled)
  override CFLAGS+=-DFEAT_JSON_SUPPORT=$(MACRO_FEAT_DISABLED)
 endif
endif

# Configure what to do with skipped deadlines
ifeq ($(subst 1,$(FEAT_ENABLED),$(FEAT_PRINT_SKIPPED_DEADLINE)),$(FEAT_ENABLED))
 $(info Print skipped deadlines enabled)
 override CFLAGS += -DFEAT_PRINT_SKIPPED_DEADLINE_SUPPORT=$(MACRO_FEAT_ENABLED)
else
 ifeq ($(subst 0,$(FEAT_DISABLED),$(FEAT_PRINT_SKIPPED_DEADLINE)),$(FEAT_DISABLED))
  $(info Print skipped deadlines disabled)
  override CFLAGS += -DFEAT_PRINT_SKIPPED_DEADLINE_SUPPORT=$(MACRO_FEAT_DISABLED)
 endif
endif


# Check if redirection of all output to log file desired
ifeq ($(subst 1,$(FEAT_ENABLED),$(FEAT_DEBUG_FILE)),$(FEAT_ENABLED))
 $(info Benchmark debug logs to file are enabled)
 override CFLAGS+=-DFEAT_DEBUG_FILE=$(MACRO_FEAT_ENABLED)
endif

ifeq ($(subst 1,$(FEAT_ENABLED),$(FEAT_GCC_STATIC)),$(FEAT_ENABLED))
 ifeq ($(FEAT_STATICX),$(FEAT_ENABLED))
  $(error Gcc static compilation and staticx are mutually exclusive, please disable one of them)
 else
  $(info Gcc static compilation enabled)
  $(info Make sure that all the libraries are available in static version)
  $(info Check the documentation for more information about dependencies)
  override LDFLAGS+=-static
 endif
else
 ifeq ($(subst 0,$(FEAT_DISABLED),$(FEAT_GCC_STATIC)),$(FEAT_DISABLED))
  $(info Gcc static compilation disabled)
 endif
endif

ifeq ($(subst 1,$(FEAT_ENABLED),$(FEAT_STATICX)),$(FEAT_ENABLED))
 #staticx command to pack all libraries in executable
$(info Staticx enabled, dinamically linked libraries will be packed in the executable)
 STATICX_CMD:=staticx
else
 STATICX_CMD:=@printf "Staticx disabled, %s will not be packed as %s\n"
 ifeq ($(subst 0,$(FEAT_DISABLED),$(FEAT_STATICX)),$(FEAT_DISABLED))
  $(info Staticx disabled)
 endif
endif

# Override synch macros if the user requests it
ifdef SYNCH_DELAY_REL_SEC
ifndef SYNCH_DELAY_REL_NSEC
 $(info Initial synch delay set to $(SYNCH_DELAY_REL_SEC) seconds)
endif
 override CFLAGS+=-DSYNCH_DELAY_REL_SEC=$(SYNCH_DELAY_REL_SEC)
endif

ifdef SYNCH_DELAY_REL_NSEC
ifndef SYNCH_DELAY_REL_SEC
 $(info Initial synch delay set to $(SYNCH_DELAY_REL_NSEC) nanoseconds)
 else
 $(info Initial synch delay set to $(SYNCH_DELAY_REL_SEC) seconds and $(SYNCH_DELAY_REL_NSEC) nanoseconds)
endif
 override CFLAGS+=-DSYNCH_DELAY_REL_NSEC=$(SYNCH_DELAY_REL_NSEC)
endif

# set custom alignment for the memory watcher, to support memory that is not byte addressable
ifeq ($(subst 1, $(FEAT_EANBLED),$(FEAT_MEM_WATCHER_ALIGN)),$(FEAT_ENABLED))
ifeq ($(MEM_WATCHER_ALIGN),1)
DLMALLOC_ALIGN=-DMALLOC_ALIGNMENT=1U
else ifeq ($(MEM_WATCHER_ALIGN),8)
DLMALLOC_ALIGN=-DMALLOC_ALIGNMENT=8U
else ifeq ($(MEM_WATCHER_ALIGN),16)
DLMALLOC_ALIGN=-DMALLOC_ALIGNMENT=16U
else
$(error alignment of $(MEM_WATCHER_ALIGN) is not supported, only 1, 8 and 16 are supported)
endif
$(info Memory alignment for set to $(MEM_WATCHER_ALIGN) bytes)
override CFLAGS+=-fpack-struct=$(MEM_WATCHER_ALIGN)
override CFLAGS+=-DFEAT_MEM_WATCHER_ALIGN=$(MACRO_FEAT_ENABLED)
override CFLAGS+=-DMEM_WATCHER_ALIGN=$(MEM_WATCHER_ALIGN)
endif

CXXFLAGS:=$(CFLAGS)

# Lists of all source files
RTBENCH_SRC=$(shell find $(RTBENCH_SRC_FLDR) -name '*.c')
# Lists of all object files
RTBENCH_O=$(addprefix $(RTBENCH_OBJ_FLDR)/,$(notdir $(RTBENCH_SRC:.c=.o)))
# Lists of all header files
RTBENCH_H=$(shell find $(RTBENCH_H_FLDR) -name '*.h')

# RT-Bench core dependencies
RTBENCH=$(PROJ_ROOT)/options.mk $(RTBENCH_OBJ_FLDR) $(RTBENCH_O) $(RTBENCH_H)

$(info )
$(info )

# RT-Bench core recipes
.PHONY: default staticx-check
## Add this recipe such that 'all' recipe in children makefile become the default one
default: all

# staticx target to setup the environment if staticx is enabled and not already on path
ifeq ($(FEAT_STATICX),$(FEAT_ENABLED))
## What to do if staticx is not already on path
ifeq ($(STATICX_PATH),)
$(info Staticx not found in path, installing it in a python virtualenv)
$(warning Make sure that ldd, readelf, objcopy and patchelf are installed in path)
# activate python venv if staticx is not in path
STATICX_REQ:=@source $(PROJ_ROOT)/.venv/bin/activate
staticx-check:
	@python -m venv $(PROJ_ROOT)/.venv
	@source $(PROJ_ROOT)/.venv/bin/activate
	@pip install -r $(PROJ_ROOT)/utils/python-dependencies
else
staticx-check:
endif
else
staticx-check:
endif

$(PROJ_ROOT)/options.mk:
		@touch $@

$(RTBENCH_OBJ_FLDR): %:
		@mkdir -p $@

$(RTBENCH_SRC_FLDR)/dlmalloc/source/dlmalloc.c:
	@echo 'Initialization and fetching of the pinned version of the dlmalloc submodule...'
	@git submodule update --init --recursive $(RTBENCH_SRC_FLDR)/dlmalloc

$(RTBENCH_OBJ_FLDR)/dlmalloc.o: $(RTBENCH_SRC_FLDR)/dlmalloc/source/dlmalloc.c $(PROJ_ROOT)/options.mk
	@echo "setting up dlmalloc"
	sed -i 's/\(extern void \*\)mbed_sbrk/\1rtbench_sbrk/' $(RTBENCH_SRC_FLDR)/dlmalloc/source/dlmalloc.c
	sed -E -i 's/#define MORECORE [^[:space:]]+/#define MORECORE rtbench_sbrk/' $(RTBENCH_SRC_FLDR)/dlmalloc/source/dlmalloc.c
	sed -i 's/#define MORECORE_CONTIGUOUS [01]/#define MORECORE_CONTIGUOUS 1/' $(RTBENCH_SRC_FLDR)/dlmalloc/source/dlmalloc.c
	sed -i 's/#define HAVE_MORECORE [01]/#define HAVE_MORECORE 1/' $(RTBENCH_SRC_FLDR)/dlmalloc/source/dlmalloc.c
	sed -i 's/#define HAVE_MMAP [01]/#define HAVE_MMAP 0/' $(RTBENCH_SRC_FLDR)/dlmalloc/source/dlmalloc.c
	sed -i 's/#define HAVE_MREMAP [01]/#define HAVE_MREMAP 0/' $(RTBENCH_SRC_FLDR)/dlmalloc/source/dlmalloc.c
	$(CROSS_COMPILE)$(CC) $(DLMALLOC_ALIGN) $(CFLAGS) -c $< -o $@ $(LDFLAGS)

$(RTBENCH_OBJ_FLDR)/main.o: $(RTBENCH_SRC_FLDR)/main.c  $(RTBENCH_H) $(PROJ_ROOT)/options.mk
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -c $< -o $@ $(LDFLAGS)

$(RTBENCH_OBJ_FLDR)/%.o: $(RTBENCH_SRC_FLDR)/%.c $(RTBENCH_H) $(PROJ_ROOT)/options.mk
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -c $< -o $@ $(LDFLAGS)
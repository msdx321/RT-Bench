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
CUR_DIR=$(strip $(dir $(abspath $(filter %rtbench.mk,$(MAKEFILE_LIST)))))
PROJ_ROOT=$(CUR_DIR)/..
OBJECT=$(CUR_DIR)/object/$(notdir $(CC))
INCLUDE=$(CUR_DIR)/include
SOURCE=$(CUR_DIR)/src

# Lists of all object files
BASE_O=$(OBJECT)/*.o

# Basic compilation flags for rt-bench
override CFLAGS+=-O2 -Wall -g -I$(INCLUDE) -DGCC

# Add linker's flags
override LDFLAGS+=-lrt -lm -pthread  -Wl,--wrap=free -Wl,--wrap=malloc -Wl,--wrap=mmap -Wl,--wrap=sbrk -Wl,--no-as-needed

#optional features

# #try to include a local config file with all the optional feature variables
sinclude $(CUR_DIR)../options.mk

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

# Check if extended report is desired
ifeq ($(subst 1,$(FEAT_ENABLED),$(FEAT_EXTENDED_REPORT)),$(FEAT_ENABLED))
 $(info Extended report enabled)
 override CFLAGS+=-DFEAT_EXTENDED_REPORT_SUPPORT=$(MACRO_FEAT_ENABLED)
endif

# Check if extended report is desired
ifeq ($(subst 1,$(FEAT_ENABLED),$(FEAT_BMARK_LOG_FILE)),$(FEAT_ENABLED))
 $(info Benchmark log file enabled)
 override CFLAGS+=-DFEAT_BMARK_LOG_FILE_SUPPORT=$(MACRO_FEAT_ENABLED)
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

CXXFLAGS:=$(CFLAGS)

$(info )
$(info )

# RT-Bench core recipes
.PHONY: default rtbench init dlmalloc staticx-check create-obj-folder
## Add this recipe such that 'all' recipe in children makefile become the default one
default: all
## Base recipe to build with the whole RT-Bench core!
rtbench: init main periodic_benchmark performance_sampler performance_counters memory_watcher logging get_cpu_timestamp dlmalloc

# staticx target to setup the environment if staticx is enabled and not already on path
ifeq ($(FEAT_STATICX),$(FEAT_ENABLED))
## What to do if staticx is not already on path
ifeq ($(STATICX_PATH),)
$(info Staticx not found in path, installing it in a python virtualenv)
$(warning Make sure that ldd, readelf, objcopy and patchelf are installed in path)
# activate python venv if statix is not in path
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

init: create-obj-folder dlmalloc staticx-check

create-obj-folder:
	@mkdir -p $(OBJECT)

dlmalloc: create-obj-folder
ifeq ("$(wildcard $(SOURCE)/dlmalloc/LICENSE)", "")
	@echo 'Initialization and fetching of the pinned version of the dlmalloc submodule...'
	@git submodule update --init --recursive $(SOURCE)/dlmalloc
	@echo "setting up dlmalloc"
	sed -E -i 's/#define MORECORE [^[:space:]]+/#define MORECORE sbrk/' $(SOURCE)/dlmalloc/source/dlmalloc.c
	sed -i 's/#define MORECORE_CONTIGUOUS [01]/#define MORECORE_CONTIGUOUS 1/' $(SOURCE)/dlmalloc/source/dlmalloc.c
	sed -i 's/#define HAVE_MORECORE [01]/#define HAVE_MORECORE 1/' $(SOURCE)/dlmalloc/source/dlmalloc.c
	sed -i 's/#define HAVE_MMAP [01]/#define HAVE_MMAP 0/' $(SOURCE)/dlmalloc/source/dlmalloc.c
	sed -i 's/#define HAVE_MREMAP [01]/#define HAVE_MREMAP 0/' $(SOURCE)/dlmalloc/source/dlmalloc.c
endif
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -c $(SOURCE)/dlmalloc/source/dlmalloc.c -o $(OBJECT)/dlmalloc.o $(LDFLAGS)

get_cpu_timestamp: init $(INCLUDE)/get_cpu_timestamp.h
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -c $(SOURCE)/get_cpu_timestamp.c -o $(OBJECT)/get_cpu_timestamp.o $(LDFLAGS)

logging: init $(INCLUDE)/logging.h
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -c $(SOURCE)/logging.c -o $(OBJECT)/logging.o $(LDFLAGS)

memory_watcher: init $(INCLUDE)/memory_watcher.h
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -c $(SOURCE)/memory_watcher.c -o $(OBJECT)/memory_watcher.o $(LDFLAGS)

performance_counters: init $(INCLUDE)/performance_counters.h
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -c $(SOURCE)/performance_counters.c -o $(OBJECT)/performance_counters.o $(LDFLAGS)

performance_sampler: init $(INCLUDE)/performance_sampler.h
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -c $(SOURCE)/performance_sampler.c -o $(OBJECT)/performance_sampelr.o $(LDFLAGS)

periodic_benchmark: init $(INCLUDE)/periodic_benchmark.h
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -c $(SOURCE)/periodic_benchmark.c -o $(OBJECT)/periodic_benchmark.o $(LDFLAGS)

main: init $(SOURCE)/main.c
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -c $(SOURCE)/main.c -o $(OBJECT)/main.o $(LDFLAGS)


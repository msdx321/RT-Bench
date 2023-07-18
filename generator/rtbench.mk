# If no target compiler specified, compiled with default gcc
CC ?= gcc

# Paths
CUR_DIR=$(dir $(lastword $(MAKEFILE_LIST)))
OBJECT=$(CUR_DIR)/object/$(notdir $(CC))
INCLUDE=$(CUR_DIR)/include
SOURCE=$(CUR_DIR)/src

# Lists of all object files
BASE_O=$(OBJECT)/*.o

# Basic compilation flags for rt-bench
override CFLAGS+=-O2 -Wall -g -I$(INCLUDE) -DGCC

# Add linker's flags
override LDFLAGS+=-lrt -lm -pthread -Wl,--wrap=malloc -Wl,--wrap=mmap -Wl,--no-as-needed

#optional features 

# #try to include a local config file with all the optional feature variables
sinclude ../options.mk

# variables to avoid confusion between enabled and disabled features
MACRO_FEAT_ENABLED=1
MACRO_FEAT_DISABLED=0
FEAT_ENABLED=y
FEAT_DISABLED=n

# deadline scheduler options
ifeq ($(FEAT_SCHED_DEADLINE),$(FEAT_ENABLED))
 override CFLAGS += -DFEAT_SCHED_DEADLINE_SUPPORT=$(MACRO_FEAT_ENABLED)
else
 ifeq ($(FEAT_SCHED_DEADLINE),$(FEAT_DISABLED))
  override CFLAGS += -DFEAT_SCHED_DEADLINE_SUPPORT=$(MACRO_FEAT_DISABLED)
 endif
endif

# perf counters options
ifeq ($(FEAT_PERF),$(FEAT_ENABLED))
 override CFLAGS += -DFEAT_PERF_SUPPORT=$(MACRO_FEAT_ENABLED)
 ifeq ($(CORE),CORTEX_A53)
  override CFLAGS +=-D__aarch64__ -DCORTEX_A53
 endif
 ifeq ($(CORE),CORE_I7)
  override CFLAGS +=-D__x86_64__ -DCORE_I7
 endif
else
 ifeq ($(FEAT_PERF),$(FEAT_DISABLED))
  override CFLAGS += -DFEAT_PERF_SUPPORT=$(MACRO_FEAT_DISABLED)
	endif
endif

# json parser options
ifeq ($(FEAT_JSON),$(FEAT_ENABLED))
 override CFLAGS+=-DFEAT_JSON_SUPPORT=$(MACRO_FEAT_ENABLED)
 override LDFLAGS+=-ljson-c
else
 ifeq ($(FEAT_JSON),$(FEAT_DISABLED))
  override CFLAGS+=-DFEAT_JSON_SUPPORT=$(MACRO_FEAT_DISABLED)
	endif
endif

# Configure what to do with skipped deadlines
ifeq ($(FEAT_PRINT_SKIPPED_DEADLINE),$(FEAT_ENABLED))
 override CFLAGS += -DFEAT_PRINT_SKIPPED_DEADLINE_SUPPORT=$(MACRO_FEAT_ENABLED)
else
 ifeq ($(FEAT_PRINT_SKIPPED_DEADLINE),$(FEAT_DISABLED))
 override CFLAGS += -DFEAT_PRINT_SKIPPED_DEADLINE_SUPPORT=$(MACRO_FEAT_DISABLED)
 endif
endif

# Check if extended report is desired
ifeq ($(EXTENDED_REPORT),1)
override CFLAGS+=-DEXTENDED_REPORT
endif

CXXFLAGS=$(CFLAGS)

# RT-Bench core recipes

.PHONY: default rtbench
## Add this recipe such that 'all' recipe in children makefile become the defualt one
default: all
## Base recipe to build with the whole RT-Bench core! 
rtbench: init main periodic_benchmark performance_sampler performance_counters memory_watcher logging get_cpu_timestamp

init:
	mkdir -p $(OBJECT)

get_cpu_timestamp: init $(INCLUDE)/get_cpu_timestamp.h
	$(CC) $(CFLAGS) -c $(SOURCE)/get_cpu_timestamp.c -o $(OBJECT)/get_cpu_timestamp.o $(LDFLAGS)

logging: init $(INCLUDE)/logging.h
	$(CC) $(CFLAGS) -c $(SOURCE)/logging.c -o $(OBJECT)/logging.o $(LDFLAGS)

memory_watcher: init $(INCLUDE)/memory_watcher.h
	$(CC) $(CFLAGS) -c $(SOURCE)/memory_watcher.c -o $(OBJECT)/memory_watcher.o $(LDFLAGS)

performance_counters: init $(INCLUDE)/performance_counters.h
	$(CC) $(CFLAGS) -c $(SOURCE)/performance_counters.c -o $(OBJECT)/performance_counters.o $(LDFLAGS)

performance_sampler: init $(INCLUDE)/performance_sampler.h
	$(CC) $(CFLAGS) -c $(SOURCE)/performance_sampler.c -o $(OBJECT)/performance_sampelr.o $(LDFLAGS)

periodic_benchmark: init $(INCLUDE)/periodic_benchmark.h
	$(CC) $(CFLAGS) -c $(SOURCE)/periodic_benchmark.c -o $(OBJECT)/periodic_benchmark.o $(LDFLAGS)

main: init $(SOURCE)/main.c
	$(CC) $(CFLAGS) -c $(SOURCE)/main.c -o $(OBJECT)/main.o $(LDFLAGS)


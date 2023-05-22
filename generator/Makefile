# If no target compiler specified, compiled with default gcc
CC ?= gcc

# Object path
GENERATOR=$(dir $(lastword $(MAKEFILE_LIST)))
BASE_O_PATH=$(GENERATOR)/object/$(CC)
BASE_O=$(BASE_O_PATH)/*.o

# Basic compilation flags for rt-bench
override CFLAGS+=-O2 -Wall -g -I$(GENERATOR) -DGCC
CXXFLAGS=$(CFLAGS)
# Add linker's flags
override LDFLAGS+=-lrt -lm -pthread -Wl,--wrap=malloc -Wl,--wrap=mmap -Wl,--no-as-needed

# Check for specified core architecture family. When specified, set the required flags.
ifdef CORE
ifeq ($(CORE),CORTEX_A53)
override CFLAGS +=-DAARCH64 -DCORTEX_A53
endif
ifeq ($(CORE),CORE_I7)
override CFLAGS +=-DX86_64 -DCORE_I7
endif
endif

# Check if json configuration input is desired.
ifeq ($(JSON),1)
override CFLAGS+=-DJSON_SUPPORT
override LDFLAGS+=-ljson-c
endif

# Check if extended report is desired
ifeq ($(EXTENDED_REPORT),1)
override CFLAGS+=-DEXTENDED_REPORT
endif

# RT-Bench core recipes

.PHONY: default rtbench
## Add this recipe such that 'all' recipe in children makefile become the defualt one
default: all
## Base recipe to build with the whole RT-Bench core! 
rtbench: init main periodic_benchmark performance_sampler performance_counters memory_watcher logging get_cpu_timestamp

init:
	mkdir -p $(BASE_O_PATH)

get_cpu_timestamp: init $(GENERATOR)/get_cpu_timestamp.h
	$(CC) $(CFLAGS) -c $(GENERATOR)/get_cpu_timestamp.c -o $(BASE_O_PATH)/get_cpu_timestamp.o $(LDFLAGS)

logging: init $(GENERATOR)/logging.h
	$(CC) $(CFLAGS) -c $(GENERATOR)/logging.c -o $(BASE_O_PATH)/logging.o $(LDFLAGS)

memory_watcher: init $(GENERATOR)/memory_watcher.h
	$(CC) $(CFLAGS) -c $(GENERATOR)/memory_watcher.c -o $(BASE_O_PATH)/memory_watcher.o $(LDFLAGS)

performance_counters: init $(GENERATOR)/performance_counters.h
	$(CC) $(CFLAGS) -c $(GENERATOR)/performance_counters.c -o $(BASE_O_PATH)/performance_counters.o $(LDFLAGS)

performance_sampler: init $(GENERATOR)/performance_sampler.h
	$(CC) $(CFLAGS) -c $(GENERATOR)/performance_sampler.c -o $(BASE_O_PATH)/performance_sampelr.o $(LDFLAGS)

periodic_benchmark: init $(GENERATOR)/periodic_benchmark.h
	$(CC) $(CFLAGS) -c $(GENERATOR)/periodic_benchmark.c -o $(BASE_O_PATH)/periodic_benchmark.o $(LDFLAGS)

main: init $(GENERATOR)/main.c
	$(CC) $(CFLAGS) -c $(GENERATOR)/main.c -o $(BASE_O_PATH)/main.o $(LDFLAGS)


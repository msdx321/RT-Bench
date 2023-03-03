# If no target compiler specified, compiled with default gcc
ifndef CC
CC=gcc
endif

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

# Add linker's flags
override LDFLAGS += -lrt -lm -pthread -Wl,--wrap=malloc -Wl,--wrap=mmap

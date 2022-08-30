CC=gcc
PGMS=bandwidth latency
BASE_SRC=$(wildcard ../generator/*.c)
BASE_O=$(patsubst %.c,%.o,$(BASE_SRC))
override CFLAGS+=-O2 -Wall -g -I../generator -DGCC -DEXTENDED_REPORT
CXXFLAGS=$(CFLAGS)
LDFLAGS=-Wl,--no-as-needed -lrt -lm -pthread -ljson-c -Wl,--wrap=malloc -Wl,--wrap=mmap


ifdef CORE
ifeq ($(CORE),CORTEX_A53)
override CFLAGS +=-DAARCH64 -DCORTEX_A53
endif
ifeq ($(CORE),CORE_I7)
override CFLAGS +=-DX86_64 -DCORE_I7
endif

endif


all: $(PGMS)
latency:latency.o $(BASE_O)
bandwidth: bandwidth.o
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(BASE_SRC) -o $@
clean:
	rm *.o $(PGMS) $(BASE_O)

include ../generator/rtbench.mk
CC ?=gcc
PGMS=overhead

ifeq ($(CC),gcc)
override CFLAGS+=-DGCC
endif

override CFLAGS+=-O2 -Wall -g -I../generator
CXXFLAGS=$(CFLAGS)

all: $(PGMS)
overhead:overhead.o rtbench
	$(CC) $(CFLAGS) $< $(BASE_O) -o $@ $(LDFLAGS)
clean:
	rm *.o $(PGMS) $(BASE_O)

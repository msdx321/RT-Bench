include ../generator/Makefile

PGMS=bandwidth latency
override CFLAGS+=-O2 -Wall -g -I../generator -DGCC -DEXTENDED_REPORT
CXXFLAGS=$(CFLAGS)
override LDFLAGS+=-Wl,--no-as-needed

all: $(PGMS)

latency: rtbench
	$(CC) $(CFLAGS) latency.c $(BASE_O) -o latency $(LDFLAGS)

bandwidth: rtbench
	$(CC) $(CFLAGS) bandwidth.c $(BASE_O) -o bandwidth $(LDFLAGS)

clean:
	rm -f *.o $(PGMS) $(BASE_O)

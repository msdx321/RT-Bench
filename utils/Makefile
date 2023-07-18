include ../generator/rtbench.mk
CC ?=gcc
PGMS=overhead get_fifo_max_prio

all: $(PGMS)
overhead:overhead.o rtbench
	$(CC) $(CFLAGS) $< $(BASE_O) -o $@ $(LDFLAGS)
get_fifo_max_prio: get_fifo_max_prio.o
	$(CC) $< -o $@ -O2 -Wall -g -DGCC -D_GNU_SOURCE
clean:
	rm *.o $(PGMS) $(BASE_O)

include ../generator/rtbench.mk
CC ?=gcc
PGMS=overhead get_fifo_max_prio synch-helper

all: $(PGMS)
overhead:overhead.o rtbench
	$(CC) $(CFLAGS) $< $(BASE_O) -o $@ $(LDFLAGS)
	$(STATICX_REQ)
	$(STATICX_CMD) overhead overhead.sx
get_fifo_max_prio: get_fifo_max_prio.o
	$(CC) $< -o $@ -O2 -Wall -g -DGCC -D_GNU_SOURCE
	$(STATICX_REQ)
	$(STATICX_CMD) get_fifo_max_prio get_fifo_max_prio.sx
synch-helper:synch-helper.c ../generator/include/synch_release_data.h
	$(CC) $(CFLAGS) $<  -o $@
	$(STATICX_REQ)
	$(STATICX_CMD) synch-helper synch-helper.sx
clean:
	@rm -f *.o *.sx $(PGMS) $(BASE_O)
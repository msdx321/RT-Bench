include ../generator/rtbench.mk
CC ?=gcc
PGMS=overhead get_fifo_max_prio

all: $(PGMS)
overhead:overhead.o rtbench
	$(CC) $(CFLAGS) $< $(BASE_O) -o $@ $(LDFLAGS)
	$(STATICX_REQ)
	$(STATICX_CMD) overhead overhead.sx
get_fifo_max_prio: get_fifo_max_prio.o
	$(CC) $< -o $@ -O2 -Wall -g -DGCC -D_GNU_SOURCE
	$(STATICX_REQ)
	$(STATICX_CMD) get_fifo_max_prio get_fifo_max_prio.sx
clean:
	@rm -f *.o *.sx $(PGMS) $(BASE_O)

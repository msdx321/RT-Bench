include ../generator/rtbench.mk
PGMS=overhead get_fifo_max_prio

all: $(PGMS)
overhead:overhead.c rtbench
	$(CROSS_COMPILE)$(CC) $(CFLAGS) $< $(BASE_O) -o $@ $(LDFLAGS)
	$(STATICX_REQ)
	$(STATICX_CMD) overhead overhead.sx
get_fifo_max_prio: get_fifo_max_prio.c
	$(CROSS_COMPILE)$(CC) $< -o $@ -O2 -Wall -g -DGCC -D_GNU_SOURCE
	$(STATICX_REQ)
	$(STATICX_CMD) get_fifo_max_prio get_fifo_max_prio.sx

clean:
	@rm -f *.o *.sx $(PGMS) $(BASE_O)

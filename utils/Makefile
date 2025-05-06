include ../generator/rtbench.mk
PGMS=overhead get_fifo_max_prio synch-helper

all: $(PGMS)

overhead: overhead.c $(RTBENCH)
	$(CROSS_COMPILE)$(CC) $(CFLAGS) $< $(RTBENCH_O) -o $@ $(LDFLAGS)
	$(STATICX_REQ)
	$(STATICX_CMD) overhead overhead.sx

get_fifo_max_prio: get_fifo_max_prio.c
	$(CROSS_COMPILE)$(CC) $< -o $@ -O2 -Wall -g -DGCC -D_GNU_SOURCE
	$(STATICX_REQ)
	$(STATICX_CMD) get_fifo_max_prio get_fifo_max_prio.sx

synch-helper: synch-helper.c ../generator/include/synch_release_data.h
	$(CROSS_COMPILE)$(CC) -g -Wall -DGCC -D_GNU_SOURCE -I$(RTBENCH_H_FLDR) $< -o $@
	$(STATICX_REQ)
	$(STATICX_CMD) synch-helper synch-helper.sx
clean:
	@rm -f *.o *.sx $(PGMS)
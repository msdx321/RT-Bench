#features enabled by default
FEAT_BMARK_LOG_FILE ?= 1
FEAT_EXTENDED_REPORT ?= 1

include ../generator/rtbench.mk

PGMS=bandwidth latency

all: $(PGMS)

latency: rtbench
	$(CROSS_COMPILE)$(CC) $(CFLAGS) latency.c $(BASE_O) -o latency $(LDFLAGS)
	$(STATICX_REQ)
	$(STATICX_CMD) latency latency.sx

bandwidth: rtbench
	$(CROSS_COMPILE)$(CC) $(CFLAGS) bandwidth.c $(BASE_O) -o bandwidth $(LDFLAGS)
	$(STATICX_REQ)
	$(STATICX_CMD) bandwidth bandwidth.sx

bandwidth-threads: rtbench
	$(CROSS_COMPILE)$(CC) $(CFLAGS) bandwidth-threads.c $(BASE_O) -o bandwidth-threads $(LDFLAGS)
	$(STATICX_REQ)
	$(STATICX_CMD) bandwidth-threads bandwidth-threads.sx

clean:
	@rm -f *.o *.sx $(PGMS) $(BASE_O)

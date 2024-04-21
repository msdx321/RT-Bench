include ../generator/rtbench.mk

PGMS=bandwidth latency

all: $(PGMS)

latency: rtbench
	$(CC) $(CFLAGS) latency.c $(BASE_O) -o latency $(LDFLAGS)
	$(STATICX_REQ)
	$(STATICX_CMD) latency latency.sx

bandwidth: rtbench
	$(CC) $(CFLAGS) bandwidth.c $(BASE_O) -o bandwidth $(LDFLAGS)
	$(STATICX_REQ)
	$(STATICX_CMD) bandwidth bandwidth.sx

clean:
	@rm -f *.o *.sx $(PGMS) $(BASE_O)

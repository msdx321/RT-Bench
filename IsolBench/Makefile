RTBENCH_PATH=$(realpath ../)
include $(RTBENCH_PATH)/generator/rtbench.mk

PGMS=bandwidth latency bandwidth-threads

all: $(PGMS)

$(PGMS): %: %.c $(RTBENCH)
	$(CROSS_COMPILE)$(CC) $(CFLAGS) $< $(RTBENCH_O) -o $@ $(LDFLAGS)
	$(STATICX_REQ)
	$(STATICX_CMD) $@ $@.sx

clean:
	@rm -f *.o *.sx $(PGMS) $(RTBENCH_O)
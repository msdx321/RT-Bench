include ../generator/rtbench.mk

RUST_TARGET_DIR = target
RUST_LIB_NAME = librust_demo.a
RUST_LIB_PATH = $(RUST_TARGET_DIR)/release/$(RUST_LIB_NAME)
RUST_DEMO_BINARY = rust-demo-benchmark

CARGO_PATH := $(shell command -v cargo)
ifeq ($(CARGO_PATH),)
$(error Cargo not found. Please install Rust and Cargo first)
endif

override LDFLAGS += -L$(RUST_TARGET_DIR)/release -lrust_demo

.PHONY: all clean rust-lib test-rust

all: rust-lib $(RUST_DEMO_BINARY)

rust-lib:
	@echo "Building Rust static library..."
	cargo build --release
	@echo "Rust library built: $(RUST_LIB_PATH)"

$(RUST_DEMO_BINARY): rust-lib $(RTBENCH)
	@echo "Linking rt-bench generator with Rust library..."
	$(CROSS_COMPILE)$(CC) $(CFLAGS) -I. -I../generator/include \
		$(RTBENCH_O) $(LDFLAGS) -o $@
	@echo "Binary created: $@"

test-rust:
	@echo "Running Rust tests..."
	cargo test

clean:
	@echo "Cleaning Rust build artifacts..."
	cargo clean
	rm -f $(RUST_DEMO_BINARY)

$(RUST_DEMO_BINARY)-static: $(RUST_DEMO_BINARY) staticx-check
	$(STATICX_REQ)
	$(STATICX_CMD) $< $@
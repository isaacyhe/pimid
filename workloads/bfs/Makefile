# Top-level Makefile for BFS workloads

.PHONY: all device host clean run-device run-host run-compare help

all: device host

# Build device-side version
device:
	@echo "Building device-side BFS with PIM annotations..."
	@$(MAKE) -C device

# Build host-only baseline version
host:
	@echo "Building host-only baseline BFS..."
	@$(MAKE) -C host

# Run device version
run-device: device
	@echo "Running device-side BFS..."
	@$(MAKE) -C device run

# Run host baseline version
run-host: host
	@echo "Running host-only baseline BFS..."
	@$(MAKE) -C host run

# Run both for comparison
run-compare: all
	@echo ""
	@echo "====================================="
	@echo "Running Device-Side BFS (with PIM):"
	@echo "====================================="
	@$(MAKE) -C device run
	@echo ""
	@echo "====================================="
	@echo "Running Host-Only Baseline BFS:"
	@echo "====================================="
	@$(MAKE) -C host run
	@echo ""
	@echo "Comparison complete!"

# Clean all builds
clean:
	@echo "Cleaning all builds..."
	@$(MAKE) -C device clean
	@$(MAKE) -C host clean

# Help message
help:
	@echo "BFS Workload Build System"
	@echo "========================="
	@echo ""
	@echo "Targets:"
	@echo "  all          - Build both device and host versions"
	@echo "  device       - Build device-side version with PIM annotations"
	@echo "  host         - Build host-only baseline version"
	@echo "  run-device   - Run device-side version"
	@echo "  run-host     - Run host-only baseline version"
	@echo "  run-compare  - Run both versions for comparison"
	@echo "  clean        - Clean all build artifacts"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make all           # Build both versions"
	@echo "  make run-compare   # Compare both versions"

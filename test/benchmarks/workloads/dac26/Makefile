# DAC26 Workloads Makefile
# PIMID-integrated benchmarks for PIM architecture evaluation

# Paths
PIMID_ROOT = ../../../..
DAC26_ADAPTER = $(PIMID_ROOT)/DAC26/pimid_adapter
POWER_MODELS = $(PIMID_ROOT)/pimid/power_models
MEMORY_MODELS = $(PIMID_ROOT)/pimid/memory_models
BUILD_DIR = $(PIMID_ROOT)/build
PIMID_INCLUDE = $(PIMID_ROOT)/pimid/include

# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra
INCLUDES = -I$(DAC26_ADAPTER) \
           -I$(PIMID_INCLUDE) \
           -I$(POWER_MODELS)/include \
           -I$(MEMORY_MODELS)/include \
           -I$(PIMID_ROOT)/pimid/external/ramulator/src \
           -I$(PIMID_ROOT)/ext/common

# Libraries
LDFLAGS = -L$(BUILD_DIR)/pimid -L$(BUILD_DIR)/pimid/power_models -L$(BUILD_DIR)/pimid/memory_models
LIBS = -lpthread -lm

# Source directories
MSG_DIR = message_passing
SHM_DIR = shared_memory

# Message Passing Workloads
MSG_WORKLOADS = bfs_message gemm_message spmv_message reduction_message \
                dotproduct_message histogram_message prefixsum_message stencil1d_message

# Shared Memory Workloads
SHM_WORKLOADS = bfs_shared gemm_shared spmv_shared reduction_shared \
                dotproduct_shared histogram_shared prefixsum_shared stencil1d_shared

# Object files
SIMULATOR_OBJ = pim_simulator.o
CACTI_WRAPPER_OBJ = cacti_wrapper.o
MCPAT_MODEL_OBJ = mcpat_model.o
POWER_MODEL_OBJ = power_model.o

ALL_OBJS = $(SIMULATOR_OBJ) $(CACTI_WRAPPER_OBJ) $(MCPAT_MODEL_OBJ) $(POWER_MODEL_OBJ)

# Targets
.PHONY: all message shared clean help test

all: message shared

message: $(MSG_WORKLOADS)

shared: $(SHM_WORKLOADS)

# Build simulator object
$(SIMULATOR_OBJ): $(DAC26_ADAPTER)/pim_simulator.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Build CACTI wrapper object
$(CACTI_WRAPPER_OBJ): $(MEMORY_MODELS)/src/cacti_wrapper.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Build McPAT model object
$(MCPAT_MODEL_OBJ): $(POWER_MODELS)/src/mcpat_model.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Build power model object
$(POWER_MODEL_OBJ): $(POWER_MODELS)/src/power_model.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Message passing workloads
$(MSG_WORKLOADS): %: $(MSG_DIR)/%_pimid.cpp $(ALL_OBJS)
	@echo "Building $@..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(ALL_OBJS) $(LDFLAGS) $(LIBS) -o $@

# Shared memory workloads
$(SHM_WORKLOADS): %: $(SHM_DIR)/%_pimid.cpp $(ALL_OBJS)
	@echo "Building $@..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(ALL_OBJS) $(LDFLAGS) $(LIBS) -o $@

# Testing
test: all
	@echo "=== Testing Message Passing Workloads ==="
	@echo "Testing reduction (message passing)..."
	@./reduction_message 8 1024 0 || echo "FAILED"
	@./reduction_message 8 1024 1 || echo "FAILED"
	@echo ""
	@echo "Testing bfs (message passing)..."
	@./bfs_message 8 64 0 || echo "FAILED"
	@./bfs_message 8 64 1 || echo "FAILED"
	@echo ""
	@echo "=== Testing Shared Memory Workloads ==="
	@echo "Testing reduction (shared memory)..."
	@./reduction_shared 8 1024 0 || echo "FAILED"
	@./reduction_shared 8 1024 1 || echo "FAILED"
	@echo ""
	@echo "Testing bfs (shared memory)..."
	@./bfs_shared 8 64 0 || echo "FAILED"
	@./bfs_shared 8 64 1 || echo "FAILED"
	@echo ""
	@echo "All tests completed!"

# Individual test targets
test-reduction-msg:
	@echo "=== Reduction (Message Passing) ==="
	@echo "Baseline (H-tree):"
	@./reduction_message 8 1024 0
	@echo ""
	@echo "LIBCom:"
	@./reduction_message 8 1024 1

test-reduction-shm:
	@echo "=== Reduction (Shared Memory) ==="
	@echo "Baseline (H-tree):"
	@./reduction_shared 8 1024 0
	@echo ""
	@echo "LIBCom:"
	@./reduction_shared 8 1024 1

test-bfs-msg:
	@echo "=== BFS (Message Passing) ==="
	@echo "Baseline (H-tree):"
	@./bfs_message 8 64 0
	@echo ""
	@echo "LIBCom:"
	@./bfs_message 8 64 1

test-bfs-shm:
	@echo "=== BFS (Shared Memory) ==="
	@echo "Baseline (H-tree):"
	@./bfs_shared 8 64 0
	@echo ""
	@echo "LIBCom:"
	@./bfs_shared 8 64 1

test-spmv-msg:
	@echo "=== SpMV (Message Passing) ==="
	@echo "Baseline (H-tree):"
	@./spmv_message 8 128 0
	@echo ""
	@echo "LIBCom:"
	@./spmv_message 8 128 1

test-gemm-msg:
	@echo "=== GEMM (Message Passing) ==="
	@echo "Baseline (H-tree):"
	@./gemm_message 8 64 0
	@echo ""
	@echo "LIBCom:"
	@./gemm_message 8 64 1

# Comprehensive benchmark suite
benchmark: all
	@echo "Running comprehensive DAC26 benchmark suite..."
	@./run_benchmark_suite.sh

# Clean
clean:
	rm -f $(MSG_WORKLOADS) $(SHM_WORKLOADS) *.o

# Help
help:
	@echo "DAC26 PIMID Workloads Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all              - Build all workloads (message + shared)"
	@echo "  message          - Build message passing workloads only"
	@echo "  shared           - Build shared memory workloads only"
	@echo "  test             - Run basic functionality tests"
	@echo "  test-*-msg       - Test specific message passing workload"
	@echo "  test-*-shm       - Test specific shared memory workload"
	@echo "  benchmark        - Run comprehensive benchmark suite"
	@echo "  clean            - Remove all build artifacts"
	@echo "  help             - Show this help message"
	@echo ""
	@echo "Workloads:"
	@echo "  Message Passing:"
	@echo "    - reduction_message, bfs_message, spmv_message, gemm_message"
	@echo "    - dotproduct_message, histogram_message, prefixsum_message, stencil1d_message"
	@echo ""
	@echo "  Shared Memory:"
	@echo "    - reduction_shared, bfs_shared, spmv_shared, gemm_shared"
	@echo "    - dotproduct_shared, histogram_shared, prefixsum_shared, stencil1d_shared"
	@echo ""
	@echo "Usage:"
	@echo "  ./<workload> <num_subarrays> <problem_size> <is_libcom>"
	@echo ""
	@echo "  num_subarrays: 8, 16, 32, etc."
	@echo "  problem_size:  Workload-specific (elements, vertices, matrix size)"
	@echo "  is_libcom:     0 = H-tree Baseline, 1 = LIBCom"
	@echo ""
	@echo "Examples:"
	@echo "  ./reduction_message 8 1024 0    # 8 subarrays, 1024 elems, H-tree"
	@echo "  ./bfs_message 16 128 1          # 16 subarrays, 128 vertices, LIBCom"
	@echo "  ./spmv_message 32 256 0         # 32 subarrays, 256x256 matrix, H-tree"

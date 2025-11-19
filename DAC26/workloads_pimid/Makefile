# Makefile for DAC26 PIMID-integrated workloads
# Technology: 45nm, 1GHz

CXX = g++
CXXFLAGS = -std=c++11 -O2 -Wall -Wextra
INCLUDES = -I../pimid_adapter -I../../pimid -I../../pimid/include -I../../pimid/power_models/include -I../../pimid/memory_models/include
LDFLAGS = -L../../pimid/build/power_models -L../../pimid/build/memory_models

# PIMID libraries
LIBS = -lpimid_power -lpimid_memory -lpthread

# Source files
ADAPTER_SRC = ../pimid_adapter/pim_simulator.cpp
PIMID_POWER_SRC = ../../pimid/power_models/src/power_model.cpp ../../pimid/power_models/src/mcpat_model.cpp

# Workload sources
WORKLOADS_SHARED = reduction_shared_pimid spmv_shared_pimid gemm_shared_pimid bfs_shared_pimid \
                   histogram_shared_pimid dotproduct_shared_pimid prefixsum_shared_pimid stencil1d_shared_pimid

WORKLOADS_MESSAGE = reduction_message_pimid spmv_message_pimid gemm_message_pimid bfs_message_pimid \
                    histogram_message_pimid dotproduct_message_pimid prefixsum_message_pimid stencil1d_message_pimid

WORKLOADS = $(WORKLOADS_SHARED) $(WORKLOADS_MESSAGE)

# Object files
ADAPTER_OBJ = ../pimid_adapter/pim_simulator.o
POWER_OBJS = power_model.o mcpat_model.o

all: $(WORKLOADS)

# Build adapter
../pimid_adapter/pim_simulator.o: $(ADAPTER_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $(ADAPTER_SRC) -o $@

# Build PIMID power model objects (local copies to avoid modifying pimid/)
power_model.o: ../../pimid/power_models/src/power_model.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

mcpat_model.o: ../../pimid/power_models/src/mcpat_model.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Build workloads
reduction_shared_pimid: reduction_shared_pimid.cpp $(ADAPTER_OBJ) $(POWER_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(ADAPTER_OBJ) $(POWER_OBJS) -o $@ -lm

spmv_shared_pimid: spmv_shared_pimid.cpp $(ADAPTER_OBJ) $(POWER_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(ADAPTER_OBJ) $(POWER_OBJS) -o $@ -lm

gemm_shared_pimid: gemm_shared_pimid.cpp $(ADAPTER_OBJ) $(POWER_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(ADAPTER_OBJ) $(POWER_OBJS) -o $@ -lm

bfs_shared_pimid: bfs_shared_pimid.cpp $(ADAPTER_OBJ) $(POWER_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(ADAPTER_OBJ) $(POWER_OBJS) -o $@ -lm

histogram_shared_pimid: histogram_shared_pimid.cpp $(ADAPTER_OBJ) $(POWER_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(ADAPTER_OBJ) $(POWER_OBJS) -o $@ -lm

dotproduct_shared_pimid: dotproduct_shared_pimid.cpp $(ADAPTER_OBJ) $(POWER_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(ADAPTER_OBJ) $(POWER_OBJS) -o $@ -lm

prefixsum_shared_pimid: prefixsum_shared_pimid.cpp $(ADAPTER_OBJ) $(POWER_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(ADAPTER_OBJ) $(POWER_OBJS) -o $@ -lm

stencil1d_shared_pimid: stencil1d_shared_pimid.cpp $(ADAPTER_OBJ) $(POWER_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(ADAPTER_OBJ) $(POWER_OBJS) -o $@ -lm

# Build message passing workloads
reduction_message_pimid: reduction_message_pimid.cpp $(ADAPTER_OBJ) $(POWER_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(ADAPTER_OBJ) $(POWER_OBJS) -o $@ -lm

spmv_message_pimid: spmv_message_pimid.cpp $(ADAPTER_OBJ) $(POWER_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(ADAPTER_OBJ) $(POWER_OBJS) -o $@ -lm

gemm_message_pimid: gemm_message_pimid.cpp $(ADAPTER_OBJ) $(POWER_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(ADAPTER_OBJ) $(POWER_OBJS) -o $@ -lm

bfs_message_pimid: bfs_message_pimid.cpp $(ADAPTER_OBJ) $(POWER_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(ADAPTER_OBJ) $(POWER_OBJS) -o $@ -lm

histogram_message_pimid: histogram_message_pimid.cpp $(ADAPTER_OBJ) $(POWER_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(ADAPTER_OBJ) $(POWER_OBJS) -o $@ -lm

dotproduct_message_pimid: dotproduct_message_pimid.cpp $(ADAPTER_OBJ) $(POWER_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(ADAPTER_OBJ) $(POWER_OBJS) -o $@ -lm

prefixsum_message_pimid: prefixsum_message_pimid.cpp $(ADAPTER_OBJ) $(POWER_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(ADAPTER_OBJ) $(POWER_OBJS) -o $@ -lm

stencil1d_message_pimid: stencil1d_message_pimid.cpp $(ADAPTER_OBJ) $(POWER_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(ADAPTER_OBJ) $(POWER_OBJS) -o $@ -lm

# Phony targets
.PHONY: all clean test

clean:
	rm -f $(WORKLOADS) *.o ../pimid_adapter/*.o

test: all
	@echo "=== Testing Shared Memory Workloads ==="
	@echo ""
	@echo "=== Testing Reduction Shared (Baseline) ==="
	./reduction_shared_pimid 8 1024 0
	@echo ""
	@echo "=== Testing SpMV Shared (Baseline) ==="
	./spmv_shared_pimid 8 128 0
	@echo ""
	@echo "=== Testing Dot Product Shared (LIBCom) ==="
	./dotproduct_shared_pimid 8 2048 1
	@echo ""
	@echo "=== Testing Histogram Shared (Baseline) ==="
	./histogram_shared_pimid 8 2048 0
	@echo ""
	@echo "=== Testing Message Passing Workloads ==="
	@echo ""
	@echo "=== Testing Reduction Message (Baseline) ==="
	./reduction_message_pimid 8 1024 0
	@echo ""
	@echo "=== Testing SpMV Message (LIBCom) ==="
	./spmv_message_pimid 8 128 1
	@echo ""
	@echo "=== Testing Dot Product Message (Baseline) ==="
	./dotproduct_message_pimid 8 2048 0
	@echo ""
	@echo "=== Testing Histogram Message (LIBCom) ==="
	./histogram_message_pimid 8 2048 1

help:
	@echo "DAC26 PIMID-Integrated Workloads"
	@echo "================================"
	@echo ""
	@echo "Targets:"
	@echo "  all                      - Build all workloads (shared + message)"
	@echo ""
	@echo "Shared Memory Workloads:"
	@echo "  reduction_shared_pimid   - Build reduction (shared) with PIMID"
	@echo "  spmv_shared_pimid        - Build SpMV (shared) with PIMID"
	@echo "  gemm_shared_pimid        - Build GEMM (shared) with PIMID"
	@echo "  bfs_shared_pimid         - Build BFS (shared) with PIMID"
	@echo "  histogram_shared_pimid   - Build Histogram (shared) with PIMID"
	@echo "  dotproduct_shared_pimid  - Build Dot Product (shared) with PIMID"
	@echo "  prefixsum_shared_pimid   - Build Prefix Sum (shared) with PIMID"
	@echo "  stencil1d_shared_pimid   - Build 1D Stencil (shared) with PIMID"
	@echo ""
	@echo "Message Passing Workloads:"
	@echo "  reduction_message_pimid  - Build reduction (message) with PIMID"
	@echo "  spmv_message_pimid       - Build SpMV (message) with PIMID"
	@echo "  gemm_message_pimid       - Build GEMM (message) with PIMID"
	@echo "  bfs_message_pimid        - Build BFS (message) with PIMID"
	@echo "  histogram_message_pimid  - Build Histogram (message) with PIMID"
	@echo "  dotproduct_message_pimid - Build Dot Product (message) with PIMID"
	@echo "  prefixsum_message_pimid  - Build Prefix Sum (message) with PIMID"
	@echo "  stencil1d_message_pimid  - Build 1D Stencil (message) with PIMID"
	@echo ""
	@echo "  test                     - Run test cases for both models"
	@echo "  clean                    - Remove build artifacts"
	@echo ""
	@echo "Configuration:"
	@echo "  Technology: 45nm"
	@echo "  Frequency: 1GHz"
	@echo "  Uses PIMID power/energy models"
	@echo "  Programming Models: Shared Memory + Message Passing"

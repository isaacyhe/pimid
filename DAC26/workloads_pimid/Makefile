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
WORKLOADS = reduction_shared_pimid

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

# Phony targets
.PHONY: all clean test

clean:
	rm -f $(WORKLOADS) *.o ../pimid_adapter/*.o

test: reduction_shared_pimid
	@echo "=== Testing Reduction Shared (Baseline) ==="
	./reduction_shared_pimid 8 1024 0
	@echo ""
	@echo "=== Testing Reduction Shared (LIBCom) ==="
	./reduction_shared_pimid 8 1024 1

help:
	@echo "DAC26 PIMID-Integrated Workloads"
	@echo "================================"
	@echo ""
	@echo "Targets:"
	@echo "  all                    - Build all workloads"
	@echo "  reduction_shared_pimid - Build reduction with PIMID"
	@echo "  test                   - Run test cases"
	@echo "  clean                  - Remove build artifacts"
	@echo ""
	@echo "Configuration:"
	@echo "  Technology: 45nm"
	@echo "  Frequency: 1GHz"
	@echo "  Uses PIMID power/energy models"

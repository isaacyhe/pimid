# Makefile for BFS Workload
# Can be built standalone or with PIMID support

CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra
LDFLAGS =

# PIMID paths (adjust if needed)
PIMID_ROOT = ../../pimid
PIMID_INCLUDE = $(PIMID_ROOT)/include
PIMID_LIB_DIR = $(PIMID_ROOT)/build
PIMID_LIB = -L$(PIMID_LIB_DIR) -lpimid_lib

# OpenMP support
OPENMP_FLAGS = -fopenmp
HAS_OPENMP := $(shell $(CXX) -fopenmp -x c++ -E - < /dev/null > /dev/null 2>&1 && echo 1 || echo 0)

# Targets
TARGET = bfs
TARGET_PIMID = bfs_pimid
TARGET_OMP = bfs_omp
TARGET_PIMID_OMP = bfs_pimid_omp

SRCS = bfs.cpp

.PHONY: all standalone pimid openmp pimid-openmp clean help

all: standalone

# Build standalone version (no PIMID, no OpenMP)
# Note: Links with PIMID library for API stubs, but PIMID simulation is not active
standalone: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -I$(PIMID_INCLUDE) -o $@ $^ $(LDFLAGS) $(PIMID_LIB)
	@echo "Built standalone BFS (no PIMID, no OpenMP)"

# Build with PIMID support
pimid: $(TARGET_PIMID)

$(TARGET_PIMID): $(SRCS)
	$(CXX) $(CXXFLAGS) -DPIMID_ENABLED -I$(PIMID_INCLUDE) -o $@ $^ $(LDFLAGS) $(PIMID_LIB)
	@echo "Built BFS with PIMID support"

# Build with OpenMP
# Note: Links with PIMID library for API stubs
openmp: $(TARGET_OMP)

$(TARGET_OMP): $(SRCS)
ifeq ($(HAS_OPENMP),1)
	$(CXX) $(CXXFLAGS) $(OPENMP_FLAGS) -I$(PIMID_INCLUDE) -o $@ $^ $(LDFLAGS) $(PIMID_LIB)
	@echo "Built BFS with OpenMP support"
else
	@echo "Error: OpenMP not available"
	@exit 1
endif

# Build with both PIMID and OpenMP
pimid-openmp: $(TARGET_PIMID_OMP)

$(TARGET_PIMID_OMP): $(SRCS)
ifeq ($(HAS_OPENMP),1)
	$(CXX) $(CXXFLAGS) $(OPENMP_FLAGS) -DPIMID_ENABLED -I$(PIMID_INCLUDE) -o $@ $^ $(LDFLAGS) $(PIMID_LIB)
	@echo "Built BFS with PIMID and OpenMP support"
else
	@echo "Error: OpenMP not available"
	@exit 1
endif

clean:
	rm -f $(TARGET) $(TARGET_PIMID) $(TARGET_OMP) $(TARGET_PIMID_OMP) *.o

help:
	@echo "BFS Workload Build System"
	@echo ""
	@echo "Targets:"
	@echo "  make                 Build standalone version (default)"
	@echo "  make standalone      Build without PIMID or OpenMP"
	@echo "  make pimid           Build with PIMID support"
	@echo "  make openmp          Build with OpenMP support"
	@echo "  make pimid-openmp    Build with both PIMID and OpenMP"
	@echo "  make clean           Remove all built files"
	@echo "  make help            Show this help message"
	@echo ""
	@echo "Usage Examples:"
	@echo "  Standalone:          ./bfs --vertices 100000 --degree 16"
	@echo "  With OpenMP:         ./bfs_omp --vertices 100000 --degree 16 --parallel"
	@echo "  Under PIMID:         pimid --config sim.yaml --workload ./bfs_pimid --vertices 100000"
	@echo "  PIMID + OpenMP:      pimid --config sim.yaml --workload ./bfs_pimid_omp --vertices 100000 --parallel"

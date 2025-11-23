# Makefile for True Host/Device Co-Simulation Workloads

CXX = g++
CXXFLAGS = -std=c++11 -O2 -Wall
LDFLAGS =

# Workloads
WORKLOADS = vector_add_cosim \
            matmul_tiled_cosim \
            histogram_merge_cosim \
            reduction_tree_cosim \
            bfs_iterative_cosim

all: $(WORKLOADS)

vector_add_cosim: vector_add_cosim.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

matmul_tiled_cosim: matmul_tiled_cosim.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

histogram_merge_cosim: histogram_merge_cosim.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

reduction_tree_cosim: reduction_tree_cosim.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

bfs_iterative_cosim: bfs_iterative_cosim.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(WORKLOADS)

.PHONY: all clean

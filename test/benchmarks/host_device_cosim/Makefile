# Makefile for True Host/Device Co-Simulation Workloads

CXX = g++
CXXFLAGS = -std=c++11 -O2 -Wall
LDFLAGS =

# Basic Workloads
BASIC_WORKLOADS = vector_add_cosim \
                  matmul_tiled_cosim \
                  histogram_merge_cosim \
                  reduction_tree_cosim \
                  bfs_iterative_cosim

# Complex Workloads
COMPLEX_WORKLOADS = image_convolution_cosim \
                    spmv_csr_cosim \
                    kmeans_clustering_cosim \
                    fft_butterfly_cosim \
                    pagerank_cosim

WORKLOADS = $(BASIC_WORKLOADS) $(COMPLEX_WORKLOADS)

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

# Complex workloads
image_convolution_cosim: image_convolution_cosim.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

spmv_csr_cosim: spmv_csr_cosim.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

kmeans_clustering_cosim: kmeans_clustering_cosim.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

fft_butterfly_cosim: fft_butterfly_cosim.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

pagerank_cosim: pagerank_cosim.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(WORKLOADS)

.PHONY: all clean

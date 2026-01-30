# Makefile for MPICH-based PIMID Workloads
#
# Requirements:
#   - MPICH installation (mpicc compiler)
#   - PIMID simulator configured
#
# Usage:
#   make all          - Build all workloads
#   make clean        - Remove all executables
#   make <target>     - Build specific workload

# Compiler and flags
MPICC = mpicc
CFLAGS = -O3 -Wall -std=c99
LDFLAGS = -lm

# Targets
TARGETS = vector_dotproduct matrix_multiply graph_pagerank

# Default target
all: $(TARGETS)

# Vector dot product
vector_dotproduct: vector_dotproduct.c
	$(MPICC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Built: $@"

# Matrix multiplication
matrix_multiply: matrix_multiply.c
	$(MPICC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Built: $@"

# Graph PageRank
graph_pagerank: graph_pagerank.c
	$(MPICC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Built: $@"

# Clean
clean:
	rm -f $(TARGETS)
	rm -f *.o
	@echo "Cleaned all executables"

# Run targets (for testing)
run-vector: vector_dotproduct
	mpirun -np 4 ./vector_dotproduct

run-matrix: matrix_multiply
	mpirun -np 4 ./matrix_multiply

run-pagerank: graph_pagerank
	mpirun -np 4 ./graph_pagerank

# Help
help:
	@echo "MPICH-based PIMID Workloads"
	@echo "============================"
	@echo "Available targets:"
	@echo "  all              - Build all workloads"
	@echo "  clean            - Remove all executables"
	@echo "  vector_dotproduct - Build vector dot product"
	@echo "  matrix_multiply  - Build matrix multiplication"
	@echo "  graph_pagerank   - Build graph PageRank"
	@echo ""
	@echo "Test targets:"
	@echo "  run-vector       - Run vector dot product (4 processes)"
	@echo "  run-matrix       - Run matrix multiplication (4 processes)"
	@echo "  run-pagerank     - Run graph PageRank (4 processes)"

.PHONY: all clean run-vector run-matrix run-pagerank help

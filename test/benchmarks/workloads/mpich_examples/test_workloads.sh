#!/bin/bash
# Test script for MPICH-based PIMID workloads
# This script builds and runs all workloads to verify functionality

set -e  # Exit on error

echo "=========================================="
echo "MPICH Workloads Test Suite"
echo "=========================================="
echo ""

# Check for MPICH installation
echo "1. Checking for MPICH installation..."
if ! command -v mpicc &> /dev/null; then
    echo "ERROR: mpicc not found. Please install MPICH."
    echo "  Ubuntu/Debian: sudo apt-get install mpich"
    echo "  RHEL/CentOS: sudo yum install mpich"
    exit 1
fi

MPICC_VERSION=$(mpicc --version | head -n 1)
echo "   Found: $MPICC_VERSION"
echo ""

# Check for mpirun
echo "2. Checking for mpirun..."
if ! command -v mpirun &> /dev/null; then
    echo "ERROR: mpirun not found."
    exit 1
fi
echo "   Found: $(which mpirun)"
echo ""

# Build all workloads
echo "3. Building all workloads..."
make clean > /dev/null 2>&1 || true
if make all; then
    echo "   ✓ All workloads built successfully"
else
    echo "   ✗ Build failed"
    exit 1
fi
echo ""

# Test configuration
NUM_PROCS=4
echo "4. Running workloads with $NUM_PROCS MPI processes..."
echo ""

# Test 1: Vector Dot Product
echo "   Test 1: Vector Dot Product"
echo "   ---------------------------"
if mpirun -np $NUM_PROCS ./vector_dotproduct > /tmp/test_vector.out 2>&1; then
    echo "   ✓ Vector dot product: PASSED"
    grep "Global dot product result" /tmp/test_vector.out || true
else
    echo "   ✗ Vector dot product: FAILED"
    cat /tmp/test_vector.out
    exit 1
fi
echo ""

# Test 2: Matrix Multiplication
echo "   Test 2: Matrix Multiplication"
echo "   ------------------------------"
if mpirun -np $NUM_PROCS ./matrix_multiply > /tmp/test_matrix.out 2>&1; then
    echo "   ✓ Matrix multiplication: PASSED"
    grep "Performance" /tmp/test_matrix.out || true
else
    echo "   ✗ Matrix multiplication: FAILED"
    cat /tmp/test_matrix.out
    exit 1
fi
echo ""

# Test 3: Graph PageRank
echo "   Test 3: Graph PageRank"
echo "   ----------------------"
if mpirun -np $NUM_PROCS ./graph_pagerank > /tmp/test_pagerank.out 2>&1; then
    echo "   ✓ Graph PageRank: PASSED"
    grep "Total iterations" /tmp/test_pagerank.out || true
else
    echo "   ✗ Graph PageRank: FAILED"
    cat /tmp/test_pagerank.out
    exit 1
fi
echo ""

# Summary
echo "=========================================="
echo "All tests passed successfully!"
echo "=========================================="
echo ""
echo "Workloads are ready for PIMID simulation."
echo "See README.md for usage instructions."
echo ""

# Cleanup test outputs
rm -f /tmp/test_vector.out /tmp/test_matrix.out /tmp/test_pagerank.out

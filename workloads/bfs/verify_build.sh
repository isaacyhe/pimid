#!/bin/bash
# Build verification script for BFS workloads
# Requires MPICH to be installed

echo "====================================="
echo "BFS Workload Build Verification"
echo "====================================="
echo ""

# Check for MPICH
if ! command -v mpicc &> /dev/null; then
    echo "ERROR: mpicc not found!"
    echo "Please install MPICH:"
    echo "  Ubuntu/Debian: sudo apt-get install mpich"
    echo "  RHEL/CentOS: sudo yum install mpich mpich-devel"
    echo "  macOS: brew install mpich"
    exit 1
fi

echo "Found MPICH: $(mpicc --version | head -1)"
echo ""

# Build both versions
echo "Building device-side version..."
cd device
make clean && make all
if [ $? -ne 0 ]; then
    echo "ERROR: Device build failed!"
    exit 1
fi
echo "Device build: SUCCESS"
echo ""

echo "Building host-only baseline..."
cd ../host
make clean && make all
if [ $? -ne 0 ]; then
    echo "ERROR: Host build failed!"
    exit 1
fi
echo "Host build: SUCCESS"
echo ""

echo "====================================="
echo "All builds completed successfully!"
echo "====================================="
echo ""
echo "To run:"
echo "  Device version: cd device && make run"
echo "  Host baseline:  cd host && make run"
echo "  Compare both:   make run-compare (from bfs/ directory)"

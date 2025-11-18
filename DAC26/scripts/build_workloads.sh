#!/bin/bash
# Build all DAC'26 workloads

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DAC26_DIR="$(dirname "$SCRIPT_DIR")"
WORKLOADS_DIR="$DAC26_DIR/workloads"
BUILD_DIR="$WORKLOADS_DIR/build"

echo "========================================="
echo "Building DAC'26 LIBCom Workloads"
echo "========================================="

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Run CMake
echo ""
echo "[1/2] Configuring with CMake..."
cmake ..

# Build
echo ""
echo "[2/2] Building workloads..."
cmake --build .

echo ""
echo "========================================="
echo "Build complete!"
echo "========================================="
echo "Executables:"
ls -lh gemm_workload bfs_workload spmv_workload reduction_workload 2>/dev/null || true
echo ""
echo "Run './run_all.sh' from the scripts directory to execute benchmarks"

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
echo "Total: 16 workload implementations"
echo ""
echo "Original workloads:"
ls -lh gemm_workload bfs_workload spmv_workload reduction_workload 2>/dev/null || echo "  (baseline implementations)"
echo ""
echo "Message Passing implementations:"
ls -lh gemm_message bfs_message spmv_message reduction_message 2>/dev/null || true
ls -lh histogram_message dotproduct_message prefixsum_message stencil1d_message 2>/dev/null || true
echo ""
echo "Shared Memory implementations:"
ls -lh gemm_shared bfs_shared spmv_shared reduction_shared 2>/dev/null || true
ls -lh histogram_shared dotproduct_shared prefixsum_shared stencil1d_shared 2>/dev/null || true
echo ""
echo "Run './run_all.sh' from the scripts directory to execute benchmarks"

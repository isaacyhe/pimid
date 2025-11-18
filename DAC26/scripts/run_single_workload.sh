#!/bin/bash
# Run a single DAC'26 workload with specified configuration

if [ "$#" -lt 3 ]; then
    echo "Usage: $0 <workload> <num_subarrays> <is_libcom> [additional_args...]"
    echo ""
    echo "Workloads:"
    echo "  gemm        - Matrix multiplication"
    echo "  bfs         - Graph traversal"
    echo "  spmv        - Sparse matrix-vector multiply"
    echo "  reduction   - Tree reduction"
    echo ""
    echo "Arguments:"
    echo "  num_subarrays - Number of subarrays (8, 16, or 32)"
    echo "  is_libcom     - 0 for baseline H-tree, 1 for LIBCom"
    echo ""
    echo "Examples:"
    echo "  $0 gemm 32 0 1024      # GEMM with 32 subarrays, baseline, matrix size 1024"
    echo "  $0 reduction 16 1 512  # Reduction with 16 subarrays, LIBCom, 512 elements"
    exit 1
fi

WORKLOAD=$1
NUM_SUBARRAYS=$2
IS_LIBCOM=$3
shift 3
EXTRA_ARGS=$@

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DAC26_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$DAC26_DIR/workloads/build"
RESULTS_DIR="$DAC26_DIR/results"

# Ensure results directory exists
mkdir -p "$RESULTS_DIR"

# Map workload name to executable
EXECUTABLE="$BUILD_DIR/${WORKLOAD}_workload"

if [ ! -f "$EXECUTABLE" ]; then
    echo "Error: Workload executable not found: $EXECUTABLE"
    echo "Run './build_workloads.sh' first"
    exit 1
fi

# Determine configuration name
if [ "$IS_LIBCOM" -eq 1 ]; then
    CONFIG_TYPE="libcom"
else
    CONFIG_TYPE="baseline"
fi

echo "========================================="
echo "DAC'26 Workload: ${WORKLOAD}"
echo "========================================="
echo "Subarrays: $NUM_SUBARRAYS"
echo "Configuration: $CONFIG_TYPE"
echo "Executable: $EXECUTABLE"
echo ""

# Run workload
"$EXECUTABLE" $NUM_SUBARRAYS $EXTRA_ARGS $IS_LIBCOM

echo ""
echo "========================================="
echo "Completed"
echo "========================================="

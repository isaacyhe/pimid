#!/bin/bash
# Run complete DAC'26 LIBCom evaluation suite
# Executes all workloads with all bank configurations

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DAC26_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$DAC26_DIR/workloads/build"
RESULTS_DIR="$DAC26_DIR/results"

# Create results directory
mkdir -p "$RESULTS_DIR"

# Timestamp for this run
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULT_FILE="$RESULTS_DIR/benchmark_${TIMESTAMP}.txt"

echo "=========================================" | tee "$RESULT_FILE"
echo "DAC'26 LIBCom vs Baseline H-tree" | tee -a "$RESULT_FILE"
echo "Complete Benchmark Suite" | tee -a "$RESULT_FILE"
echo "=========================================" | tee -a "$RESULT_FILE"
echo "Timestamp: $(date)" | tee -a "$RESULT_FILE"
echo "" | tee -a "$RESULT_FILE"

# Check if workloads are built
if [ ! -f "$BUILD_DIR/gemm_workload" ]; then
    echo "Error: Workloads not built. Run './build_workloads.sh' first" | tee -a "$RESULT_FILE"
    exit 1
fi

# Bank configurations
# Bank 1: 8 subarrays, 32KB
# Bank 2: 16 subarrays, 64KB
# Bank 3: 32 subarrays, 128KB

run_workload() {
    local workload=$1
    local num_subarrays=$2
    local bank_name=$3
    local extra_args=$4

    echo "" | tee -a "$RESULT_FILE"
    echo "=========================================" | tee -a "$RESULT_FILE"
    echo "$workload - $bank_name ($num_subarrays subarrays)" | tee -a "$RESULT_FILE"
    echo "=========================================" | tee -a "$RESULT_FILE"

    # Run baseline (H-tree)
    echo "" | tee -a "$RESULT_FILE"
    echo "--- Baseline (H-tree) ---" | tee -a "$RESULT_FILE"
    "$BUILD_DIR/${workload}_workload" $num_subarrays $extra_args 0 | tee -a "$RESULT_FILE"

    # Run LIBCom
    echo "" | tee -a "$RESULT_FILE"
    echo "--- LIBCom ---" | tee -a "$RESULT_FILE"
    "$BUILD_DIR/${workload}_workload" $num_subarrays $extra_args 1 | tee -a "$RESULT_FILE"

    echo "" | tee -a "$RESULT_FILE"
}

# =========================================
# GEMM Workload
# =========================================
echo "" | tee -a "$RESULT_FILE"
echo "####################################" | tee -a "$RESULT_FILE"
echo "# GEMM: Matrix Multiplication" | tee -a "$RESULT_FILE"
echo "####################################" | tee -a "$RESULT_FILE"

run_workload "gemm" 8 "Bank1-32KB" "512"     # 512×512 matrix
run_workload "gemm" 16 "Bank2-64KB" "1024"   # 1024×1024 matrix
run_workload "gemm" 32 "Bank3-128KB" "1024"  # 1024×1024 matrix

# =========================================
# BFS Workload
# =========================================
echo "" | tee -a "$RESULT_FILE"
echo "####################################" | tee -a "$RESULT_FILE"
echo "# BFS: Graph Traversal" | tee -a "$RESULT_FILE"
echo "####################################" | tee -a "$RESULT_FILE"

run_workload "bfs" 8 "Bank1-32KB" "64"     # 64 vertices
run_workload "bfs" 16 "Bank2-64KB" "128"   # 128 vertices
run_workload "bfs" 32 "Bank3-128KB" "256"  # 256 vertices

# =========================================
# SpMV Workload
# =========================================
echo "" | tee -a "$RESULT_FILE"
echo "####################################" | tee -a "$RESULT_FILE"
echo "# SpMV: Sparse Matrix-Vector" | tee -a "$RESULT_FILE"
echo "####################################" | tee -a "$RESULT_FILE"

run_workload "spmv" 8 "Bank1-32KB" "256"        # 256×256 sparse matrix
run_workload "spmv" 16 "Bank2-64KB" "512"       # 512×512 sparse matrix
run_workload "spmv" 32 "Bank3-128KB" "1024"     # 1024×1024 sparse matrix

# =========================================
# Reduction Workload
# =========================================
echo "" | tee -a "$RESULT_FILE"
echo "####################################" | tee -a "$RESULT_FILE"
echo "# Reduction: Tree Reduction" | tee -a "$RESULT_FILE"
echo "####################################" | tee -a "$RESULT_FILE"

run_workload "reduction" 8 "Bank1-32KB" "1024"   # 1024 elements per subarray
run_workload "reduction" 16 "Bank2-64KB" "1024"  # 1024 elements per subarray
run_workload "reduction" 32 "Bank3-128KB" "1024" # 1024 elements per subarray (CRITICAL TEST)

# =========================================
# Summary
# =========================================
echo "" | tee -a "$RESULT_FILE"
echo "=========================================" | tee -a "$RESULT_FILE"
echo "Benchmark Suite Completed" | tee -a "$RESULT_FILE"
echo "=========================================" | tee -a "$RESULT_FILE"
echo "Results saved to: $RESULT_FILE" | tee -a "$RESULT_FILE"
echo "" | tee -a "$RESULT_FILE"
echo "Next steps:" | tee -a "$RESULT_FILE"
echo "  1. Review results in $RESULT_FILE" | tee -a "$RESULT_FILE"
echo "  2. Run analysis: cd ../analysis && python analyze_results.py $RESULT_FILE" | tee -a "$RESULT_FILE"
echo "" | tee -a "$RESULT_FILE"

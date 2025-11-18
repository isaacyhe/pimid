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

run_workload_with_models() {
    local workload=$1
    local num_subarrays=$2
    local bank_name=$3
    local extra_args=$4

    echo "" | tee -a "$RESULT_FILE"
    echo "=========================================" | tee -a "$RESULT_FILE"
    echo "$workload - $bank_name ($num_subarrays subarrays)" | tee -a "$RESULT_FILE"
    echo "=========================================" | tee -a "$RESULT_FILE"

    # Message Passing Model - Baseline
    echo "" | tee -a "$RESULT_FILE"
    echo "--- Message Passing - Baseline (H-tree) ---" | tee -a "$RESULT_FILE"
    "$BUILD_DIR/${workload}_message" $num_subarrays $extra_args 0 | tee -a "$RESULT_FILE"

    # Message Passing Model - LIBCom
    echo "" | tee -a "$RESULT_FILE"
    echo "--- Message Passing - LIBCom ---" | tee -a "$RESULT_FILE"
    "$BUILD_DIR/${workload}_message" $num_subarrays $extra_args 1 | tee -a "$RESULT_FILE"

    # Shared Memory Model - Baseline
    echo "" | tee -a "$RESULT_FILE"
    echo "--- Shared Memory - Baseline (H-tree) ---" | tee -a "$RESULT_FILE"
    "$BUILD_DIR/${workload}_shared" $num_subarrays $extra_args 0 | tee -a "$RESULT_FILE"

    # Shared Memory Model - LIBCom
    echo "" | tee -a "$RESULT_FILE"
    echo "--- Shared Memory - LIBCom ---" | tee -a "$RESULT_FILE"
    "$BUILD_DIR/${workload}_shared" $num_subarrays $extra_args 1 | tee -a "$RESULT_FILE"

    echo "" | tee -a "$RESULT_FILE"
}

# =========================================
# GEMM Workload (Message + Shared)
# =========================================
echo "" | tee -a "$RESULT_FILE"
echo "####################################" | tee -a "$RESULT_FILE"
echo "# GEMM: Matrix Multiplication" | tee -a "$RESULT_FILE"
echo "####################################" | tee -a "$RESULT_FILE"

run_workload_with_models "gemm" 8 "Bank1-32KB" "512"     # 512×512 matrix
run_workload_with_models "gemm" 16 "Bank2-64KB" "1024"   # 1024×1024 matrix
run_workload_with_models "gemm" 32 "Bank3-128KB" "1024"  # 1024×1024 matrix

# =========================================
# BFS Workload (Message + Shared)
# =========================================
echo "" | tee -a "$RESULT_FILE"
echo "####################################" | tee -a "$RESULT_FILE"
echo "# BFS: Graph Traversal" | tee -a "$RESULT_FILE"
echo "####################################" | tee -a "$RESULT_FILE"

run_workload_with_models "bfs" 8 "Bank1-32KB" "64"     # 64 vertices
run_workload_with_models "bfs" 16 "Bank2-64KB" "128"   # 128 vertices
run_workload_with_models "bfs" 32 "Bank3-128KB" "256"  # 256 vertices

# =========================================
# SpMV Workload (Message + Shared)
# =========================================
echo "" | tee -a "$RESULT_FILE"
echo "####################################" | tee -a "$RESULT_FILE"
echo "# SpMV: Sparse Matrix-Vector" | tee -a "$RESULT_FILE"
echo "####################################" | tee -a "$RESULT_FILE"

run_workload_with_models "spmv" 8 "Bank1-32KB" "256"        # 256×256 sparse matrix
run_workload_with_models "spmv" 16 "Bank2-64KB" "512"       # 512×512 sparse matrix
run_workload_with_models "spmv" 32 "Bank3-128KB" "1024"     # 1024×1024 sparse matrix

# =========================================
# Reduction Workload (Message + Shared)
# =========================================
echo "" | tee -a "$RESULT_FILE"
echo "####################################" | tee -a "$RESULT_FILE"
echo "# Reduction: Tree Reduction" | tee -a "$RESULT_FILE"
echo "####################################" | tee -a "$RESULT_FILE"

run_workload_with_models "reduction" 8 "Bank1-32KB" "1024"   # 1024 elements per subarray
run_workload_with_models "reduction" 16 "Bank2-64KB" "1024"  # 1024 elements per subarray
run_workload_with_models "reduction" 32 "Bank3-128KB" "1024" # 1024 elements per subarray (CRITICAL TEST)

# =========================================
# Histogram Workload (Message + Shared)
# =========================================
echo "" | tee -a "$RESULT_FILE"
echo "####################################" | tee -a "$RESULT_FILE"
echo "# Histogram: Parallel Histogram" | tee -a "$RESULT_FILE"
echo "####################################" | tee -a "$RESULT_FILE"

run_workload_with_models "histogram" 8 "Bank1-32KB" "2048 256"     # 2048 elements, 256 bins
run_workload_with_models "histogram" 16 "Bank2-64KB" "4096 256"   # 4096 elements, 256 bins
run_workload_with_models "histogram" 32 "Bank3-128KB" "8192 256"  # 8192 elements, 256 bins

# =========================================
# Dot Product Workload (Message + Shared)
# =========================================
echo "" | tee -a "$RESULT_FILE"
echo "####################################" | tee -a "$RESULT_FILE"
echo "# Dot Product: Vector Dot Product" | tee -a "$RESULT_FILE"
echo "####################################" | tee -a "$RESULT_FILE"

run_workload_with_models "dotproduct" 8 "Bank1-32KB" "2048"     # 2048 elements
run_workload_with_models "dotproduct" 16 "Bank2-64KB" "4096"    # 4096 elements
run_workload_with_models "dotproduct" 32 "Bank3-128KB" "8192"   # 8192 elements

# =========================================
# Prefix Sum Workload (Message + Shared)
# =========================================
echo "" | tee -a "$RESULT_FILE"
echo "####################################" | tee -a "$RESULT_FILE"
echo "# Prefix Sum: Parallel Scan" | tee -a "$RESULT_FILE"
echo "####################################" | tee -a "$RESULT_FILE"

run_workload_with_models "prefixsum" 8 "Bank1-32KB" "2048"     # 2048 elements
run_workload_with_models "prefixsum" 16 "Bank2-64KB" "4096"    # 4096 elements
run_workload_with_models "prefixsum" 32 "Bank3-128KB" "8192"   # 8192 elements

# =========================================
# Stencil 1D Workload (Message + Shared)
# =========================================
echo "" | tee -a "$RESULT_FILE"
echo "####################################" | tee -a "$RESULT_FILE"
echo "# Stencil 1D: 1D Stencil Operation" | tee -a "$RESULT_FILE"
echo "####################################" | tee -a "$RESULT_FILE"

run_workload_with_models "stencil1d" 8 "Bank1-32KB" "2048 3"     # 2048 elements, radius 3
run_workload_with_models "stencil1d" 16 "Bank2-64KB" "4096 3"    # 4096 elements, radius 3
run_workload_with_models "stencil1d" 32 "Bank3-128KB" "8192 3"   # 8192 elements, radius 3

# =========================================
# Summary
# =========================================
echo "" | tee -a "$RESULT_FILE"
echo "=========================================" | tee -a "$RESULT_FILE"
echo "Benchmark Suite Completed" | tee -a "$RESULT_FILE"
echo "=========================================" | tee -a "$RESULT_FILE"
echo "Total workloads: 8 (each with 2 programming models)" | tee -a "$RESULT_FILE"
echo "Total implementations: 16" | tee -a "$RESULT_FILE"
echo "Bank configurations: 3 (8, 16, 32 subarrays)" | tee -a "$RESULT_FILE"
echo "Interconnects: 2 (Baseline H-tree, LIBCom)" | tee -a "$RESULT_FILE"
echo "" | tee -a "$RESULT_FILE"
echo "Results saved to: $RESULT_FILE" | tee -a "$RESULT_FILE"
echo "" | tee -a "$RESULT_FILE"
echo "Next steps:" | tee -a "$RESULT_FILE"
echo "  1. Review results in $RESULT_FILE" | tee -a "$RESULT_FILE"
echo "  2. Run analysis: cd ../analysis && python analyze_results.py $RESULT_FILE" | tee -a "$RESULT_FILE"
echo "" | tee -a "$RESULT_FILE"

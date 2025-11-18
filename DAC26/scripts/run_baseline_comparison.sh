#!/bin/bash
# Compare three network topologies:
# 1. Baseline: H-tree bus only (no switches, traditional SRAM)
# 2. H-tree with switches (adds routing capability)
# 3. LIBCom (direct paths)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DAC26_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$DAC26_DIR/workloads/build"
RESULTS_DIR="$DAC26_DIR/results"

mkdir -p "$RESULTS_DIR"

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULT_FILE="$RESULTS_DIR/baseline_comparison_${TIMESTAMP}.txt"

echo "=========================================" | tee "$RESULT_FILE"
echo "DAC'26 Baseline Topology Comparison" | tee -a "$RESULT_FILE"
echo "=========================================" | tee -a "$RESULT_FILE"
echo "Timestamp: $(date)" | tee -a "$RESULT_FILE"
echo "" | tee -a "$RESULT_FILE"

# Check if workload is built
if [ ! -f "$BUILD_DIR/reduction_message_vc" ]; then
    echo "Error: reduction_message_vc not built. Building now..." | tee -a "$RESULT_FILE"
    cd "$BUILD_DIR" && make reduction_message_vc
fi

# Function to run a single configuration
run_config() {
    local num_subarrays=$1
    local elements_per_sa=$2
    local bank_name=$3
    local topology=$4
    local num_vcs=$5
    local topo_name=$6

    echo "" | tee -a "$RESULT_FILE"
    echo "=========================================" | tee -a "$RESULT_FILE"
    echo "Bank: $bank_name | Topology: $topo_name | VCs: $num_vcs" | tee -a "$RESULT_FILE"
    echo "=========================================" | tee -a "$RESULT_FILE"

    "$BUILD_DIR/reduction_message_vc" $num_subarrays $elements_per_sa $topology $num_vcs | tee -a "$RESULT_FILE"

    echo "" | tee -a "$RESULT_FILE"
}

echo "=========================================" | tee -a "$RESULT_FILE"
echo "BANK 3: 128KB with 32 Subarrays" | tee -a "$RESULT_FILE"
echo "Critical Test Case for Topology Comparison" | tee -a "$RESULT_FILE"
echo "=========================================" | tee -a "$RESULT_FILE"

# Baseline: H-tree bus only (no switches)
run_config 32 1024 "Bank3-128KB" 0 1 "H-tree Bus Only (Baseline)"

# H-tree with switches (31 switches)
run_config 32 1024 "Bank3-128KB" 1 1 "H-tree with Switches (31 switches)"
run_config 32 1024 "Bank3-128KB" 1 2 "H-tree with Switches (31 switches, 2 VCs)"
run_config 32 1024 "Bank3-128KB" 1 4 "H-tree with Switches (31 switches, 4 VCs)"

# LIBCom (direct paths)
run_config 32 1024 "Bank3-128KB" 2 1 "LIBCom"

echo "=========================================" | tee -a "$RESULT_FILE"
echo "BANK 2: 64KB with 16 Subarrays" | tee -a "$RESULT_FILE"
echo "=========================================" | tee -a "$RESULT_FILE"

run_config 16 1024 "Bank2-64KB" 0 1 "H-tree Bus Only (Baseline)"
run_config 16 1024 "Bank2-64KB" 1 1 "H-tree with Switches (15 switches)"
run_config 16 1024 "Bank2-64KB" 1 4 "H-tree with Switches (15 switches, 4 VCs)"
run_config 16 1024 "Bank2-64KB" 2 1 "LIBCom"

echo "=========================================" | tee -a "$RESULT_FILE"
echo "BANK 1: 32KB with 8 Subarrays" | tee -a "$RESULT_FILE"
echo "=========================================" | tee -a "$RESULT_FILE"

run_config 8 1024 "Bank1-32KB" 0 1 "H-tree Bus Only (Baseline)"
run_config 8 1024 "Bank1-32KB" 1 1 "H-tree with Switches (7 switches)"
run_config 8 1024 "Bank1-32KB" 1 4 "H-tree with Switches (7 switches, 4 VCs)"
run_config 8 1024 "Bank1-32KB" 2 1 "LIBCom"

# Summary
echo "" | tee -a "$RESULT_FILE"
echo "=========================================" | tee -a "$RESULT_FILE"
echo "Topology Comparison Complete" | tee -a "$RESULT_FILE"
echo "=========================================" | tee -a "$RESULT_FILE"
echo "Three topologies tested:" | tee -a "$RESULT_FILE"
echo "  1. H-tree Bus Only: Traditional SRAM with ALUs" | tee -a "$RESULT_FILE"
echo "     - All transfers through central port" | tee -a "$RESULT_FILE"
echo "     - No switching infrastructure" | tee -a "$RESULT_FILE"
echo "     - Worst performance (baseline)" | tee -a "$RESULT_FILE"
echo "" | tee -a "$RESULT_FILE"
echo "  2. H-tree with Switches: Adds routing capability" | tee -a "$RESULT_FILE"
echo "     - N-1 switches for N subarrays" | tee -a "$RESULT_FILE"
echo "     - Better than baseline" | tee -a "$RESULT_FILE"
echo "     - Still limited by tree topology" | tee -a "$RESULT_FILE"
echo "" | tee -a "$RESULT_FILE"
echo "  3. LIBCom: Direct paths" | tee -a "$RESULT_FILE"
echo "     - Point-to-point connections" | tee -a "$RESULT_FILE"
echo "     - 0 contention cycles" | tee -a "$RESULT_FILE"
echo "     - Best performance" | tee -a "$RESULT_FILE"
echo "" | tee -a "$RESULT_FILE"
echo "Results saved to: $RESULT_FILE" | tee -a "$RESULT_FILE"

#!/bin/bash
# Comprehensive VC × Buffer Depth evaluation
# Tests all combinations: (1, 2, 4 VCs) × (1d, 2d, 4d buffer depths)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DAC26_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$DAC26_DIR/workloads/build"
RESULTS_DIR="$DAC26_DIR/results"

mkdir -p "$RESULTS_DIR"

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULT_FILE="$RESULTS_DIR/vc_buffer_sweep_${TIMESTAMP}.txt"

echo "=========================================" | tee "$RESULT_FILE"
echo "DAC'26 VC × Buffer Depth Evaluation" | tee -a "$RESULT_FILE"
echo "=========================================" | tee -a "$RESULT_FILE"
echo "Timestamp: $(date)" | tee -a "$RESULT_FILE"
echo "" | tee -a "$RESULT_FILE"
echo "Testing all combinations:" | tee -a "$RESULT_FILE"
echo "  VCs: 1 (no-vc), 2, 4" | tee -a "$RESULT_FILE"
echo "  Buffer Depths: 1d, 2d, 4d" | tee -a "$RESULT_FILE"
echo "  Total configurations: 9 per bank" | tee -a "$RESULT_FILE"
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
    local buffer_depth=$6
    local config_name=$7

    echo "" | tee -a "$RESULT_FILE"
    echo "=========================================" | tee -a "$RESULT_FILE"
    echo "Bank: $bank_name | Config: $config_name" | tee -a "$RESULT_FILE"
    echo "=========================================" | tee -a "$RESULT_FILE"

    "$BUILD_DIR/reduction_message_vc" $num_subarrays $elements_per_sa $topology $num_vcs $buffer_depth | tee -a "$RESULT_FILE"

    echo "" | tee -a "$RESULT_FILE"
}

# Bank 3: 128KB with 32 Subarrays (Critical Test Case)
echo "=========================================" | tee -a "$RESULT_FILE"
echo "BANK 3: 128KB with 32 Subarrays (31 switches)" | tee -a "$RESULT_FILE"
echo "=========================================" | tee -a "$RESULT_FILE"

# 1 VC (no-vc) with different buffer depths
run_config 32 1024 "Bank3-128KB" 1 1 1 "1 VC, 1d buffer"
run_config 32 1024 "Bank3-128KB" 1 1 2 "1 VC, 2d buffer"
run_config 32 1024 "Bank3-128KB" 1 1 4 "1 VC, 4d buffer"

# 2 VCs with different buffer depths
run_config 32 1024 "Bank3-128KB" 1 2 1 "2 VCs, 1d buffer"
run_config 32 1024 "Bank3-128KB" 1 2 2 "2 VCs, 2d buffer"
run_config 32 1024 "Bank3-128KB" 1 2 4 "2 VCs, 4d buffer"

# 4 VCs with different buffer depths
run_config 32 1024 "Bank3-128KB" 1 4 1 "4 VCs, 1d buffer"
run_config 32 1024 "Bank3-128KB" 1 4 2 "4 VCs, 2d buffer"
run_config 32 1024 "Bank3-128KB" 1 4 4 "4 VCs, 4d buffer"

# LIBCom (for comparison)
run_config 32 1024 "Bank3-128KB" 2 1 2 "LIBCom (baseline)"

# Bank 2: 64KB with 16 Subarrays
echo "=========================================" | tee -a "$RESULT_FILE"
echo "BANK 2: 64KB with 16 Subarrays (15 switches)" | tee -a "$RESULT_FILE"
echo "=========================================" | tee -a "$RESULT_FILE"

# Sample configurations for Bank 2
run_config 16 1024 "Bank2-64KB" 1 1 1 "1 VC, 1d buffer"
run_config 16 1024 "Bank2-64KB" 1 1 4 "1 VC, 4d buffer"
run_config 16 1024 "Bank2-64KB" 1 4 4 "4 VCs, 4d buffer"
run_config 16 1024 "Bank2-64KB" 2 1 2 "LIBCom (baseline)"

# Bank 1: 32KB with 8 Subarrays
echo "=========================================" | tee -a "$RESULT_FILE"
echo "BANK 1: 32KB with 8 Subarrays (7 switches)" | tee -a "$RESULT_FILE"
echo "=========================================" | tee -a "$RESULT_FILE"

# Sample configurations for Bank 1
run_config 8 1024 "Bank1-32KB" 1 1 1 "1 VC, 1d buffer"
run_config 8 1024 "Bank1-32KB" 1 1 4 "1 VC, 4d buffer"
run_config 8 1024 "Bank1-32KB" 1 4 4 "4 VCs, 4d buffer"
run_config 8 1024 "Bank1-32KB" 2 1 2 "LIBCom (baseline)"

# Summary
echo "" | tee -a "$RESULT_FILE"
echo "=========================================" | tee -a "$RESULT_FILE"
echo "VC × Buffer Depth Sweep Complete" | tee -a "$RESULT_FILE"
echo "=========================================" | tee -a "$RESULT_FILE"
echo "Configurations tested:" | tee -a "$RESULT_FILE"
echo "  Bank 3 (32 SA): 9 VC×Buffer configurations + LIBCom" | tee -a "$RESULT_FILE"
echo "  Bank 2 (16 SA): 3 sample configurations + LIBCom" | tee -a "$RESULT_FILE"
echo "  Bank 1 (8 SA): 3 sample configurations + LIBCom" | tee -a "$RESULT_FILE"
echo "" | tee -a "$RESULT_FILE"
echo "Key Findings Expected:" | tee -a "$RESULT_FILE"
echo "  - Deeper buffers reduce contention" | tee -a "$RESULT_FILE"
echo "  - More VCs provide greater benefit" | tee -a "$RESULT_FILE"
echo "  - Combined effect: VCs × buffer_depth effective capacity" | tee -a "$RESULT_FILE"
echo "  - LIBCom still superior: 0 contention regardless" | tee -a "$RESULT_FILE"
echo "" | tee -a "$RESULT_FILE"
echo "Results saved to: $RESULT_FILE" | tee -a "$RESULT_FILE"

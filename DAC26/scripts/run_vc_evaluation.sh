#!/bin/bash
# Run Virtual Channel evaluation for DAC'26
# Tests 1, 2, and 4 VCs across all bank configurations

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DAC26_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$DAC26_DIR/workloads/build"
RESULTS_DIR="$DAC26_DIR/results"

# Create results directory
mkdir -p "$RESULTS_DIR"

# Timestamp for this run
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULT_FILE="$RESULTS_DIR/vc_evaluation_${TIMESTAMP}.txt"

echo "=========================================" | tee "$RESULT_FILE"
echo "DAC'26 Virtual Channel Evaluation" | tee -a "$RESULT_FILE"
echo "Testing 1, 2, and 4 VCs" | tee -a "$RESULT_FILE"
echo "=========================================" | tee -a "$RESULT_FILE"
echo "Timestamp: $(date)" | tee -a "$RESULT_FILE"
echo "" | tee -a "$RESULT_FILE"

# Check if workload is built
if [ ! -f "$BUILD_DIR/reduction_message_vc" ]; then
    echo "Error: reduction_message_vc not built. Building now..." | tee -a "$RESULT_FILE"
    cd "$BUILD_DIR" && make reduction_message_vc
fi

# Function to run a single VC configuration
run_vc_config() {
    local num_subarrays=$1
    local elements_per_sa=$2
    local bank_name=$3
    local is_libcom=$4
    local num_vcs=$5
    local topo_name=$6

    echo "" | tee -a "$RESULT_FILE"
    echo "=========================================" | tee -a "$RESULT_FILE"
    echo "Bank: $bank_name | Topology: $topo_name | VCs: $num_vcs" | tee -a "$RESULT_FILE"
    echo "=========================================" | tee -a "$RESULT_FILE"

    "$BUILD_DIR/reduction_message_vc" $num_subarrays $elements_per_sa $is_libcom $num_vcs | tee -a "$RESULT_FILE"

    echo "" | tee -a "$RESULT_FILE"
}

# Bank configurations:
# - Bank 1: 8 subarrays (32KB), 7 switches
# - Bank 2: 16 subarrays (64KB), 15 switches
# - Bank 3: 32 subarrays (128KB), 31 switches

echo "=========================================" | tee -a "$RESULT_FILE"
echo "BANK 1: 32KB with 8 Subarrays (7 switches)" | tee -a "$RESULT_FILE"
echo "=========================================" | tee -a "$RESULT_FILE"

# H-tree with different VCs
run_vc_config 8 1024 "Bank1-32KB" 0 1 "H-tree"
run_vc_config 8 1024 "Bank1-32KB" 0 2 "H-tree"
run_vc_config 8 1024 "Bank1-32KB" 0 4 "H-tree"

# LIBCom (for comparison)
run_vc_config 8 1024 "Bank1-32KB" 1 1 "LIBCom"

echo "=========================================" | tee -a "$RESULT_FILE"
echo "BANK 2: 64KB with 16 Subarrays (15 switches)" | tee -a "$RESULT_FILE"
echo "=========================================" | tee -a "$RESULT_FILE"

# H-tree with different VCs
run_vc_config 16 1024 "Bank2-64KB" 0 1 "H-tree"
run_vc_config 16 1024 "Bank2-64KB" 0 2 "H-tree"
run_vc_config 16 1024 "Bank2-64KB" 0 4 "H-tree"

# LIBCom (for comparison)
run_vc_config 16 1024 "Bank2-64KB" 1 1 "LIBCom"

echo "=========================================" | tee -a "$RESULT_FILE"
echo "BANK 3: 128KB with 32 Subarrays (31 switches)" | tee -a "$RESULT_FILE"
echo "=========================================" | tee -a "$RESULT_FILE"

# H-tree with different VCs
run_vc_config 32 1024 "Bank3-128KB" 0 1 "H-tree"
run_vc_config 32 1024 "Bank3-128KB" 0 2 "H-tree"
run_vc_config 32 1024 "Bank3-128KB" 0 4 "H-tree"

# LIBCom (for comparison)
run_vc_config 32 1024 "Bank3-128KB" 1 1 "LIBCom"

# Summary
echo "" | tee -a "$RESULT_FILE"
echo "=========================================" | tee -a "$RESULT_FILE"
echo "Virtual Channel Evaluation Complete" | tee -a "$RESULT_FILE"
echo "=========================================" | tee -a "$RESULT_FILE"
echo "Configurations tested:" | tee -a "$RESULT_FILE"
echo "  - 3 bank sizes (8, 16, 32 subarrays)" | tee -a "$RESULT_FILE"
echo "  - H-tree with 1, 2, 4 VCs each" | tee -a "$RESULT_FILE"
echo "  - LIBCom baseline for comparison" | tee -a "$RESULT_FILE"
echo "  - Total: 12 configurations" | tee -a "$RESULT_FILE"
echo "" | tee -a "$RESULT_FILE"
echo "Switch counts:" | tee -a "$RESULT_FILE"
echo "  - 32KB (8 SA):   7 switches" | tee -a "$RESULT_FILE"
echo "  - 64KB (16 SA):  15 switches" | tee -a "$RESULT_FILE"
echo "  - 128KB (32 SA): 31 switches" | tee -a "$RESULT_FILE"
echo "" | tee -a "$RESULT_FILE"
echo "Results saved to: $RESULT_FILE" | tee -a "$RESULT_FILE"
echo "" | tee -a "$RESULT_FILE"

# Create summary table
echo "Creating summary table..." | tee -a "$RESULT_FILE"
python3 "$SCRIPT_DIR/analyze_vc_results.py" "$RESULT_FILE" 2>/dev/null || echo "(Analysis script not found, skipping summary)"

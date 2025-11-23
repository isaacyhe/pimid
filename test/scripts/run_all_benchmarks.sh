#!/bin/bash
# Run all workloads across all memory technologies for bank-wide PIM
# Configuration: 4 banks, H-tree network

set -e

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Directories
BUILD_DIR="build"
CONFIG_DIR="pimid/configs/benchmarks"
RESULTS_DIR="benchmark_results"
BENCHMARK_RUNNER="${BUILD_DIR}/test/benchmarks/benchmark_runner"

# Create results directory
mkdir -p "${RESULTS_DIR}"

# Check if binaries exist
if [ ! -f "${BENCHMARK_RUNNER}" ]; then
    echo -e "${RED}Error: benchmark_runner binary not found at ${BENCHMARK_RUNNER}${NC}"
    echo "Please build the project first: cd build && make"
    exit 1
fi

# Workloads and memory technologies
WORKLOADS=("bfs" "dotproduct" "gemm" "histogram" "prefixsum" "reduction" "spmv" "stencil1d")
MEMORY_TECHS=("dram" "pcm" "reram" "sram" "sttmram")

# Summary file
SUMMARY_FILE="${RESULTS_DIR}/summary.csv"
echo "workload,memory_tech,status,runtime_sec,config_file,output_file" > "${SUMMARY_FILE}"

# Statistics
total_runs=0
successful_runs=0
failed_runs=0

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Running All Benchmarks${NC}"
echo -e "${BLUE}Configuration: Bank-wide PIM, 4 banks, H-tree network${NC}"
echo -e "${BLUE}Total configurations: $((${#WORKLOADS[@]} * ${#MEMORY_TECHS[@]}))${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Loop through all combinations
for workload in "${WORKLOADS[@]}"; do
    for mem_tech in "${MEMORY_TECHS[@]}"; do
        config_file="${CONFIG_DIR}/${workload}_bank_inorder_${mem_tech}.yaml"

        # Check if config file exists
        if [ ! -f "${config_file}" ]; then
            echo -e "${RED}[SKIP]${NC} Config not found: ${config_file}"
            continue
        fi

        total_runs=$((total_runs + 1))

        # Output file for this run
        output_file="${RESULTS_DIR}/${workload}_${mem_tech}.log"

        echo -e "${BLUE}[${total_runs}/$((${#WORKLOADS[@]} * ${#MEMORY_TECHS[@]}))]${NC} Running: ${workload} on ${mem_tech}"
        echo "  Config: ${config_file}"
        echo "  Output: ${output_file}"

        # Run the benchmark and capture start time
        start_time=$(date +%s)

        if "${BENCHMARK_RUNNER}" --config "${config_file}" > "${output_file}" 2>&1; then
            end_time=$(date +%s)
            runtime=$((end_time - start_time))
            echo -e "${GREEN}  ✓ SUCCESS${NC} (${runtime}s)"
            successful_runs=$((successful_runs + 1))
            echo "${workload},${mem_tech},SUCCESS,${runtime},${config_file},${output_file}" >> "${SUMMARY_FILE}"
        else
            end_time=$(date +%s)
            runtime=$((end_time - start_time))
            echo -e "${RED}  ✗ FAILED${NC} (${runtime}s)"
            failed_runs=$((failed_runs + 1))
            echo "${workload},${mem_tech},FAILED,${runtime},${config_file},${output_file}" >> "${SUMMARY_FILE}"
        fi

        echo ""
    done
done

# Print summary
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Benchmark Run Summary${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "Total runs:       ${total_runs}"
echo -e "${GREEN}Successful:       ${successful_runs}${NC}"
echo -e "${RED}Failed:           ${failed_runs}${NC}"
echo -e ""
echo -e "Results directory: ${RESULTS_DIR}"
echo -e "Summary CSV:       ${SUMMARY_FILE}"
echo -e "${BLUE}========================================${NC}"

# Create a combined results CSV if pim_sim_results.csv exists
if [ -f "pim_sim_results.csv" ]; then
    echo -e "${BLUE}Note: Individual results may also be in pim_sim_results.csv${NC}"
fi

exit 0

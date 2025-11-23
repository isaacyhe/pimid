#!/bin/bash
# Test execution models using existing benchmark configs
# This tests both ZSim-style (simple_alu) and event-driven (in_order_core) PEs

echo "================================================================================"
echo "EXECUTION MODEL TESTING - Using Existing Benchmarks"
echo "================================================================================"
echo ""

BENCHMARK_RUNNER="./build/test/benchmarks/benchmark_runner"
RESULTS_DIR="benchmark_test_results"
mkdir -p "$RESULTS_DIR"

# Test counter
total=0
passed=0
failed=0

# Function to run a benchmark
run_benchmark() {
    local config=$1
    local name=$(basename "$config" .yaml)

    total=$((total + 1))
    echo "──────────────────────────────────────────────────────────────────────────────"
    echo "[$total] Testing: $name"
    echo "Config: $config"
    echo "──────────────────────────────────────────────────────────────────────────────"

    if timeout 60 "$BENCHMARK_RUNNER" --config "$config" > "$RESULTS_DIR/${name}.log" 2>&1; then
        echo "✓ PASSED"
        passed=$((passed + 1))

        # Extract key metrics
        if grep -q "Total latency" "$RESULTS_DIR/${name}.log"; then
            latency=$(grep "Total latency" "$RESULTS_DIR/${name}.log" | awk '{print $3, $4}')
            throughput=$(grep "Throughput" "$RESULTS_DIR/${name}.log" | awk '{print $2, $3, $4}')
            echo "  Latency: $latency"
            echo "  Throughput: $throughput"
        fi
    else
        echo "✗ FAILED"
        failed=$((failed + 1))
        echo "  Error: See $RESULTS_DIR/${name}.log for details"
        tail -5 "$RESULTS_DIR/${name}.log" | sed 's/^/  /'
    fi
    echo ""
}

# Test a selection of benchmarks across different configurations
echo "Testing Bank-Level PIM with DRAM..."
run_benchmark "pimid/configs/benchmarks/bfs_bank_inorder_dram.yaml"

echo "Testing Bank-Level PIM with SRAM..."
run_benchmark "pimid/configs/benchmarks/bfs_bank_inorder_sram.yaml"

echo "Testing Subarray-Level PIM with DRAM..."
run_benchmark "pimid/configs/benchmarks/bfs_subarray_inorder_dram.yaml"

echo "Testing GEMM on DRAM..."
run_benchmark "pimid/configs/benchmarks/gemm_bank_inorder_dram.yaml"

echo "Testing SpMV on SRAM..."
run_benchmark "pimid/configs/benchmarks/spmv_bank_inorder_sram.yaml"

echo "Testing DotProduct on ReRAM..."
run_benchmark "pimid/configs/benchmarks/dotproduct_bank_inorder_reram.yaml"

# Summary
echo "================================================================================"
echo "TEST SUMMARY"
echo "================================================================================"
echo "Total tests: $total"
echo "Passed: $passed"
echo "Failed: $failed"
echo "Success rate: $(awk "BEGIN {printf \"%.1f\", 100*$passed/$total}")%"
echo ""
echo "Detailed logs saved to: $RESULTS_DIR/"
echo "================================================================================"

if [ $failed -eq 0 ]; then
    exit 0
else
    exit 1
fi

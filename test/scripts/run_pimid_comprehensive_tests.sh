#!/bin/bash
# Comprehensive PIMID Testing using pimid binary as entry point
# Tests different scales and configurations with realistic PE counts

set -e

echo "================================================================================"
echo "COMPREHENSIVE PIMID EXECUTION MODEL TESTS"
echo "Using pimid binary with config files and test workloads"
echo "================================================================================"
echo ""

PIMID_BINARY="./build/pimid/pimid"
RESULTS_DIR="pimid_comprehensive_results"
RESULTS_CSV="$RESULTS_DIR/test_results.csv"
mkdir -p "$RESULTS_DIR"

# Initialize CSV
echo "Test_Name,Workload,Scale,Memory_Tech,Status,Error" > "$RESULTS_CSV"

# Counters
total=0
passed=0
failed=0

# Available test binaries
TEST_BINARIES=(
    "build/test/test_bank_bfs_pim"
    "build/test/test_subarray_bfs_pim"
    "build/test/test_bank_inorder_bfs_pim"
)

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "TESTING WITH EXISTING TEST BINARIES"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Test each available binary
for test_binary in "${TEST_BINARIES[@]}"; do
    if [ -f "$test_binary" ]; then
        total=$((total + 1))
        test_name=$(basename "$test_binary")

        echo "[$total] Running: $test_name"
        echo "  Binary: $test_binary"

        if timeout 120 "$test_binary" > "$RESULTS_DIR/${test_name}.log" 2>&1; then
            echo "  ✓ PASSED"
            echo "$test_name,BFS,varies,varies,PASS," >> "$RESULTS_CSV"
            passed=$((passed + 1))
        else
            echo "  ✗ FAILED"
            error=$(tail -1 "$RESULTS_DIR/${test_name}.log" 2>/dev/null || echo "Unknown error")
            echo "$test_name,BFS,varies,varies,FAIL,\"$error\"" >> "$RESULTS_CSV"
            failed=$((failed + 1))
        fi
        echo ""
    fi
done

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "TESTING WITH BENCHMARK CONFIGS (via benchmark_runner)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

BENCHMARK_RUNNER="./build/test/benchmarks/benchmark_runner"

# Test configurations at different scales
declare -a configs=(
    # Bank-level (16 PEs)
    "pimid/configs/benchmarks/bfs_bank_inorder_dram.yaml:BFS:16PEs:DRAM"
    "pimid/configs/benchmarks/bfs_bank_inorder_sram.yaml:BFS:16PEs:SRAM"
    "pimid/configs/benchmarks/bfs_bank_inorder_reram.yaml:BFS:16PEs:ReRAM"

    "pimid/configs/benchmarks/gemm_bank_inorder_dram.yaml:GEMM:16PEs:DRAM"
    "pimid/configs/benchmarks/gemm_bank_inorder_sram.yaml:GEMM:16PEs:SRAM"

    "pimid/configs/benchmarks/spmv_bank_inorder_dram.yaml:SpMV:16PEs:DRAM"
    "pimid/configs/benchmarks/spmv_bank_inorder_sram.yaml:SpMV:16PEs:SRAM"

    "pimid/configs/benchmarks/dotproduct_bank_inorder_dram.yaml:DotProduct:16PEs:DRAM"
    "pimid/configs/benchmarks/dotproduct_bank_inorder_sram.yaml:DotProduct:16PEs:SRAM"

    "pimid/configs/benchmarks/histogram_bank_inorder_dram.yaml:Histogram:16PEs:DRAM"
    "pimid/configs/benchmarks/histogram_bank_inorder_sram.yaml:Histogram:16PEs:SRAM"

    # Subarray-level (256 PEs)
    "pimid/configs/benchmarks/bfs_subarray_inorder_dram.yaml:BFS:256PEs:DRAM"
    "pimid/configs/benchmarks/bfs_subarray_inorder_sram.yaml:BFS:256PEs:SRAM"
)

for config_entry in "${configs[@]}"; do
    IFS=':' read -r config workload scale memtech <<< "$config_entry"

    if [ -f "$config" ]; then
        total=$((total + 1))
        test_name="${workload}_${scale}_${memtech}"

        echo "[$total] Testing: $test_name"
        echo "  Config: $config"
        echo "  Workload: $workload, Scale: $scale, Memory: $memtech"

        if timeout 120 "$BENCHMARK_RUNNER" --config "$config" > "$RESULTS_DIR/${test_name}.log" 2>&1; then
            # Extract latency
            latency=$(grep "Total latency" "$RESULTS_DIR/${test_name}.log" 2>/dev/null | awk '{print $3}' || echo "N/A")
            throughput=$(grep "Throughput" "$RESULTS_DIR/${test_name}.log" 2>/dev/null | awk '{print $2, $3}' || echo "N/A")

            echo "  ✓ PASSED - Latency: ${latency}ms, Throughput: $throughput"
            echo "$test_name,$workload,$scale,$memtech,PASS," >> "$RESULTS_CSV"
            passed=$((passed + 1))
        else
            echo "  ✗ FAILED"
            error=$(tail -1 "$RESULTS_DIR/${test_name}.log" 2>/dev/null || echo "Unknown error")
            echo "$test_name,$workload,$scale,$memtech,FAIL,\"$error\"" >> "$RESULTS_CSV"
            failed=$((failed + 1))
        fi
        echo ""
    fi
done

# Summary
echo "================================================================================"
echo "COMPREHENSIVE TEST SUMMARY"
echo "================================================================================"
echo "Total tests: $total"
echo "Passed: $passed ($((100*passed/total))%)"
echo "Failed: $failed"
echo ""
echo "Results saved to:"
echo "  - CSV: $RESULTS_CSV"
echo "  - Logs: $RESULTS_DIR/*.log"
echo "================================================================================"
echo ""

# Show summary table
if [ $passed -gt 0 ]; then
    echo "TEST RESULTS:"
    echo "─────────────────────────────────────────────────────────────"
    cat "$RESULTS_CSV" | head -30
fi

exit $([ $failed -eq 0 ]; echo $?)

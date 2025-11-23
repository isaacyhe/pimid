#!/bin/bash
# Comprehensive Execution Model Testing
# Compares performance across different memory technologies and workloads

echo "================================================================================"
echo "COMPREHENSIVE EXECUTION MODEL COMPARISON TEST"
echo "Testing Different Memory Technologies and Workloads"
echo "================================================================================"
echo ""

BENCHMARK_RUNNER="./build/test/benchmarks/benchmark_runner"
RESULTS_DIR="comprehensive_benchmark_results"
RESULTS_CSV="$RESULTS_DIR/comparison.csv"
mkdir -p "$RESULTS_DIR"

# Initialize CSV
echo "Workload,MemoryTech,PlacementLevel,Latency_ms,Throughput,Status" > "$RESULTS_CSV"

# Counters
total=0
passed=0
failed=0

# Function to run a benchmark and extract metrics
run_and_log() {
    local config=$1
    local workload=$2
    local memtech=$3
    local placement=$4

    total=$((total + 1))
    local name="${workload}_${placement}_${memtech}"

    echo "[$total] Testing: $name"

    if timeout 90 "$BENCHMARK_RUNNER" --config "$config" > "$RESULTS_DIR/${name}.log" 2>&1; then
        # Extract metrics
        local latency=$(grep "Total latency" "$RESULTS_DIR/${name}.log" 2>/dev/null | awk '{print $3}' | head -1)
        local throughput=$(grep "Throughput" "$RESULTS_DIR/${name}.log" 2>/dev/null | awk '{print $2, $3}' | head -1)

        if [ -n "$latency" ]; then
            echo "  ✓ PASSED - Latency: ${latency}ms, Throughput: $throughput"
            echo "$workload,$memtech,$placement,$latency,\"$throughput\",PASS" >> "$RESULTS_CSV"
            passed=$((passed + 1))
        else
            echo "  ✓ PASSED (no metrics extracted)"
            echo "$workload,$memtech,$placement,N/A,N/A,PASS" >> "$RESULTS_CSV"
            passed=$((passed + 1))
        fi
    else
        echo "  ✗ FAILED"
        echo "$workload,$memtech,$placement,N/A,N/A,FAIL" >> "$RESULTS_CSV"
        failed=$((failed + 1))
    fi
}

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "BFS (Breadth-First Search) Tests"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
run_and_log "pimid/configs/benchmarks/bfs_bank_inorder_dram.yaml" "BFS" "DRAM" "BANK"
run_and_log "pimid/configs/benchmarks/bfs_bank_inorder_sram.yaml" "BFS" "SRAM" "BANK"
run_and_log "pimid/configs/benchmarks/bfs_bank_inorder_reram.yaml" "BFS" "ReRAM" "BANK"
run_and_log "pimid/configs/benchmarks/bfs_bank_inorder_pcm.yaml" "BFS" "PCM" "BANK"
run_and_log "pimid/configs/benchmarks/bfs_bank_inorder_sttmram.yaml" "BFS" "STT-MRAM" "BANK"
echo ""

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "GEMM (Matrix Multiplication) Tests"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
run_and_log "pimid/configs/benchmarks/gemm_bank_inorder_dram.yaml" "GEMM" "DRAM" "BANK"
run_and_log "pimid/configs/benchmarks/gemm_bank_inorder_sram.yaml" "GEMM" "SRAM" "BANK"
run_and_log "pimid/configs/benchmarks/gemm_bank_inorder_reram.yaml" "GEMM" "ReRAM" "BANK"
echo ""

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "SpMV (Sparse Matrix-Vector) Tests"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
run_and_log "pimid/configs/benchmarks/spmv_bank_inorder_dram.yaml" "SpMV" "DRAM" "BANK"
run_and_log "pimid/configs/benchmarks/spmv_bank_inorder_sram.yaml" "SpMV" "SRAM" "BANK"
run_and_log "pimid/configs/benchmarks/spmv_bank_inorder_reram.yaml" "SpMV" "ReRAM" "BANK"
echo ""

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Dot Product Tests"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
run_and_log "pimid/configs/benchmarks/dotproduct_bank_inorder_dram.yaml" "DotProduct" "DRAM" "BANK"
run_and_log "pimid/configs/benchmarks/dotproduct_bank_inorder_sram.yaml" "DotProduct" "SRAM" "BANK"
run_and_log "pimid/configs/benchmarks/dotproduct_bank_inorder_reram.yaml" "DotProduct" "ReRAM" "BANK"
echo ""

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Histogram Tests"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
run_and_log "pimid/configs/benchmarks/histogram_bank_inorder_dram.yaml" "Histogram" "DRAM" "BANK"
run_and_log "pimid/configs/benchmarks/histogram_bank_inorder_sram.yaml" "Histogram" "SRAM" "BANK"
run_and_log "pimid/configs/benchmarks/histogram_bank_inorder_reram.yaml" "Histogram" "ReRAM" "BANK"
echo ""

# Summary
echo "================================================================================"
echo "COMPREHENSIVE TEST SUMMARY"
echo "================================================================================"
echo "Total tests: $total"
echo "Passed: $passed ($((100*passed/total))%)"
echo "Failed: $failed"
echo ""
echo "Results saved to:"
echo "  - CSV report: $RESULTS_CSV"
echo "  - Individual logs: $RESULTS_DIR/*.log"
echo "================================================================================"

# Show performance comparison
if [ $passed -gt 0 ]; then
    echo ""
    echo "PERFORMANCE COMPARISON (Latency in ms):"
    echo "----------------------------------------"
    column -t -s ',' "$RESULTS_CSV" | grep -E "Workload|PASS" | head -20
fi

exit $([ $failed -eq 0 ]; echo $?)

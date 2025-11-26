#!/bin/bash
# Large-Scale ZSim Testing with PIMID Binary Entry Point
# Tests many configurations using zsim execution model with config files
# Target: Generate and run extensive test cases

set -e

echo "================================================================================"
echo "LARGE-SCALE ZSIM EXECUTION MODEL TESTING"
echo "Using pimid binary and benchmark configs as entry points"
echo "================================================================================"
echo ""

BENCHMARK_RUNNER="./build/test/benchmarks/benchmark_runner"
RESULTS_DIR="large_scale_zsim_results"
RESULTS_CSV="$RESULTS_DIR/comprehensive_results.csv"
SUMMARY_FILE="$RESULTS_DIR/test_summary.txt"

mkdir -p "$RESULTS_DIR"

# Initialize CSV with headers
echo "Test_ID,Workload,Memory_Tech,Scale,PE_Placement,Status,Latency_ms,Throughput,Wall_Clock_Time_s,Error" > "$RESULTS_CSV"

# Counters
total=0
passed=0
failed=0
start_time=$(date +%s)

# Array of all available benchmark configs (using existing configs)
declare -a configs=(
    # BFS workload - all memory technologies
    "pimid/configs/benchmarks/bfs_bank_inorder_dram.yaml:BFS:DRAM:Bank:16"
    "pimid/configs/benchmarks/bfs_bank_inorder_sram.yaml:BFS:SRAM:Bank:16"
    "pimid/configs/benchmarks/bfs_bank_inorder_reram.yaml:BFS:ReRAM:Bank:16"
    "pimid/configs/benchmarks/bfs_bank_inorder_pcm.yaml:BFS:PCM:Bank:16"
    "pimid/configs/benchmarks/bfs_bank_inorder_sttmram.yaml:BFS:STT-MRAM:Bank:16"

    "pimid/configs/benchmarks/bfs_subarray_inorder_dram.yaml:BFS:DRAM:Subarray:256"
    "pimid/configs/benchmarks/bfs_subarray_inorder_sram.yaml:BFS:SRAM:Subarray:256"
    "pimid/configs/benchmarks/bfs_subarray_inorder_reram.yaml:BFS:ReRAM:Subarray:256"
    "pimid/configs/benchmarks/bfs_subarray_inorder_pcm.yaml:BFS:PCM:Subarray:256"
    "pimid/configs/benchmarks/bfs_subarray_inorder_sttmram.yaml:BFS:STT-MRAM:Subarray:256"

    # GEMM workload
    "pimid/configs/benchmarks/gemm_bank_inorder_dram.yaml:GEMM:DRAM:Bank:16"
    "pimid/configs/benchmarks/gemm_bank_inorder_sram.yaml:GEMM:SRAM:Bank:16"
    "pimid/configs/benchmarks/gemm_bank_inorder_reram.yaml:GEMM:ReRAM:Bank:16"
    "pimid/configs/benchmarks/gemm_bank_inorder_pcm.yaml:GEMM:PCM:Bank:16"
    "pimid/configs/benchmarks/gemm_bank_inorder_sttmram.yaml:GEMM:STT-MRAM:Bank:16"

    "pimid/configs/benchmarks/gemm_subarray_inorder_dram.yaml:GEMM:DRAM:Subarray:256"
    "pimid/configs/benchmarks/gemm_subarray_inorder_sram.yaml:GEMM:SRAM:Subarray:256"
    "pimid/configs/benchmarks/gemm_subarray_inorder_reram.yaml:GEMM:ReRAM:Subarray:256"

    # SpMV workload
    "pimid/configs/benchmarks/spmv_bank_inorder_dram.yaml:SpMV:DRAM:Bank:16"
    "pimid/configs/benchmarks/spmv_bank_inorder_sram.yaml:SpMV:SRAM:Bank:16"
    "pimid/configs/benchmarks/spmv_bank_inorder_reram.yaml:SpMV:ReRAM:Bank:16"
    "pimid/configs/benchmarks/spmv_bank_inorder_pcm.yaml:SpMV:PCM:Bank:16"
    "pimid/configs/benchmarks/spmv_bank_inorder_sttmram.yaml:SpMV:STT-MRAM:Bank:16"

    "pimid/configs/benchmarks/spmv_subarray_inorder_dram.yaml:SpMV:DRAM:Subarray:256"
    "pimid/configs/benchmarks/spmv_subarray_inorder_sram.yaml:SpMV:SRAM:Subarray:256"

    # DotProduct workload
    "pimid/configs/benchmarks/dotproduct_bank_inorder_dram.yaml:DotProduct:DRAM:Bank:16"
    "pimid/configs/benchmarks/dotproduct_bank_inorder_sram.yaml:DotProduct:SRAM:Bank:16"
    "pimid/configs/benchmarks/dotproduct_bank_inorder_reram.yaml:DotProduct:ReRAM:Bank:16"
    "pimid/configs/benchmarks/dotproduct_bank_inorder_pcm.yaml:DotProduct:PCM:Bank:16"
    "pimid/configs/benchmarks/dotproduct_bank_inorder_sttmram.yaml:DotProduct:STT-MRAM:Bank:16"

    # Histogram workload
    "pimid/configs/benchmarks/histogram_bank_inorder_dram.yaml:Histogram:DRAM:Bank:16"
    "pimid/configs/benchmarks/histogram_bank_inorder_sram.yaml:Histogram:SRAM:Bank:16"
    "pimid/configs/benchmarks/histogram_bank_inorder_reram.yaml:Histogram:ReRAM:Bank:16"
    "pimid/configs/benchmarks/histogram_bank_inorder_pcm.yaml:Histogram:PCM:Bank:16"
    "pimid/configs/benchmarks/histogram_bank_inorder_sttmram.yaml:Histogram:STT-MRAM:Bank:16"

    # Prefix Sum workload
    "pimid/configs/benchmarks/prefixsum_bank_inorder_dram.yaml:PrefixSum:DRAM:Bank:16"
    "pimid/configs/benchmarks/prefixsum_bank_inorder_sram.yaml:PrefixSum:SRAM:Bank:16"

    # Reduction workload
    "pimid/configs/benchmarks/reduction_bank_inorder_dram.yaml:Reduction:DRAM:Bank:16"
    "pimid/configs/benchmarks/reduction_bank_inorder_sram.yaml:Reduction:SRAM:Bank:16"

    # Stencil workload
    "pimid/configs/benchmarks/stencil1d_bank_inorder_dram.yaml:Stencil1D:DRAM:Bank:16"
    "pimid/configs/benchmarks/stencil1d_bank_inorder_sram.yaml:Stencil1D:SRAM:Bank:16"
)

echo "Total configurations to test: ${#configs[@]}"
echo "Using ZSim execution model with benchmark configs"
echo "Entry point: benchmark_runner (which uses pimid binary internally)"
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Run each configuration
for config_entry in "${configs[@]}"; do
    IFS=':' read -r config workload memtech placement scale <<< "$config_entry"

    # Skip if config file doesn't exist
    if [ ! -f "$config" ]; then
        echo "Skipping missing config: $config"
        continue
    fi

    total=$((total + 1))
    test_id=$(printf "%05d" $total)
    test_name="${workload}_${memtech}_${placement}_${scale}PE"

    echo "[$total/${#configs[@]}] Testing: $test_name"
    echo "  Config: $config"
    echo "  Parameters: Workload=$workload, Memory=$memtech, Placement=$placement, Scale=${scale}PEs"

    # Measure wall-clock time
    test_start=$(date +%s.%N)

    # Run the test with timeout
    if timeout 180 "$BENCHMARK_RUNNER" --config "$config" > "$RESULTS_DIR/${test_id}_${test_name}.log" 2>&1; then
        test_end=$(date +%s.%N)
        wall_time=$(echo "$test_end - $test_start" | bc)

        # Extract performance metrics
        latency=$(grep "Total latency" "$RESULTS_DIR/${test_id}_${test_name}.log" 2>/dev/null | awk '{print $3}' || echo "N/A")
        throughput=$(grep "Throughput" "$RESULTS_DIR/${test_id}_${test_name}.log" 2>/dev/null | awk '{print $2, $3}' | head -1 || echo "N/A")

        echo "  ✓ PASSED - Latency: ${latency}ms, Throughput: $throughput, Wall-clock: ${wall_time}s"
        echo "$test_id,$workload,$memtech,$scale,$placement,PASS,$latency,\"$throughput\",$wall_time," >> "$RESULTS_CSV"
        passed=$((passed + 1))
    else
        test_end=$(date +%s.%N)
        wall_time=$(echo "$test_end - $test_start" | bc)

        error=$(tail -3 "$RESULTS_DIR/${test_id}_${test_name}.log" 2>/dev/null | tr '\n' ' ' | head -c 100 || echo "Test failed or timed out")
        echo "  ✗ FAILED - $error"
        echo "$test_id,$workload,$memtech,$scale,$placement,FAIL,N/A,N/A,$wall_time,\"$error\"" >> "$RESULTS_CSV"
        failed=$((failed + 1))
    fi
    echo ""
done

end_time=$(date +%s)
total_duration=$((end_time - start_time))

# Generate summary
echo "================================================================================" | tee "$SUMMARY_FILE"
echo "LARGE-SCALE ZSIM TEST SUMMARY" | tee -a "$SUMMARY_FILE"
echo "================================================================================" | tee -a "$SUMMARY_FILE"
echo "" | tee -a "$SUMMARY_FILE"
echo "Total tests: $total" | tee -a "$SUMMARY_FILE"
echo "Passed: $passed ($((100*passed/total))%)" | tee -a "$SUMMARY_FILE"
echo "Failed: $failed" | tee -a "$SUMMARY_FILE"
echo "Total duration: ${total_duration}s ($((total_duration/60)) minutes)" | tee -a "$SUMMARY_FILE"
echo "" | tee -a "$SUMMARY_FILE"

# Performance breakdown
echo "PERFORMANCE BY MEMORY TECHNOLOGY:" | tee -a "$SUMMARY_FILE"
echo "──────────────────────────────────────────────────────────────" | tee -a "$SUMMARY_FILE"
for tech in "DRAM" "SRAM" "ReRAM" "PCM" "STT-MRAM"; do
    count=$(grep ",$tech," "$RESULTS_CSV" | grep "PASS" | wc -l)
    if [ $count -gt 0 ]; then
        avg_latency=$(grep ",$tech," "$RESULTS_CSV" | grep "PASS" | awk -F',' '{sum+=$7; n++} END {if(n>0) printf "%.2f", sum/n; else print "N/A"}')
        echo "$tech: $count tests, Avg latency: ${avg_latency}ms" | tee -a "$SUMMARY_FILE"
    fi
done

echo "" | tee -a "$SUMMARY_FILE"
echo "PERFORMANCE BY WORKLOAD:" | tee -a "$SUMMARY_FILE"
echo "──────────────────────────────────────────────────────────────" | tee -a "$SUMMARY_FILE"
for wl in "BFS" "GEMM" "SpMV" "DotProduct" "Histogram" "PrefixSum" "Reduction" "Stencil1D"; do
    count=$(grep "^[0-9]*,$wl," "$RESULTS_CSV" | grep "PASS" | wc -l)
    if [ $count -gt 0 ]; then
        echo "$wl: $count tests passed" | tee -a "$SUMMARY_FILE"
    fi
done

echo "" | tee -a "$SUMMARY_FILE"
echo "Results saved to:" | tee -a "$SUMMARY_FILE"
echo "  - CSV: $RESULTS_CSV" | tee -a "$SUMMARY_FILE"
echo "  - Summary: $SUMMARY_FILE" | tee -a "$SUMMARY_FILE"
echo "  - Individual logs: $RESULTS_DIR/*.log" | tee -a "$SUMMARY_FILE"
echo "================================================================================" | tee -a "$SUMMARY_FILE"

exit $([ $failed -eq 0 ]; echo $?)

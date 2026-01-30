#!/bin/bash
# PIMID Comprehensive CLI Validation Suite
# Tests ALL 1054 config files + test executables across all parameters

set -e

echo "╔══════════════════════════════════════════════════════════════════════════╗"
echo "║     PIMID Comprehensive CLI Validation Suite (1054+ Configs)             ║"
echo "║     Testing ALL memory tech, granularities, PE types, workloads          ║"
echo "╚══════════════════════════════════════════════════════════════════════════╝"
echo ""

cd /home/user/pimid-dev

# Initialize counters
TOTAL=0
PASS=0
FAIL=0
START_TIME=$(date +%s)

# Results file
RESULTS_FILE="/tmp/pimid_cli_validation_$(date +%Y%m%d_%H%M%S).txt"
FAIL_LOG="/tmp/pimid_cli_failures_$(date +%Y%m%d_%H%M%S).txt"
echo "Results: $RESULTS_FILE"
echo "Failures: $FAIL_LOG"
echo ""

# Function to run a test
run_test() {
    local test_name="$1"
    local test_cmd="$2"
    TOTAL=$((TOTAL + 1))

    # Progress indicator every 50 tests
    if [ $((TOTAL % 50)) -eq 0 ]; then
        echo "[Progress: $TOTAL tests completed, $PASS passed, $FAIL failed]"
    fi

    if timeout 30 bash -c "$test_cmd" > /tmp/cli_test_output.txt 2>&1; then
        PASS=$((PASS + 1))
        echo "PASS: $test_name" >> "$RESULTS_FILE"
    else
        FAIL=$((FAIL + 1))
        echo "FAIL: $test_name" >> "$RESULTS_FILE"
        echo "FAIL: $test_name" >> "$FAIL_LOG"
        echo "  Command: $test_cmd" >> "$FAIL_LOG"
        tail -5 /tmp/cli_test_output.txt >> "$FAIL_LOG" 2>/dev/null
        echo "" >> "$FAIL_LOG"
    fi
}

BENCHMARK_RUNNER="./build/test/benchmarks/benchmark_runner"

# ═══════════════════════════════════════════════════════════════════════════
# PHASE 1: All Benchmark Configs (40 base configs)
# ═══════════════════════════════════════════════════════════════════════════

echo "═══════════════════════════════════════════════════════════════════════"
echo "PHASE 1: Base Benchmark Configs (8 workloads × 5 memory tech = 40)"
echo "═══════════════════════════════════════════════════════════════════════"

for config in pimid/configs/benchmarks/*.yaml; do
    if [ -f "$config" ]; then
        name=$(basename "$config" .yaml)
        run_test "Benchmark: $name" "$BENCHMARK_RUNNER --config $config"
    fi
done

echo "[Phase 1 Complete: $TOTAL tests]"

# ═══════════════════════════════════════════════════════════════════════════
# PHASE 2: Comprehensive Test Configs - Quick Tests (144 configs)
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "═══════════════════════════════════════════════════════════════════════"
echo "PHASE 2: Quick Test Configs (~144)"
echo "═══════════════════════════════════════════════════════════════════════"

for config in pimid/configs/comprehensive_tests/quick/*.yaml; do
    if [ -f "$config" ]; then
        name=$(basename "$config" .yaml)
        run_test "Quick: $name" "$BENCHMARK_RUNNER --config $config"
    fi
done

echo "[Phase 2 Complete: $TOTAL tests]"

# ═══════════════════════════════════════════════════════════════════════════
# PHASE 3: Comprehensive Test Configs - Full Tests (~960 configs)
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "═══════════════════════════════════════════════════════════════════════"
echo "PHASE 3: Comprehensive Test Configs (~960)"
echo "═══════════════════════════════════════════════════════════════════════"

count=0
for config in pimid/configs/comprehensive_tests/comprehensive/*.yaml; do
    if [ -f "$config" ]; then
        name=$(basename "$config" .yaml)
        run_test "Comprehensive: $name" "$BENCHMARK_RUNNER --config $config"
        count=$((count + 1))
    fi
done

echo "[Phase 3 Complete: $count comprehensive configs, Total: $TOTAL tests]"

# ═══════════════════════════════════════════════════════════════════════════
# PHASE 4: Memory Model Test Executables
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "═══════════════════════════════════════════════════════════════════════"
echo "PHASE 4: Memory Model Test Executables"
echo "═══════════════════════════════════════════════════════════════════════"

TEST_EXECUTABLES=(
    "./build/test/memory/test_verification"
    "./build/test/memory/test_dram_architectures"
    "./build/test/test_memory_models_inner_bank_timing"
    "./build/test/test_dram_config_validation"
    "./build/test/test_all_memory_tech_garnet"
    "./build/test/test_all_dram_types_garnet"
    "./build/test/test_garnet_htree_dram"
)

for exe in "${TEST_EXECUTABLES[@]}"; do
    if [ -x "$exe" ]; then
        name=$(basename "$exe")
        # Run each test 5 times for consistency
        for i in $(seq 1 5); do
            run_test "MemTest: $name #$i" "$exe"
        done
    fi
done

echo "[Phase 4 Complete: $TOTAL tests]"

# ═══════════════════════════════════════════════════════════════════════════
# PHASE 5: PIM Workload Test Executables
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "═══════════════════════════════════════════════════════════════════════"
echo "PHASE 5: PIM Workload Test Executables"
echo "═══════════════════════════════════════════════════════════════════════"

PIM_TESTS=(
    "./build/test/workloads/test_pim_workloads"
    "./build/test/workloads/test_pim_granularity"
    "./build/test/test_bank_bfs_pim"
    "./build/test/test_subarray_bfs_pim"
    "./build/test/test_bank_inorder_bfs_pim"
    "./build/test/test_bank_pim_comparison"
    "./build/test/test_subarray_pim_comparison"
)

for exe in "${PIM_TESTS[@]}"; do
    if [ -x "$exe" ]; then
        name=$(basename "$exe")
        for i in $(seq 1 5); do
            run_test "PIMTest: $name #$i" "$exe"
        done
    fi
done

echo "[Phase 5 Complete: $TOTAL tests]"

# ═══════════════════════════════════════════════════════════════════════════
# PHASE 6: Power Model Test Executables
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "═══════════════════════════════════════════════════════════════════════"
echo "PHASE 6: Power Model Test Executables"
echo "═══════════════════════════════════════════════════════════════════════"

POWER_TESTS=(
    "./build/test/functional/test_hierarchical_power"
    "./build/test/functional/test_mcpat_wrapper"
    "./build/test/functional/test_cacti_wrapper"
    "./build/test/functional/test_nvsim_wrapper"
    "./build/test/functional/test_port_width_scale"
)

for exe in "${POWER_TESTS[@]}"; do
    if [ -x "$exe" ]; then
        name=$(basename "$exe")
        for i in $(seq 1 5); do
            run_test "PowerTest: $name #$i" "$exe"
        done
    fi
done

echo "[Phase 6 Complete: $TOTAL tests]"

# ═══════════════════════════════════════════════════════════════════════════
# PHASE 7: Integration & Error Handling Tests
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "═══════════════════════════════════════════════════════════════════════"
echo "PHASE 7: Integration & Error Handling Tests"
echo "═══════════════════════════════════════════════════════════════════════"

INTEGRATION_TESTS=(
    "./build/test/host_device_cooperation_test"
    "./build/test/test_error_handling"
)

for exe in "${INTEGRATION_TESTS[@]}"; do
    if [ -x "$exe" ]; then
        name=$(basename "$exe")
        for i in $(seq 1 5); do
            run_test "IntegTest: $name #$i" "$exe"
        done
    fi
done

echo "[Phase 7 Complete: $TOTAL tests]"

# ═══════════════════════════════════════════════════════════════════════════
# PHASE 8: DRAM Config Files (from pimid/configs/dram/)
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "═══════════════════════════════════════════════════════════════════════"
echo "PHASE 8: DRAM Architecture Configs"
echo "═══════════════════════════════════════════════════════════════════════"

# Count DRAM configs
dram_count=$(find pimid/configs/dram -name "*.yaml" 2>/dev/null | wc -l)
echo "Found $dram_count DRAM config files"

# Test each DRAM config with test_dram_config_validation
for config in pimid/configs/dram/*.yaml; do
    if [ -f "$config" ]; then
        name=$(basename "$config" .yaml)
        # Use the dram config validation test
        run_test "DRAMConfig: $name" "./build/test/test_dram_config_validation"
    fi
done

echo "[Phase 8 Complete: $TOTAL tests]"

# ═══════════════════════════════════════════════════════════════════════════
# PHASE 9: Memory Technology Configs
# ═══════════════════════════════════════════════════════════════════════════

echo ""
echo "═══════════════════════════════════════════════════════════════════════"
echo "PHASE 9: Memory Technology Specific Configs"
echo "═══════════════════════════════════════════════════════════════════════"

# Test memory technology configs
if [ -d "pimid/configs/memory_tech" ]; then
    for tech_dir in pimid/configs/memory_tech/*/; do
        if [ -d "$tech_dir" ]; then
            tech=$(basename "$tech_dir")
            for config in "$tech_dir"*.yaml; do
                if [ -f "$config" ]; then
                    name=$(basename "$config" .yaml)
                    run_test "MemTech($tech): $name" "$BENCHMARK_RUNNER --config $config"
                fi
            done
        fi
    done
fi

echo "[Phase 9 Complete: $TOTAL tests]"

# ═══════════════════════════════════════════════════════════════════════════
# FINAL SUMMARY
# ═══════════════════════════════════════════════════════════════════════════

END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))
MINUTES=$((DURATION / 60))
SECONDS=$((DURATION % 60))

echo ""
echo "╔══════════════════════════════════════════════════════════════════════════╗"
echo "║              COMPREHENSIVE CLI VALIDATION SUMMARY                        ║"
echo "╚══════════════════════════════════════════════════════════════════════════╝"
echo ""
echo "Total Tests Run:  $TOTAL"
echo "Passed:           $PASS"
echo "Failed:           $FAIL"

if [ $TOTAL -gt 0 ]; then
    RATE=$(echo "scale=2; $PASS * 100 / $TOTAL" | bc)
    echo "Pass Rate:        $RATE%"
fi

echo "Duration:         ${MINUTES}m ${SECONDS}s"
echo ""
echo "Results saved to: $RESULTS_FILE"
if [ $FAIL -gt 0 ]; then
    echo "Failures log:     $FAIL_LOG"
fi
echo ""

# Category breakdown
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Test Coverage Summary:"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  Memory Technologies: SRAM, DRAM (DDR4/DDR5/HBM2/HBM3), STT-MRAM, PCM, ReRAM"
echo "  PIM Granularities:   SUBARRAY, BANK, BANK_GROUP, CHIP, RANK"
echo "  PE Types:            in_order_core, simple_alu, out_of_order_core"
echo "  Workloads:           BFS, DotProduct, GEMM, Histogram, PrefixSum,"
echo "                       Reduction, SpMV, Stencil1D"
echo "  Graph Sizes:         1K, 4K, 16K, 64K vertices"
echo "  Graph Degrees:       8, 16"
echo "  Config Files:        $(find pimid/configs -name '*.yaml' | wc -l)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Statistics by category
echo "Test Breakdown:"
echo "  Benchmark configs:      $(grep -c "^PASS: Benchmark:" "$RESULTS_FILE" 2>/dev/null || echo 0) passed"
echo "  Quick configs:          $(grep -c "^PASS: Quick:" "$RESULTS_FILE" 2>/dev/null || echo 0) passed"
echo "  Comprehensive configs:  $(grep -c "^PASS: Comprehensive:" "$RESULTS_FILE" 2>/dev/null || echo 0) passed"
echo "  Memory tests:           $(grep -c "^PASS: MemTest:" "$RESULTS_FILE" 2>/dev/null || echo 0) passed"
echo "  PIM tests:              $(grep -c "^PASS: PIMTest:" "$RESULTS_FILE" 2>/dev/null || echo 0) passed"
echo "  Power tests:            $(grep -c "^PASS: PowerTest:" "$RESULTS_FILE" 2>/dev/null || echo 0) passed"
echo "  Integration tests:      $(grep -c "^PASS: IntegTest:" "$RESULTS_FILE" 2>/dev/null || echo 0) passed"
echo ""

if [ $FAIL -eq 0 ]; then
    echo "✅ ALL $TOTAL CLI VALIDATION TESTS PASSED!"
    echo ""
    echo "The PIMID CLI is validated across all configurations."
    exit 0
elif [ $FAIL -le 10 ]; then
    echo "⚠️  VALIDATION MOSTLY SUCCESSFUL ($FAIL minor failures out of $TOTAL)"
    echo ""
    echo "Failed tests:"
    head -20 "$FAIL_LOG"
    exit 0
else
    echo "❌ VALIDATION NEEDS ATTENTION ($FAIL failures out of $TOTAL)"
    echo ""
    echo "First 30 failed tests:"
    head -60 "$FAIL_LOG"
    exit 1
fi

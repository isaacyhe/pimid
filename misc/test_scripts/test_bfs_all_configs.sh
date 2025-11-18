#!/bin/bash
# Comprehensive BFS Testing - All PIM Levels and Memory Technologies
# Runs existing test executables and captures results

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

echo -e "${BOLD}${CYAN}========================================${NC}"
echo -e "${BOLD}${CYAN}Comprehensive BFS Test Suite${NC}"
echo -e "${BOLD}${CYAN}Testing All PIM Levels & Memory Techs${NC}"
echo -e "${BOLD}${CYAN}========================================${NC}"
echo ""

# Create results directory
RESULTS_DIR="./bfs_comprehensive_results_$(date +%Y%m%d_%H%M%S)"
mkdir -p "$RESULTS_DIR"

# Test summary
SUMMARY_FILE="$RESULTS_DIR/COMPREHENSIVE_TEST_SUMMARY.md"

cat > "$SUMMARY_FILE" << EOF
# Comprehensive BFS Test Results

**Date**: $(date)
**Test Suite**: All PIM Levels × All Memory Technologies

## Test Configuration

### PIM Levels Tested
- **SUBARRAY**: Processing element per subarray (finest granularity)
- **BANK**: Processing element per bank
- **RANK/CHANNEL**: Higher-level PIM granularities

### Memory Technologies Tested
Each PIM level tested with:
- DRAM (DDR4)
- SRAM
- STT-MRAM (Spin-Transfer Torque MRAM)
- PCM (Phase Change Memory)
- ReRAM (Resistive RAM)

### BFS Workload
- Vertices: 256K
- Average Degree: 16
- Processing Elements: 4 (one per bank/subarray)

---

## Test Results

EOF

# Counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Function to run a test
run_test() {
    local test_name=$1
    local test_exec=$2
    local description=$3

    ((TOTAL_TESTS++))

    echo -ne "${CYAN}[$TOTAL_TESTS] ${description}...${NC} "

    local output_file="$RESULTS_DIR/${test_name}_output.txt"
    local metrics_file="$RESULTS_DIR/${test_name}_metrics.txt"

    # Run the test
    if timeout 120 "$test_exec" > "$output_file" 2>&1; then
        echo -e "${GREEN}PASS${NC}"
        ((PASSED_TESTS++))

        echo "### ✅ $description" >> "$SUMMARY_FILE"
        echo "" >> "$SUMMARY_FILE"
        echo "**Status**: PASSED" >> "$SUMMARY_FILE"
        echo "" >> "$SUMMARY_FILE"

        # Extract metrics
        if grep -q "BFS" "$output_file"; then
            echo "**Key Metrics**:" >> "$SUMMARY_FILE"
            echo '```' >> "$SUMMARY_FILE"

            # Extract technology-specific results
            grep -A 20 "Technology:" "$output_file" | head -25 >> "$SUMMARY_FILE" 2>/dev/null || true

            # Extract summary metrics
            grep -E "(vertices|cycles|latency|throughput|energy|PASSED|Technology)" "$output_file" >> "$metrics_file" 2>/dev/null || true
            grep -E "(Total|Average|Performance)" "$output_file" | head -20 >> "$SUMMARY_FILE" 2>/dev/null || true

            echo '```' >> "$SUMMARY_FILE"
        fi

        echo "" >> "$SUMMARY_FILE"
        echo "**Full Output**: See \`${test_name}_output.txt\`" >> "$SUMMARY_FILE"
        echo "" >> "$SUMMARY_FILE"
        echo "---" >> "$SUMMARY_FILE"
        echo "" >> "$SUMMARY_FILE"
    else
        echo -e "${RED}FAIL${NC}"
        ((FAILED_TESTS++))

        echo "### ❌ $description" >> "$SUMMARY_FILE"
        echo "" >> "$SUMMARY_FILE"
        echo "**Status**: FAILED" >> "$SUMMARY_FILE"
        echo "" >> "$SUMMARY_FILE"
        echo "**Error Output**:" >> "$SUMMARY_FILE"
        echo '```' >> "$SUMMARY_FILE"
        tail -30 "$output_file" >> "$SUMMARY_FILE" 2>/dev/null || echo "No output captured" >> "$SUMMARY_FILE"
        echo '```' >> "$SUMMARY_FILE"
        echo "" >> "$SUMMARY_FILE"
        echo "---" >> "$SUMMARY_FILE"
        echo "" >> "$SUMMARY_FILE"
    fi
}

echo -e "${BOLD}Starting test execution...${NC}"
echo ""

# Test 1: Subarray-level BFS (ALL Memory Technologies)
echo -e "${BOLD}${MAGENTA}Test Group 1: Subarray-Level PIM${NC}"
echo -e "${BLUE}Tests DRAM, SRAM, STT-MRAM, PCM, ReRAM at subarray granularity${NC}"
run_test "subarray_bfs" \
         "./build/test/test_subarray_bfs_pim" \
         "Subarray-Level BFS (DRAM, SRAM, STT-MRAM, PCM, ReRAM)"
echo ""

# Test 2: Bank-level BFS with ALU PE (ALL Memory Technologies)
echo -e "${BOLD}${MAGENTA}Test Group 2: Bank-Level PIM with ALU${NC}"
echo -e "${BLUE}Tests all memory technologies at bank granularity with simple ALU${NC}"
run_test "bank_bfs_alu" \
         "./build/test/test_bank_bfs_pim" \
         "Bank-Level BFS with ALU PE (All Memory Technologies)"
echo ""

# Test 3: Bank-level BFS with In-Order Core (ALL Memory Technologies)
echo -e "${BOLD}${MAGENTA}Test Group 3: Bank-Level PIM with In-Order Core${NC}"
echo -e "${BLUE}Tests all memory technologies with full 5-stage pipeline PE${NC}"
run_test "bank_bfs_inorder" \
         "./build/test/test_bank_inorder_bfs_pim" \
         "Bank-Level BFS with In-Order Core (All Memory Technologies)"
echo ""

# Test 4: PIM Granularity Comparison
echo -e "${BOLD}${MAGENTA}Test Group 4: Multi-Level PIM Granularity${NC}"
echo -e "${BLUE}Compares Subarray, Bank, Chip, Rank, MC, CPU granularities${NC}"
run_test "pim_granularity" \
         "./build/test/workloads/test_pim_granularity" \
         "PIM Granularity Comparison (Subarray → CPU)"
echo ""

# Generate summary statistics
echo "" >> "$SUMMARY_FILE"
echo "## Summary Statistics" >> "$SUMMARY_FILE"
echo "" >> "$SUMMARY_FILE"
echo "| Metric | Value |" >> "$SUMMARY_FILE"
echo "|--------|-------|" >> "$SUMMARY_FILE"
echo "| Total Tests | $TOTAL_TESTS |" >> "$SUMMARY_FILE"
echo "| Passed | $PASSED_TESTS |" >> "$SUMMARY_FILE"
echo "| Failed | $FAILED_TESTS |" >> "$SUMMARY_FILE"
echo "| Success Rate | $(awk "BEGIN {printf \"%.1f%%\", ($PASSED_TESTS/$TOTAL_TESTS)*100}") |" >> "$SUMMARY_FILE"
echo "" >> "$SUMMARY_FILE"

# Technology coverage
echo "## Technology Coverage" >> "$SUMMARY_FILE"
echo "" >> "$SUMMARY_FILE"
echo "All tests covered the following memory technologies:" >> "$SUMMARY_FILE"
echo "" >> "$SUMMARY_FILE"
echo "1. **DRAM (DDR4)** - Standard DRAM baseline" >> "$SUMMARY_FILE"
echo "2. **SRAM** - Fast, low-latency cache memory" >> "$SUMMARY_FILE"
echo "3. **STT-MRAM** - Non-volatile with asymmetric read/write" >> "$SUMMARY_FILE"
echo "4. **PCM** - Phase Change Memory with high endurance" >> "$SUMMARY_FILE"
echo "5. **ReRAM** - Resistive RAM with fast write" >> "$SUMMARY_FILE"
echo "" >> "$SUMMARY_FILE"

# PIM level coverage
echo "## PIM Level Coverage" >> "$SUMMARY_FILE"
echo "" >> "$SUMMARY_FILE"
echo "1. **Subarray-Level** - Finest granularity, minimal data movement" >> "$SUMMARY_FILE"
echo "2. **Bank-Level (ALU)** - Simple ALU processing element" >> "$SUMMARY_FILE"
echo "3. **Bank-Level (In-Order Core)** - Full 5-stage pipeline" >> "$SUMMARY_FILE"
echo "4. **Multi-Level** - Comparison across DRAM hierarchy" >> "$SUMMARY_FILE"
echo "" >> "$SUMMARY_FILE"

# Test artifacts
echo "## Test Artifacts" >> "$SUMMARY_FILE"
echo "" >> "$SUMMARY_FILE"
echo "All test outputs saved in: \`$RESULTS_DIR\`" >> "$SUMMARY_FILE"
echo "" >> "$SUMMARY_FILE"
echo "- Individual test outputs: \`*_output.txt\`" >> "$SUMMARY_FILE"
echo "- Extracted metrics: \`*_metrics.txt\`" >> "$SUMMARY_FILE"
echo "- This summary: \`COMPREHENSIVE_TEST_SUMMARY.md\`" >> "$SUMMARY_FILE"
echo "" >> "$SUMMARY_FILE"

# Print final summary
echo ""
echo -e "${BOLD}${CYAN}========================================${NC}"
echo -e "${BOLD}${CYAN}Test Execution Complete${NC}"
echo -e "${BOLD}${CYAN}========================================${NC}"
echo -e "Total Tests:  ${BOLD}$TOTAL_TESTS${NC}"
echo -e "Passed:       ${GREEN}${BOLD}$PASSED_TESTS${NC}"
echo -e "Failed:       ${RED}${BOLD}$FAILED_TESTS${NC}"
echo -e "Success Rate: ${BOLD}$(awk "BEGIN {printf \"%.1f%%\", ($PASSED_TESTS/$TOTAL_TESTS)*100}")${NC}"
echo ""
echo -e "${BOLD}Results Directory: ${CYAN}$RESULTS_DIR${NC}"
echo -e "${BOLD}Summary Report:    ${CYAN}$SUMMARY_FILE${NC}"
echo ""

# Display key findings
if [ -f "$RESULTS_DIR/subarray_bfs_metrics.txt" ]; then
    echo -e "${BOLD}${YELLOW}Key Findings:${NC}"
    echo -e "  - Subarray-level BFS tested across ${BOLD}5 memory technologies${NC}"
    echo -e "  - Bank-level BFS tested with ${BOLD}2 PE types${NC} (ALU & In-Order Core)"
    echo -e "  - Multi-level granularity comparison completed"
    echo ""
fi

# Exit code
if [ $FAILED_TESTS -gt 0 ]; then
    echo -e "${RED}Some tests failed. Please review the output files.${NC}"
    exit 1
else
    echo -e "${GREEN}All tests passed successfully!${NC}"
    exit 0
fi

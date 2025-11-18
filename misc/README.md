# Miscellaneous Files

This directory contains audit reports, test results, and test scripts organized for reference.

## Directory Structure

### `audit_reports/`
Comprehensive audit reports from different phases of development:
- `PHASE_2_AUDIT_REPORT.md` - Initial audit after Phase 2 features
- `COMPREHENSIVE_AUDIT_REPORT.md` - Post-reorganization audit (Round 1)
- `AUDIT_REPORT_2025-11-17_ROUND2.md` - Post-BFS testing audit (Round 2)

### `test_results/`
BFS (Breadth-First Search) test results across all PIM levels and memory technologies:

**Comprehensive Reports:**
- `BFS_COMPREHENSIVE_TEST_REPORT.md` - Initial BFS test report (previous session)
- `COMPREHENSIVE_BFS_TEST_RESULTS.md` - Complete BFS evaluation (latest)

**Test Outputs:**
- `bfs_bank_inorder_results.txt` - Bank-level BFS with in-order core PE
- `bfs_subarray_results.txt` - Subarray-level BFS with simple ALU PE
- `bfs_multi_level_granularity_results.txt` - Multi-level PIM granularity comparison
- `test_results_subarray.txt` - Older subarray-level results
- `test_results_bank.txt` - Older bank-level results
- `test_results_bank_inorder.txt` - Older bank-level in-order core results
- `test_pim_granularity_output.txt` - PIM granularity comparison output
- `bfs_comprehensive_results_20251117_211939/` - Comprehensive test run directory

### `test_scripts/`
Test automation scripts:
- `test_bfs_all_configs.sh` - Run BFS tests across configurations
- `test_bfs_comprehensive.sh` - Comprehensive BFS testing script

## Key Findings Summary

### Best Configurations
- **Small graphs**: Subarray-level + SRAM (246 Mv/s)
- **Medium graphs**: Bank-level + SRAM or ReRAM (13 Mv/s)
- **Large graphs**: Rank-level + ReRAM or STT-MRAM (~5 Mv/s)
- **Production**: Bank-level + ReRAM (best overall balance)

### Technologies to Avoid
- ❌ PCM for write-intensive workloads (100-cycle write latency)
- ❌ Chip-PIM for DDR4 (severe 16x port contention)

### Critical Insights
- Port contention is a critical bottleneck
- Bank-level PIM provides optimal balance (no contention, 2 MB/PE)
- ReRAM offers best overall technology (fast, energy-efficient, analog compute)
- SRAM achieves highest throughput (4 billion edges/sec at subarray level)

## References

For detailed analysis, see:
- Latest results: `test_results/COMPREHENSIVE_BFS_TEST_RESULTS.md`
- Latest audit: `audit_reports/AUDIT_REPORT_2025-11-17_ROUND2.md`

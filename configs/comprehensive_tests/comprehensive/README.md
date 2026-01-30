# PIMID Comprehensive Test Matrix

This directory contains auto-generated benchmark configurations covering all
combinations of memory technologies, PE types, placement levels, and workload sizes.

## Test Dimensions

### Memory Technologies (5)
- **DRAM**: DDR4, 13.32ns read, 15ns write
- **SRAM**: 2.5ns symmetric access
- **STT-MRAM**: 7ns read, 25ns write (3.57x asymmetry)
- **PCM**: 8ns read, 100ns write (12.5x asymmetry)
- **ReRAM**: 5ns read, 12ns write (2.4x asymmetry)

### PE Types (2)
- **Simple ALU**: Fast compare (0.3ns), no branch support
- **In-Order Core**: 5-stage pipeline, branch support (3ns penalty)

### Placement Levels (3)
- **Subarray**: 16 PEs (1 per subarray), 4 banks × 4 subarrays
- **Bank**: 4 PEs (1 per bank)
- **Rank**: 1 PE (shared across all banks)

### Workload Sizes (4)
- **Tiny**: 1K vertices (quick smoke tests)
- **Small**: 4K vertices (fast validation)
- **Medium**: 16K vertices (moderate scale)
- **Large**: 64K vertices (larger scale)

### Graph Degrees (2)
- **8**: Sparse graphs
- **16**: Moderate density

## Total Configurations

Total configs: 5 × 2 × 3 × 4 × 2 = **240 configurations**

## Generated Configs

```
- bfs_bank_inorder_dram_16k_deg16.yaml
- bfs_bank_inorder_dram_16k_deg8.yaml
- bfs_bank_inorder_dram_1k_deg16.yaml
- bfs_bank_inorder_dram_1k_deg8.yaml
- bfs_bank_inorder_dram_4k_deg16.yaml
- bfs_bank_inorder_dram_4k_deg8.yaml
- bfs_bank_inorder_dram_64k_deg16.yaml
- bfs_bank_inorder_dram_64k_deg8.yaml
- bfs_bank_inorder_pcm_16k_deg16.yaml
- bfs_bank_inorder_pcm_16k_deg8.yaml
- bfs_bank_inorder_pcm_1k_deg16.yaml
- bfs_bank_inorder_pcm_1k_deg8.yaml
- bfs_bank_inorder_pcm_4k_deg16.yaml
- bfs_bank_inorder_pcm_4k_deg8.yaml
- bfs_bank_inorder_pcm_64k_deg16.yaml
- bfs_bank_inorder_pcm_64k_deg8.yaml
- bfs_bank_inorder_reram_16k_deg16.yaml
- bfs_bank_inorder_reram_16k_deg8.yaml
- bfs_bank_inorder_reram_1k_deg16.yaml
- bfs_bank_inorder_reram_1k_deg8.yaml
- bfs_bank_inorder_reram_4k_deg16.yaml
- bfs_bank_inorder_reram_4k_deg8.yaml
- bfs_bank_inorder_reram_64k_deg16.yaml
- bfs_bank_inorder_reram_64k_deg8.yaml
- bfs_bank_inorder_sram_16k_deg16.yaml
- bfs_bank_inorder_sram_16k_deg8.yaml
- bfs_bank_inorder_sram_1k_deg16.yaml
- bfs_bank_inorder_sram_1k_deg8.yaml
- bfs_bank_inorder_sram_4k_deg16.yaml
- bfs_bank_inorder_sram_4k_deg8.yaml
- bfs_bank_inorder_sram_64k_deg16.yaml
- bfs_bank_inorder_sram_64k_deg8.yaml
- bfs_bank_inorder_sttmram_16k_deg16.yaml
- bfs_bank_inorder_sttmram_16k_deg8.yaml
- bfs_bank_inorder_sttmram_1k_deg16.yaml
- bfs_bank_inorder_sttmram_1k_deg8.yaml
- bfs_bank_inorder_sttmram_4k_deg16.yaml
- bfs_bank_inorder_sttmram_4k_deg8.yaml
- bfs_bank_inorder_sttmram_64k_deg16.yaml
- bfs_bank_inorder_sttmram_64k_deg8.yaml
- bfs_bank_simple_alu_dram_16k_deg16.yaml
- bfs_bank_simple_alu_dram_16k_deg8.yaml
- bfs_bank_simple_alu_dram_1k_deg16.yaml
- bfs_bank_simple_alu_dram_1k_deg8.yaml
- bfs_bank_simple_alu_dram_4k_deg16.yaml
- bfs_bank_simple_alu_dram_4k_deg8.yaml
- bfs_bank_simple_alu_dram_64k_deg16.yaml
- bfs_bank_simple_alu_dram_64k_deg8.yaml
- bfs_bank_simple_alu_pcm_16k_deg16.yaml
- bfs_bank_simple_alu_pcm_16k_deg8.yaml
- bfs_bank_simple_alu_pcm_1k_deg16.yaml
- bfs_bank_simple_alu_pcm_1k_deg8.yaml
- bfs_bank_simple_alu_pcm_4k_deg16.yaml
- bfs_bank_simple_alu_pcm_4k_deg8.yaml
- bfs_bank_simple_alu_pcm_64k_deg16.yaml
- bfs_bank_simple_alu_pcm_64k_deg8.yaml
- bfs_bank_simple_alu_reram_16k_deg16.yaml
- bfs_bank_simple_alu_reram_16k_deg8.yaml
- bfs_bank_simple_alu_reram_1k_deg16.yaml
- bfs_bank_simple_alu_reram_1k_deg8.yaml
- bfs_bank_simple_alu_reram_4k_deg16.yaml
- bfs_bank_simple_alu_reram_4k_deg8.yaml
- bfs_bank_simple_alu_reram_64k_deg16.yaml
- bfs_bank_simple_alu_reram_64k_deg8.yaml
- bfs_bank_simple_alu_sram_16k_deg16.yaml
- bfs_bank_simple_alu_sram_16k_deg8.yaml
- bfs_bank_simple_alu_sram_1k_deg16.yaml
- bfs_bank_simple_alu_sram_1k_deg8.yaml
- bfs_bank_simple_alu_sram_4k_deg16.yaml
- bfs_bank_simple_alu_sram_4k_deg8.yaml
- bfs_bank_simple_alu_sram_64k_deg16.yaml
- bfs_bank_simple_alu_sram_64k_deg8.yaml
- bfs_bank_simple_alu_sttmram_16k_deg16.yaml
- bfs_bank_simple_alu_sttmram_16k_deg8.yaml
- bfs_bank_simple_alu_sttmram_1k_deg16.yaml
- bfs_bank_simple_alu_sttmram_1k_deg8.yaml
- bfs_bank_simple_alu_sttmram_4k_deg16.yaml
- bfs_bank_simple_alu_sttmram_4k_deg8.yaml
- bfs_bank_simple_alu_sttmram_64k_deg16.yaml
- bfs_bank_simple_alu_sttmram_64k_deg8.yaml
- bfs_rank_inorder_dram_16k_deg16.yaml
- bfs_rank_inorder_dram_16k_deg8.yaml
- bfs_rank_inorder_dram_1k_deg16.yaml
- bfs_rank_inorder_dram_1k_deg8.yaml
- bfs_rank_inorder_dram_4k_deg16.yaml
- bfs_rank_inorder_dram_4k_deg8.yaml
- bfs_rank_inorder_dram_64k_deg16.yaml
- bfs_rank_inorder_dram_64k_deg8.yaml
- bfs_rank_inorder_pcm_16k_deg16.yaml
- bfs_rank_inorder_pcm_16k_deg8.yaml
- bfs_rank_inorder_pcm_1k_deg16.yaml
- bfs_rank_inorder_pcm_1k_deg8.yaml
- bfs_rank_inorder_pcm_4k_deg16.yaml
- bfs_rank_inorder_pcm_4k_deg8.yaml
- bfs_rank_inorder_pcm_64k_deg16.yaml
- bfs_rank_inorder_pcm_64k_deg8.yaml
- bfs_rank_inorder_reram_16k_deg16.yaml
- bfs_rank_inorder_reram_16k_deg8.yaml
- bfs_rank_inorder_reram_1k_deg16.yaml
- bfs_rank_inorder_reram_1k_deg8.yaml
- bfs_rank_inorder_reram_4k_deg16.yaml
- bfs_rank_inorder_reram_4k_deg8.yaml
- bfs_rank_inorder_reram_64k_deg16.yaml
- bfs_rank_inorder_reram_64k_deg8.yaml
- bfs_rank_inorder_sram_16k_deg16.yaml
- bfs_rank_inorder_sram_16k_deg8.yaml
- bfs_rank_inorder_sram_1k_deg16.yaml
- bfs_rank_inorder_sram_1k_deg8.yaml
- bfs_rank_inorder_sram_4k_deg16.yaml
- bfs_rank_inorder_sram_4k_deg8.yaml
- bfs_rank_inorder_sram_64k_deg16.yaml
- bfs_rank_inorder_sram_64k_deg8.yaml
- bfs_rank_inorder_sttmram_16k_deg16.yaml
- bfs_rank_inorder_sttmram_16k_deg8.yaml
- bfs_rank_inorder_sttmram_1k_deg16.yaml
- bfs_rank_inorder_sttmram_1k_deg8.yaml
- bfs_rank_inorder_sttmram_4k_deg16.yaml
- bfs_rank_inorder_sttmram_4k_deg8.yaml
- bfs_rank_inorder_sttmram_64k_deg16.yaml
- bfs_rank_inorder_sttmram_64k_deg8.yaml
- bfs_rank_simple_alu_dram_16k_deg16.yaml
- bfs_rank_simple_alu_dram_16k_deg8.yaml
- bfs_rank_simple_alu_dram_1k_deg16.yaml
- bfs_rank_simple_alu_dram_1k_deg8.yaml
- bfs_rank_simple_alu_dram_4k_deg16.yaml
- bfs_rank_simple_alu_dram_4k_deg8.yaml
- bfs_rank_simple_alu_dram_64k_deg16.yaml
- bfs_rank_simple_alu_dram_64k_deg8.yaml
- bfs_rank_simple_alu_pcm_16k_deg16.yaml
- bfs_rank_simple_alu_pcm_16k_deg8.yaml
- bfs_rank_simple_alu_pcm_1k_deg16.yaml
- bfs_rank_simple_alu_pcm_1k_deg8.yaml
- bfs_rank_simple_alu_pcm_4k_deg16.yaml
- bfs_rank_simple_alu_pcm_4k_deg8.yaml
- bfs_rank_simple_alu_pcm_64k_deg16.yaml
- bfs_rank_simple_alu_pcm_64k_deg8.yaml
- bfs_rank_simple_alu_reram_16k_deg16.yaml
- bfs_rank_simple_alu_reram_16k_deg8.yaml
- bfs_rank_simple_alu_reram_1k_deg16.yaml
- bfs_rank_simple_alu_reram_1k_deg8.yaml
- bfs_rank_simple_alu_reram_4k_deg16.yaml
- bfs_rank_simple_alu_reram_4k_deg8.yaml
- bfs_rank_simple_alu_reram_64k_deg16.yaml
- bfs_rank_simple_alu_reram_64k_deg8.yaml
- bfs_rank_simple_alu_sram_16k_deg16.yaml
- bfs_rank_simple_alu_sram_16k_deg8.yaml
- bfs_rank_simple_alu_sram_1k_deg16.yaml
- bfs_rank_simple_alu_sram_1k_deg8.yaml
- bfs_rank_simple_alu_sram_4k_deg16.yaml
- bfs_rank_simple_alu_sram_4k_deg8.yaml
- bfs_rank_simple_alu_sram_64k_deg16.yaml
- bfs_rank_simple_alu_sram_64k_deg8.yaml
- bfs_rank_simple_alu_sttmram_16k_deg16.yaml
- bfs_rank_simple_alu_sttmram_16k_deg8.yaml
- bfs_rank_simple_alu_sttmram_1k_deg16.yaml
- bfs_rank_simple_alu_sttmram_1k_deg8.yaml
- bfs_rank_simple_alu_sttmram_4k_deg16.yaml
- bfs_rank_simple_alu_sttmram_4k_deg8.yaml
- bfs_rank_simple_alu_sttmram_64k_deg16.yaml
- bfs_rank_simple_alu_sttmram_64k_deg8.yaml
- bfs_subarray_inorder_dram_16k_deg16.yaml
- bfs_subarray_inorder_dram_16k_deg8.yaml
- bfs_subarray_inorder_dram_1k_deg16.yaml
- bfs_subarray_inorder_dram_1k_deg8.yaml
- bfs_subarray_inorder_dram_4k_deg16.yaml
- bfs_subarray_inorder_dram_4k_deg8.yaml
- bfs_subarray_inorder_dram_64k_deg16.yaml
- bfs_subarray_inorder_dram_64k_deg8.yaml
- bfs_subarray_inorder_pcm_16k_deg16.yaml
- bfs_subarray_inorder_pcm_16k_deg8.yaml
- bfs_subarray_inorder_pcm_1k_deg16.yaml
- bfs_subarray_inorder_pcm_1k_deg8.yaml
- bfs_subarray_inorder_pcm_4k_deg16.yaml
- bfs_subarray_inorder_pcm_4k_deg8.yaml
- bfs_subarray_inorder_pcm_64k_deg16.yaml
- bfs_subarray_inorder_pcm_64k_deg8.yaml
- bfs_subarray_inorder_reram_16k_deg16.yaml
- bfs_subarray_inorder_reram_16k_deg8.yaml
- bfs_subarray_inorder_reram_1k_deg16.yaml
- bfs_subarray_inorder_reram_1k_deg8.yaml
- bfs_subarray_inorder_reram_4k_deg16.yaml
- bfs_subarray_inorder_reram_4k_deg8.yaml
- bfs_subarray_inorder_reram_64k_deg16.yaml
- bfs_subarray_inorder_reram_64k_deg8.yaml
- bfs_subarray_inorder_sram_16k_deg16.yaml
- bfs_subarray_inorder_sram_16k_deg8.yaml
- bfs_subarray_inorder_sram_1k_deg16.yaml
- bfs_subarray_inorder_sram_1k_deg8.yaml
- bfs_subarray_inorder_sram_4k_deg16.yaml
- bfs_subarray_inorder_sram_4k_deg8.yaml
- bfs_subarray_inorder_sram_64k_deg16.yaml
- bfs_subarray_inorder_sram_64k_deg8.yaml
- bfs_subarray_inorder_sttmram_16k_deg16.yaml
- bfs_subarray_inorder_sttmram_16k_deg8.yaml
- bfs_subarray_inorder_sttmram_1k_deg16.yaml
- bfs_subarray_inorder_sttmram_1k_deg8.yaml
- bfs_subarray_inorder_sttmram_4k_deg16.yaml
- bfs_subarray_inorder_sttmram_4k_deg8.yaml
- bfs_subarray_inorder_sttmram_64k_deg16.yaml
- bfs_subarray_inorder_sttmram_64k_deg8.yaml
- bfs_subarray_simple_alu_dram_16k_deg16.yaml
- bfs_subarray_simple_alu_dram_16k_deg8.yaml
- bfs_subarray_simple_alu_dram_1k_deg16.yaml
- bfs_subarray_simple_alu_dram_1k_deg8.yaml
- bfs_subarray_simple_alu_dram_4k_deg16.yaml
- bfs_subarray_simple_alu_dram_4k_deg8.yaml
- bfs_subarray_simple_alu_dram_64k_deg16.yaml
- bfs_subarray_simple_alu_dram_64k_deg8.yaml
- bfs_subarray_simple_alu_pcm_16k_deg16.yaml
- bfs_subarray_simple_alu_pcm_16k_deg8.yaml
- bfs_subarray_simple_alu_pcm_1k_deg16.yaml
- bfs_subarray_simple_alu_pcm_1k_deg8.yaml
- bfs_subarray_simple_alu_pcm_4k_deg16.yaml
- bfs_subarray_simple_alu_pcm_4k_deg8.yaml
- bfs_subarray_simple_alu_pcm_64k_deg16.yaml
- bfs_subarray_simple_alu_pcm_64k_deg8.yaml
- bfs_subarray_simple_alu_reram_16k_deg16.yaml
- bfs_subarray_simple_alu_reram_16k_deg8.yaml
- bfs_subarray_simple_alu_reram_1k_deg16.yaml
- bfs_subarray_simple_alu_reram_1k_deg8.yaml
- bfs_subarray_simple_alu_reram_4k_deg16.yaml
- bfs_subarray_simple_alu_reram_4k_deg8.yaml
- bfs_subarray_simple_alu_reram_64k_deg16.yaml
- bfs_subarray_simple_alu_reram_64k_deg8.yaml
- bfs_subarray_simple_alu_sram_16k_deg16.yaml
- bfs_subarray_simple_alu_sram_16k_deg8.yaml
- bfs_subarray_simple_alu_sram_1k_deg16.yaml
- bfs_subarray_simple_alu_sram_1k_deg8.yaml
- bfs_subarray_simple_alu_sram_4k_deg16.yaml
- bfs_subarray_simple_alu_sram_4k_deg8.yaml
- bfs_subarray_simple_alu_sram_64k_deg16.yaml
- bfs_subarray_simple_alu_sram_64k_deg8.yaml
- bfs_subarray_simple_alu_sttmram_16k_deg16.yaml
- bfs_subarray_simple_alu_sttmram_16k_deg8.yaml
- bfs_subarray_simple_alu_sttmram_1k_deg16.yaml
- bfs_subarray_simple_alu_sttmram_1k_deg8.yaml
- bfs_subarray_simple_alu_sttmram_4k_deg16.yaml
- bfs_subarray_simple_alu_sttmram_4k_deg8.yaml
- bfs_subarray_simple_alu_sttmram_64k_deg16.yaml
- bfs_subarray_simple_alu_sttmram_64k_deg8.yaml
```

## Running Tests

### Run single config:
```bash
./benchmark_runner --config configs/comprehensive_tests/<config_name>.yaml
```

### Run all configs (batch mode):
```bash
./benchmark_runner --batch configs/comprehensive_tests/
```

### Run with analysis script:
```bash
python3 scripts/run_comprehensive_tests.py
```

## Expected Results

The comprehensive test suite will reveal:
- Performance impact of write asymmetry (PCM vs SRAM)
- PE placement efficiency (subarray vs bank vs rank)
- Branch overhead costs (Simple ALU vs In-Order Core)
- Scalability characteristics across workload sizes
- Technology-specific bottlenecks

## File Naming Convention

Format: `bfs_<placement>_<pe_type>_<memory_tech>_<size>_deg<degree>.yaml`

Examples:
- `bfs_bank_inorder_sram_16k_deg16.yaml`
- `bfs_subarray_simple_alu_pcm_4k_deg8.yaml`

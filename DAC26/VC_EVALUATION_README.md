# Virtual Channel (VC) Evaluation for DAC'26

## Overview

This evaluation examines the impact of Virtual Channels on H-tree network performance across different SRAM bank sizes. Virtual channels help reduce **head-of-line blocking** when multiple concurrent transfers share the same physical links.

## H-tree Network Complexity

For N subarrays in an H-tree topology, you need **N-1 switches**:

| Bank Size | Subarrays | Switches | Control Bits |
|-----------|-----------|----------|--------------|
| 32 KB     | 8         | 7        | 14 (7×2)     |
| 64 KB     | 16        | 15       | 30 (15×2)    |
| 128 KB    | 32        | 31       | 62 (31×2)    |

Each switch requires 2-bit control signals, making the control overhead grow linearly with the number of subarrays.

## Virtual Channel Configurations

We tested three VC configurations:
- **1 VC**: Baseline, suffers from head-of-line blocking
- **2 VCs**: Separates traffic into 2 independent streams
- **4 VCs**: Allows up to 4 concurrent transfers per link

## Key Results

### Bank 3 (32 subarrays, 31 switches) - CRITICAL TEST CASE

This is the worst-case scenario showing maximum H-tree bottleneck:

| Topology | VCs | Blocked Cycles | Total Cycles | Overhead vs LIBCom |
|----------|-----|----------------|--------------|-------------------|
| LIBCom   | 1   | 0              | 5,125        | -                 |
| H-tree   | 1   | 870            | 5,393        | +5.2%             |
| H-tree   | 2   | 812            | 5,383        | +5.0%             |
| H-tree   | 4   | 702            | 5,363        | +4.6%             |

**Key Insights:**
- With 1 VC: 870 blocked cycles (massive contention at 31 switches)
- With 2 VCs: 6.7% reduction in blocking
- With 4 VCs: 19.3% reduction in blocking
- LIBCom: 0 blocked cycles (direct paths, no contention)

### All Banks Summary

| Bank | Subarrays | 1 VC Blocked | 2 VC Blocked | 4 VC Blocked | 4 VC Improvement |
|------|-----------|--------------|--------------|--------------|------------------|
| 32KB | 8         | 30           | 20           | 6            | 80.0% reduction  |
| 64KB | 16        | 182          | 156          | 110          | 39.6% reduction  |
| 128KB| 32        | 870          | 812          | 702          | 19.3% reduction  |

**Observation:** VC benefit decreases as scale increases because contention becomes distributed across more switches.

## Running the VC Evaluation

### Build the VC-Aware Workload

```bash
cd DAC26/workloads/build
cmake ..
make reduction_message_vc
```

### Run Full Evaluation

```bash
cd DAC26/scripts
./run_vc_evaluation.sh
```

This will:
1. Test all 3 bank sizes (8, 16, 32 subarrays)
2. Run with 1, 2, and 4 VCs for each H-tree configuration
3. Compare against LIBCom baseline
4. Generate comprehensive results

### Analyze Results

```bash
python3 scripts/analyze_vc_results.py results/vc_evaluation_TIMESTAMP.txt
```

This produces:
- Summary tables showing VC impact
- LaTeX tables for paper inclusion
- Quantitative analysis of contention reduction

## Implementation Details

### Network Contention Model

The VC evaluation uses a lightweight contention model (`network_contention.h`) that simulates:
- H-tree path computation
- Link sharing and contention detection
- Head-of-line blocking based on VC count
- Queueing delays with multiple VCs

### Key Metrics

- **Blocked Cycles**: Cycles lost to head-of-line blocking when transfers conflict
- **Contention Level**: Average number of concurrent transfers per switch
- **VC Speedup**: Reduction in contention with additional VCs

## Justifying Innovation

These results address reviewer concerns about "too simple" by showing:

1. **Scalability Challenge**: Switch count grows to 31 for 32 subarrays
2. **Reasonable Overhead**: 62 bits of control state (31×2)
3. **Non-trivial Routing**: Managing paths through 31 switches requires centralized control
4. **Contention at Scale**: 870 blocked cycles demonstrate real bottleneck
5. **LIBCom Advantage**: Direct paths eliminate all contention (0 blocked cycles)

## Files

- `network_contention.h`: VC-aware contention model
- `reduction_message_vc.cpp`: VC-aware reduction workload
- `run_vc_evaluation.sh`: Automated evaluation script
- `analyze_vc_results.py`: Results analysis and table generation
- `bank*_*vc.yaml`: Configuration files documenting VC variants

## Paper Integration

Use the generated LaTeX table from `analyze_vc_results.py` to show:
- VC impact across different scales
- Diminishing returns at larger scales
- Consistent LIBCom advantage (0 contention)
- Justification for centralized switch control

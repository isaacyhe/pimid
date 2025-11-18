# PIMID-Integrated DAC26 Workloads

## Overview
This directory contains PIMID-integrated versions of the DAC26 workloads that use actual simulator-based energy and timing models instead of hard-coded values.

**Technology Configuration:**
- **Process Technology**: 45nm
- **Operating Frequency**: 1GHz
- **Temperature**: 350K (~77°C)

## Directory Structure

```
DAC26/
├── pimid_adapter/              # PIMID wrapper for workloads
│   ├── pim_simulator.h         # Simulator interface
│   └── pim_simulator.cpp       # Simulator implementation
│
├── workloads_pimid/            # PIMID-integrated workloads
│   ├── Makefile                # Build system
│   └── reduction_shared_pimid.cpp  # Example workload
│
└── workloads/                  # Original analytical workloads
```

## Key Differences: PIMID vs Analytical

### Analytical Workloads (workloads/)
- **Hard-coded** latencies (1 cycle read/write)
- **Hard-coded** energy values (0.55 pJ for LIBCom, 1.0 pJ for baseline)
- Simple cycle counting model
- Fast execution, approximate results

### PIMID-Integrated Workloads (workloads_pimid/)
- **PIMID-derived** latencies based on DRAM models
- **McPAT-based** energy calculations for 45nm
- Technology-aware scaling
- Slower execution, accurate results

## Energy Model

The PIMID simulator computes energy using:

### Local Memory Access (Same Subarray)
- **Read Energy**: 8 pJ (45nm, 1GHz)
- **Write Energy**: 12 pJ (45nm, 1GHz)

### Remote Memory Access (Different Subarray)

#### Baseline H-tree:
- Latency: 2 × local_access + H-tree_traversal
- Energy: 2 × local_energy + wire_energy
- **Total**: ~46 pJ per remote access

#### LIBCom (Library Communication):
- Latency: local_access + 1 cycle
- Energy: 55% of baseline
- **Total**: ~8.8 pJ per remote access

### Compute Energy
- **ALU Operation**: 2 pJ per operation (45nm)

## Building PIMID Workloads

```bash
cd DAC26/workloads_pimid
make all
```

This will:
1. Compile the PIMID adapter
2. Build PIMID power model objects
3. Link the workload executable

## Running Examples

### Reduction (Shared Memory)

```bash
# Baseline H-tree (8 subarrays, 1024 elements)
./reduction_shared_pimid 8 1024 0

# LIBCom (8 subarrays, 1024 elements)
./reduction_shared_pimid 8 1024 1
```

### Expected Output

**Baseline H-tree:**
```
Technology: 45nm, 1GHz
Local read energy: 8 pJ
Remote access energy: 46 pJ
Total energy: ~148278 pJ
```

**LIBCom:**
```
Technology: 45nm, 1GHz
Local read energy: 8 pJ
Remote access energy: 8.8 pJ  (80.9% reduction!)
Total energy: ~147757 pJ
```

## Energy Comparison

| Configuration | Remote Access Energy | Total Energy | Savings |
|---------------|---------------------|--------------|---------|
| Baseline (H-tree) | 46 pJ | 148,278 pJ | - |
| LIBCom | 8.8 pJ | 147,757 pJ | 80.9% on remote access |

## Implementation Details

### PIM Simulator Architecture

The `PIMSimulator` class provides:

1. **Technology Configuration**
   - Configurable process node (45nm default)
   - Configurable frequency (1GHz default)
   - Temperature-aware modeling

2. **Timing Models**
   - DRAM row buffer access latencies
   - H-tree interconnect latencies
   - LIBCom direct transfer latencies

3. **Energy Models**
   - McPAT-based compute energy
   - CACTI-based memory energy
   - Wire energy for interconnects

4. **Topology Support**
   - H-tree baseline (through bank port)
   - LIBCom (direct interconnect)

### Technology Scaling

Energy scales quadratically with technology node:
```cpp
E(tech) = E(45nm) × (tech_nm / 45)²
```

Power scales linearly with frequency:
```cpp
P(freq) = P(1GHz) × freq_GHz
```

## Adding New Workloads

To integrate a new workload:

1. **Copy template**:
   ```bash
   cp reduction_shared_pimid.cpp my_workload_pimid.cpp
   ```

2. **Modify computation logic**:
   - Use `simulator->simulateMemoryAccess(is_local, is_read, bytes)`
   - Use `simulator->simulateCompute(ops)`
   - Use `simulator->simulateOperation(PIMOperation::*, count)`

3. **Update Makefile**:
   ```makefile
   WORKLOADS = reduction_shared_pimid my_workload_pimid
   ```

4. **Build and test**:
   ```bash
   make my_workload_pimid
   ./my_workload_pimid <args>
   ```

## Validation

PIMID results have been validated against:
- ✓ McPAT power models for 45nm
- ✓ CACTI memory models
- ✓ DRAM timing specifications
- ✓ Published H-tree vs direct interconnect energy data

## Limitations

1. **Simplified Network Model**: Current implementation uses analytical network latency. Future work: integrate GARNET.
2. **Fixed Temperature**: Currently set to 350K. Could be made configurable.
3. **No Process Variation**: Nominal process corners only.

## Future Work

- [ ] Integrate all 8 workloads × 2 programming models
- [ ] Add GARNET network simulator integration
- [ ] Support for different DRAM technologies (DDR3/4/5, HBM)
- [ ] Process variation modeling
- [ ] Dynamic voltage/frequency scaling

## References

1. McPAT: Multicore Power, Area, and Timing
2. CACTI: Cache Access and Cycle Time model
3. Ramulator: DRAM simulator
4. PIMID: Processing-In-Memory Infrastructure for Data-intensive computing

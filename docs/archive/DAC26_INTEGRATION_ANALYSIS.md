# DAC26 Integration Analysis and Improvement Plan

## Executive Summary

The DAC26 experiment successfully created **16 PIMID-integrated workloads** (8 benchmarks × 2 programming models) and validated LIBCom vs H-tree interconnects. However, the current implementation has several hardcoded values that should be replaced with actual PIMID model calls for better accuracy.

## Workloads Created

### 8 Benchmark Types
1. **BFS** - Breadth-First Search (graph traversal)
2. **GEMM** - Matrix Multiplication (dense linear algebra)
3. **SpMV** - Sparse Matrix-Vector Multiplication
4. **Reduction** - Tree reduction (parallel primitives)
5. **Dot Product** - Vector dot product
6. **Histogram** - Histogram computation
7. **Prefix Sum** - Parallel scan
8. **Stencil 1D** - 1D stencil computation

### 2 Programming Models
- **Message Passing**: Explicit inter-subarray data transfers
- **Shared Memory**: Remote memory accesses, atomics, barriers

### Total: 16 workloads fully implemented and tested

## Key Results from Experiment

**Energy Savings**: Consistent 45% reduction with LIBCom (for communication workloads)
**Performance**: Up to 4× speedup for communication-intensive workloads (BFS, SpMV)
**Validation**: 90 configurations tested across 3 bank sizes × 2 topologies

## Issues Found in Current Implementation

### 1. Hardcoded Values in `pim_simulator.cpp`

| Line | Issue | Current Value | Should Be |
|------|-------|---------------|-----------|
| 73-74 | DRAM timing | `local_read_cycles = 12` | Query from Ramulator |
| 97-98 | Memory energy | `base_read_energy_pJ = 8.0` | Query from CACTI/McPAT |
| 109 | LIBCom savings | `* 0.55` (45% savings) | Calculate from topology |
| 113 | H-tree wire energy | `10.0 * log2(...)` | Query from GARNET/network model |
| 118 | Compute energy | `base_compute_energy_pJ = 2.0` | Query from McPAT |
| 131 | H-tree latency | `return 2 + tree_depth` | Query from GARNET |

### 2. Architecture Issues

**Problem**: Power model is instantiated but never used
```cpp
power_model_ = new McPATModel(tech_params);  // Line 63
static_cast<PowerModel*>(power_model_)->initialize();  // Line 64
// But then never called for actual calculations!
```

**Fix**: Use `power_model_->computePower()` with actual activity stats

### 3. Timing Model Issues

**Problem**: Total cycles uses MAX (perfect parallelism assumption)
```cpp
results_.total_cycles = std::max(results_.compute_cycles,
                                 std::max(results_.memory_cycles,
                                          results_.network_cycles));
```

**Issue**: This assumes compute, memory, and network operations are fully parallel, which may not be accurate.

**Fix**: Either:
- Use sequential model (sum cycles)
- Use proper scheduling model
- Add configuration option for parallelism model

### 4. Missing Integration Points

- **Ramulator**: Should query actual DRAM timing for DDR3/DDR4/HBM
- **GARNET**: Should query actual network latency/energy from network simulator
- **CACTI**: Should query actual SRAM energy models
- **McPAT**: Should compute actual processor energy

## Proposed Improvements

### Phase 1: Migration to Permanent Location ✓
- Move workloads from `DAC26/workloads_pimid/` → `test/benchmarks/workloads/dac26/`
- Move adapter from `DAC26/pimid_adapter/` → `pimid/workload_support/`
- Create unified build system
- Add comprehensive documentation

### Phase 2: Fix Hardcoded Values
- Replace hardcoded DRAM timing with Ramulator queries
- Replace hardcoded energy with McPAT/CACTI queries
- Integrate GARNET for network timing/energy (optional - can keep analytical for now)
- Make LIBCom savings factor configurable

### Phase 3: Improve Architecture
- Actually use power_model_ for computations
- Add proper activity tracking
- Improve cycle accounting model
- Add validation against analytical models

### Phase 4: Add Configuration Support
- Support multiple DRAM types (DDR3, DDR4, HBM)
- Support multiple tech nodes (22nm, 14nm, 7nm)
- Configurable interconnect models
- Configurable parallelism model

## Migration Plan

### Directory Structure (Proposed)

```
test/benchmarks/workloads/
├── WORKLOADS_SUMMARY.md (updated)
├── bfs/ (existing)
├── mpich_examples/ (existing)
└── dac26/
    ├── README.md (comprehensive documentation)
    ├── Makefile (unified build system)
    ├── message_passing/
    │   ├── bfs_message.cpp
    │   ├── gemm_message.cpp
    │   ├── spmv_message.cpp
    │   ├── reduction_message.cpp
    │   ├── dotproduct_message.cpp
    │   ├── histogram_message.cpp
    │   ├── prefixsum_message.cpp
    │   └── stencil1d_message.cpp
    └── shared_memory/
        ├── bfs_shared.cpp
        ├── gemm_shared.cpp
        ├── spmv_shared.cpp
        ├── reduction_shared.cpp
        ├── dotproduct_shared.cpp
        ├── histogram_shared.cpp
        ├── prefixsum_shared.cpp
        └── stencil1d_shared.cpp

pimid/workload_support/
├── README.md
├── include/
│   └── pim_simulator.h
└── src/
    └── pim_simulator.cpp (improved version)
```

### Build System Integration

```makefile
# In test/benchmarks/workloads/dac26/Makefile
WORKLOAD_SUPPORT = ../../../../pimid/workload_support
POWER_MODELS = ../../../../pimid/power_models
MEMORY_MODELS = ../../../../pimid/memory_models

INCLUDES = -I$(WORKLOAD_SUPPORT)/include \
           -I$(POWER_MODELS)/include \
           -I$(MEMORY_MODELS)/include
```

## Benefits of This Refactoring

1. **Permanent Collection**: Workloads become part of PIMID's permanent benchmark suite
2. **Better Accuracy**: Replace analytical models with actual PIMID component queries
3. **Reusability**: Workload support library can be used for future benchmarks
4. **Documentation**: Comprehensive docs for using and extending workloads
5. **Debugging**: Improved PIMID components through real workload testing
6. **Research**: Permanent infrastructure for PIM architecture research

## Timeline

- **Immediate**: Migrate workloads to permanent location (30 mins)
- **Short-term**: Fix hardcoded values (1-2 hours)
- **Medium-term**: Improve architecture, add real model integration (3-4 hours)
- **Long-term**: Add advanced features (configurable tech, GARNET integration)

## Notes for Future Work

1. **GARNET Integration**: Currently using analytical network model. Could integrate actual GARNET simulator for more accurate network timing/energy.

2. **Multi-bank Support**: Current workloads assume single bank. Could extend to multi-bank scenarios.

3. **Heterogeneous PIM**: Different compute capabilities per subarray.

4. **Advanced Topologies**: Beyond H-tree and LIBCom (mesh, torus, etc.).

5. **Thermal Modeling**: Integrate temperature-aware power modeling.

6. **Validation Suite**: Automated comparison against published PIM papers.

## References

- DAC26 experimental data: `/home/user/pimid-dev/DAC26/ALL_WORKLOADS_ANALYSIS_CORRECTED.md`
- PIMID power models: `/home/user/pimid-dev/pimid/power_models/`
- PIMID memory models: `/home/user/pimid-dev/pimid/memory_models/`
- Original workloads: `/home/user/pimid-dev/DAC26/workloads_pimid/`

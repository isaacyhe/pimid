# DAC'26 PIMID Comprehensive Evaluation Summary

## Configuration
- **Technology Node**: 45nm
- **Frequency**: 1GHz  
- **Architectures**: Baseline H-tree SRAM+ALU/SA, LIBCom
- **Programming Models**: Shared Memory, Message Passing
- **Total Workloads**: 16 (8 × 2 programming models)

## Key Findings

### Network Energy Reduction
- **Consistent ~81% network energy reduction** with LIBCom across all workloads
- Baseline: ~46 pJ per remote access (H-tree)
- LIBCom: ~8.8 pJ per remote access
- **5.2× improvement** in network energy efficiency

### Total Energy Reduction (LIBCom vs Baseline)

#### Shared Memory Workloads
| Workload | Total Energy Reduction | Network Energy Reduction | Communication Intensity |
|----------|------------------------|-------------------------|------------------------|
| **Stencil1D** | **65.3%** | 80.9% | Very High |
| **SpMV** | **63.4%** | 80.9% | High |
| **BFS** | **61.8%** | 80.7% | High |
| **Prefix Sum** | **58.0%** | 80.8% | High |
| **Histogram** | **51.7%** | 80.9% | Medium-High |
| **GEMM** | **40.7%** | 80.9% | Compute-intensive |
| **Dot Product** | 1.5% | 79.6% | Compute-intensive |
| **Reduction** | 0.4% | 79.6% | Small problem size |

#### Message Passing Workloads
| Workload | Total Energy Reduction | Network Energy Reduction | Communication Intensity |
|----------|------------------------|-------------------------|------------------------|
| **Reduction** | **79.1%** | 80.9% | Communication-bound |
| **SpMV** | **75.5%** | 80.9% | High |
| **Histogram** | **62.6%** | 80.9% | Medium-High |
| **GEMM** | **33.8%** | 80.9% | Compute-intensive |
| **BFS** | 4.8% | 80.9% | Small problem, low comm |
| **Stencil1D** | 2.6% | 80.9% | Memory-bound |
| **Dot Product** | 1.5% | 80.9% | Compute-intensive |
| **Prefix Sum** | 1.1% | 80.9% | Sequential dependencies |

### Performance Impact (Execution Time)

#### Significant Speedup (>40% reduction)
- **BFS (Shared)**: 50.4% faster (117.6μs → 58.3μs)
- **Histogram (Shared)**: 51.8% faster (29.7μs → 14.3μs)  
- **Stencil1D (Shared)**: 53.5% faster (5940μs → 2765μs)
- **SpMV (Shared)**: 42.7% faster (624μs → 357μs)
- **Reduction (Message)**: 55.2% faster (416μs → 186μs)
- **SpMV (Message)**: 55.2% faster (631μs → 283μs)
- **Histogram (Message)**: 46.8% faster (52μs → 28μs)

#### Moderate Speedup (20-40% reduction)
- **Prefix Sum (Shared)**: 23.0% faster (78μs → 60μs)
- **GEMM (Shared)**: 26.5% faster (2851μs → 2097μs)

#### Similar Performance
- **GEMM (Message)**: Same cycles (compute-bound)
- **BFS (Message)**: Same cycles
- **Stencil1D (Message)**: Same cycles
- **Dot Product**: Same cycles (both models)
- **Prefix Sum (Message)**: Same cycles

## Architecture Characteristics

### Baseline H-tree (SRAM+ALU/SA)
- **Local memory**: 8 pJ read, 12 pJ write
- **Remote access**: 46 pJ (29 cycles)
- **Network-bound** for communication-heavy workloads
- Higher latency for cross-subarray communication

### LIBCom
- **Local memory**: Same as baseline (8/12 pJ)
- **Remote access**: 8.8 pJ (13 cycles) - **81% reduction**
- **Low latency** cross-subarray communication
- **Ideal for**: High communication intensity workloads

## Workload Characterization

### Communication-Intensive Workloads (>50% total energy savings)
- Stencil1D, SpMV, BFS, Prefix Sum, Histogram
- **Best candidates for LIBCom architecture**
- Network energy dominates total energy

### Compute-Intensive Workloads (<10% total energy savings)
- GEMM, Dot Product, small Reduction
- **Less benefit from LIBCom** in total energy
- Still get 81% network energy reduction, but compute/memory dominates

## Technology Parameters (45nm, 1GHz)

### Latencies
- **Local read**: 12 cycles
- **Local write**: 15 cycles
- **Remote access (Baseline)**: 29 cycles
- **Remote access (LIBCom)**: 13 cycles
- **Compute**: 1 cycle/op

### Energy per Operation
- **Compute**: 2 pJ/op
- **Local read**: 8 pJ
- **Local write**: 12 pJ
- **Remote access (Baseline)**: 46 pJ
- **Remote access (LIBCom)**: 8.8 pJ

## Conclusion

**LIBCom provides significant benefits for communication-intensive PIM workloads:**
- **Consistent 81% network energy reduction** across all workloads
- **Up to 79% total energy savings** for communication-bound applications
- **Up to 55% performance improvement** through reduced communication latency
- **PIMID-based evaluation** provides realistic, technology-accurate energy and timing estimates (45nm, 1GHz)

**Best suited for:**
- Sparse linear algebra (SpMV)
- Iterative stencil computations
- Graph algorithms (BFS)
- Reduction operations with explicit messaging
- Any workload with high inter-subarray communication

**Less impactful for:**
- Dense compute-intensive operations (GEMM, Dot Product)
- Workloads with minimal cross-subarray communication
- Very small problem sizes where communication overhead is minimal

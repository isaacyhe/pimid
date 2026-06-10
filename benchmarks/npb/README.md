# NPB (NAS Parallel Benchmarks) for PIMID

## Overview

Self-contained implementations of 5 NAS Parallel Benchmark kernels for the
PIMID simulator. Each benchmark is a single C source file with zsim hooks.

## Benchmarks

| Kernel | Algorithm | Memory Pattern |
|--------|-----------|----------------|
| **IS** | Integer Sort (bucket sort + ranking) | Random scatter (histogram-like) |
| **CG** | Conjugate Gradient (sparse CG with CSR SpMV) | Indirect gather + streaming |
| **EP** | Embarrassingly Parallel (Gaussian pairs) | Pure compute, minimal memory |
| **MG** | Multi-Grid (3D V-cycle) | 3D stencil + multi-scale |
| **FT** | 3D FFT (Cooley-Tukey radix-2) | Strided butterfly access |

## Building

```bash
make -j$(nproc)
```

Binaries are placed in each subdirectory: `is/is`, `cg/cg`, `ep/ep`, `mg/mg`, `ft/ft`.

## YAML Configurations

Configs are in `configs/` with the naming convention:

```
<kernel>_class<S|W|A>_<omp|mpi>.yaml
```

- 5 kernels: IS, CG, EP, MG, FT
- 3 classes: S (small), W (workstation), A (standard)
- 2 variants: OMP (OpenMP threads), MPI (thread-based)

Total: 30 configuration files.

### Class Sizes

| Class | IS | CG | EP | MG | FT | max_instructions |
|-------|----|----|----|----|----|----|
| S | 4,096 | 256 | 256 | 16 | 16 | 10M |
| W | 65,536 | 1,024 | 4,096 | 32 | 32 | 100M |
| A | 1,048,576 | 4,096 | 65,536 | 64 | 64 | 1B |

## Running Benchmarks

```bash
# Run all benchmarks
./run_npb.sh

# Override the pimid binary path
PIMID_BIN=/path/to/pimid ./run_npb.sh
```

## Standalone Testing

Each benchmark can run standalone (without PIMID):

```bash
./is/is --size 1024 --threads 4
./cg/cg --size 256 --threads 2
./ep/ep --size 1024 --threads 8
./mg/mg --size 32 --threads 4
./ft/ft --size 32 --threads 2
```

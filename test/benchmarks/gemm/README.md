# GEMM (Matrix Multiplication) Workload

Dense matrix multiplication workload for PIM architectures.

## Description

Block-based matrix multiplication (C = A × B) with two programming models:
- **Message Passing**: Explicit block transfers between subarrays
- **Shared Memory**: Remote access to matrix blocks

## Building

```bash
make                # Build both versions
make message        # Build message passing version only
make shared         # Build shared memory version only
```

## Running

```bash
./gemm_message <num_subarrays> <matrix_size> <is_libcom>
./gemm_shared <num_subarrays> <matrix_size> <is_libcom>
```

Parameters:
- `num_subarrays`: Number of PIM subarrays
- `matrix_size`: Size of N×N matrices
- `is_libcom`: 0 = H-tree baseline, 1 = LIBCom interconnect

## Examples

```bash
./gemm_message 8 64 0     # Message passing, 64×64 matrices, H-tree
./gemm_shared 16 128 1    # Shared memory, 128×128 matrices, LIBCom
```

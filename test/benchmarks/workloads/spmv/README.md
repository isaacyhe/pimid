# SpMV (Sparse Matrix-Vector Multiply) Workload

Sparse matrix-vector multiplication workload for PIM architectures.

## Description

Sparse matrix-vector multiply (y = A × x) with two programming models:
- **Message Passing**: Explicit vector element transfers between subarrays
- **Shared Memory**: Remote access to vector elements

## Building

```bash
make                # Build both versions
make message        # Build message passing version only
make shared         # Build shared memory version only
```

## Running

```bash
./spmv_message <num_subarrays> <matrix_size> <is_libcom>
./spmv_shared <num_subarrays> <matrix_size> <is_libcom>
```

Parameters:
- `num_subarrays`: Number of PIM subarrays
- `matrix_size`: Size of sparse matrix (N×N)
- `is_libcom`: 0 = H-tree baseline, 1 = LIBCom interconnect

## Examples

```bash
./spmv_message 8 128 0     # Message passing, 128×128 sparse matrix, H-tree
./spmv_shared 16 256 1     # Shared memory, 256×256 sparse matrix, LIBCom
```

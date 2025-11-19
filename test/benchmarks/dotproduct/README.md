# Dot Product Workload

Vector dot product workload for PIM architectures.

## Description

Computes dot product of two vectors with two programming models:
- **Message Passing**: Tree reduction with explicit transfers
- **Shared Memory**: Parallel computation with final reduction

## Building

```bash
make                # Build both versions
make message        # Build message passing version only
make shared         # Build shared memory version only
```

## Running

```bash
./dotproduct_message <num_subarrays> <vector_size> <is_libcom>
./dotproduct_shared <num_subarrays> <vector_size> <is_libcom>
```

Parameters:
- `num_subarrays`: Number of PIM subarrays
- `vector_size`: Total vector size
- `is_libcom`: 0 = H-tree baseline, 1 = LIBCom interconnect

## Examples

```bash
./dotproduct_message 8 1024 0     # Message passing, H-tree
./dotproduct_shared 16 2048 1     # Shared memory, LIBCom
```

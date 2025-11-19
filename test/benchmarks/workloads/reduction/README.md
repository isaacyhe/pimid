# Tree Reduction Workload

Parallel tree reduction workload for PIM architectures.

## Description

Implements hierarchical tree reduction across multiple subarrays with two programming models:
- **Message Passing**: Explicit inter-subarray data transfers in a tree pattern
- **Shared Memory**: Remote memory accesses for reduction

## Building

```bash
make                # Build both versions
make message        # Build message passing version only  
make shared         # Build shared memory version only
```

## Running

```bash
./reduction_message <num_subarrays> <elements> <is_libcom>
./reduction_shared <num_subarrays> <elements> <is_libcom>
```

Parameters:
- `num_subarrays`: Number of PIM subarrays (8, 16, 32, etc.)
- `elements`: Number of elements per subarray
- `is_libcom`: 0 = H-tree baseline, 1 = LIBCom interconnect

## Examples

```bash
./reduction_message 8 1024 0    # Message passing, 8 subarrays, H-tree
./reduction_shared 16 2048 1    # Shared memory, 16 subarrays, LIBCom
```

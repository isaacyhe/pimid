# Prefix Sum (Parallel Scan) Workload

Parallel prefix sum workload for PIM architectures.

## Description

Computes parallel prefix sum (scan) with two programming models:
- **Message Passing**: Hierarchical scan with explicit transfers
- **Shared Memory**: Parallel scan with boundary propagation

## Building

```bash
make                # Build both versions
make message        # Build message passing version only
make shared         # Build shared memory version only
```

## Running

```bash
./prefixsum_message <num_subarrays> <array_size> <is_libcom>
./prefixsum_shared <num_subarrays> <array_size> <is_libcom>
```

Parameters:
- `num_subarrays`: Number of PIM subarrays
- `array_size`: Total array size
- `is_libcom`: 0 = H-tree baseline, 1 = LIBCom interconnect

## Examples

```bash
./prefixsum_message 8 1024 0     # Message passing, H-tree
./prefixsum_shared 16 2048 1     # Shared memory, LIBCom
```

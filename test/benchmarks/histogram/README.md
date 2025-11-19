# Histogram Workload

Parallel histogram computation workload for PIM architectures.

## Description

Computes histogram of data distribution with two programming models:
- **Message Passing**: Local histogram construction with tree merge
- **Shared Memory**: Atomic updates to shared histogram

## Building

```bash
make                # Build both versions
make message        # Build message passing version only
make shared         # Build shared memory version only
```

## Running

```bash
./histogram_message <num_subarrays> <data_size> <is_libcom>
./histogram_shared <num_subarrays> <data_size> <is_libcom>
```

Parameters:
- `num_subarrays`: Number of PIM subarrays
- `data_size`: Total number of data elements
- `is_libcom`: 0 = H-tree baseline, 1 = LIBCom interconnect

## Examples

```bash
./histogram_message 8 1024 0     # Message passing, H-tree
./histogram_shared 16 2048 1     # Shared memory, LIBCom
```

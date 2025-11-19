# 1D Stencil Workload

1D stencil computation workload for PIM architectures.

## Description

Iterative 1D stencil computation with two programming models:
- **Message Passing**: Boundary exchange with explicit transfers
- **Shared Memory**: Halo region access via remote reads

## Building

```bash
make                # Build both versions
make message        # Build message passing version only
make shared         # Build shared memory version only
```

## Running

```bash
./stencil1d_message <num_subarrays> <array_size> <is_libcom>
./stencil1d_shared <num_subarrays> <array_size> <is_libcom>
```

Parameters:
- `num_subarrays`: Number of PIM subarrays
- `array_size`: Total array size
- `is_libcom`: 0 = H-tree baseline, 1 = LIBCom interconnect

## Examples

```bash
./stencil1d_message 8 1024 0     # Message passing, H-tree
./stencil1d_shared 16 2048 1     # Shared memory, LIBCom
```

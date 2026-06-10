# zsim

A fast x86-64 microarchitectural simulator.

## Supported Features

**Core types:**
- Simple (IPC=1)
- Timing (detailed pipeline)
- OOO (out-of-order)

**Cache hierarchy:**
- Multi-level caches (L1, L2, L3)
- Set-associative, Z-array, ideal LRU
- Inclusive/exclusive policies
- Coherence (MESI)

**Memory:**
- Simple fixed-latency
- DDR3/DDR4 timing models
- DRAMSim2/Ramulator integration

**Workloads:**
- Single/multi-threaded (OpenMP)
- Multi-process
- MPI (MPICH)

**Pin versions:**
- Pin 2.14+ (legacy)
- Pin 4.x (native mode, requires `ZSIM_FORCE_POSIX_SHM=1`)

## Prerequisites

gcc >= 4.6, scons, Pin, libconfig, libhdf5, libelf

## Installation

```bash
export PINPATH=/path/to/pin
scons -j$(nproc)
```

## Usage

```bash
./build/opt/zsim tests/simple.cfg
```

## License

GPLv2

# MPICH + PIN/ZSim Setup Guide

## Problem

Ubuntu 24.04's MPICH package is **incompatible with PIN instrumentation** due to:

1. **PMIx mismatch**: MPICH is built with `--with-pmix` but hydra sends PMI1 environment variables
2. **ch4:ucx device**: Complex initialization that conflicts with PIN's process control

```
# Symptom: MPI_Comm_size returns 1 for all ranks
$ mpirun.mpich -np 4 ./test
Rank=0, Size=1  # Wrong! Should be Size=4
Rank=0, Size=1
Rank=0, Size=1
Rank=0, Size=1
```

## Solution: Build MPICH from Source

Build MPICH with `ch3:nemesis` device which uses simple PMI1 protocol.

### Quick Install

```bash
# Download
cd /tmp
wget https://www.mpich.org/static/downloads/4.2.0/mpich-4.2.0.tar.gz
tar xzf mpich-4.2.0.tar.gz
cd mpich-4.2.0

# Configure with PIN-compatible options
./configure --prefix=/opt/mpich \
    --with-device=ch3:nemesis \
    --with-pm=hydra \
    --disable-fortran \
    --enable-fast=O2

# Build and install
make -j$(nproc)
sudo make install

# Verify
/opt/mpich/bin/mpichversion | grep device
# Should show: ch3:nemesis
```

### Key Configuration Flags

| Flag | Purpose |
|------|---------|
| `--with-device=ch3:nemesis` | Use ch3 device with simple PMI1 (not ch4:ucx with PMIx) |
| `--with-pm=hydra` | Use hydra process manager |
| `--disable-fortran` | Skip Fortran bindings (faster build) |
| `--enable-fast=O2` | Optimize for performance |

### Verify Installation

```bash
# Test MPI communication works
cat > /tmp/test_mpi.c << 'EOF'
#include <stdio.h>
#include <mpi.h>
int main(int argc, char** argv) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    printf("Rank=%d, Size=%d\n", rank, size);
    MPI_Finalize();
    return 0;
}
EOF

/opt/mpich/bin/mpicc -o /tmp/test_mpi /tmp/test_mpi.c
/opt/mpich/bin/mpirun -np 4 /tmp/test_mpi

# Expected output:
# Rank=0, Size=4
# Rank=1, Size=4
# Rank=2, Size=4
# Rank=3, Size=4
```

## Running MPI under ZSim/PIN

### Recommended: Use zsim_mpi_run.sh

```bash
# Set environment
export PINPATH=/path/to/pin

# Run MPI workload (4 ranks)
./scripts/zsim_mpi_run.sh -np 4 -- /path/to/mpi_binary

# With custom output directory
./scripts/zsim_mpi_run.sh -np 4 -o /tmp/my_results -- /path/to/mpi_binary

# With custom config template
./scripts/zsim_mpi_run.sh -np 4 -cfg configs/mpi_template.cfg -- /path/to/mpi_binary

# Results are in:
#   /tmp/zsim_mpi_<pid>/rank0/zsim.out
#   /tmp/zsim_mpi_<pid>/rank1/zsim.out
#   ...
```

### Single Rank (Direct)

```bash
# Create config
cat > mpi_single.cfg << 'EOF'
sys = {
    cores = { simple = { type = "Simple"; cores = 1; icache = "l1i"; dcache = "l1d"; }; };
    lineSize = 64;
    caches = {
        l1d = { caches = 1; size = 32768; };
        l1i = { caches = 1; size = 32768; };
        l2 = { caches = 1; size = 1048576; children = "l1d|l1i"; };
    };
};
sim = { phaseLength = 10000; maxTotalInstrs = 500000000; };
process0 = { command = "/path/to/mpi_binary"; };
EOF

PINPATH=/path/to/pin ZSIM_FORCE_POSIX_SHM=1 ./build/opt/zsim mpi_single.cfg
```

### Multi-Rank (Wrapper Script)

Create wrapper script `/tmp/pin_mpi_wrapper.sh`:

```bash
#!/bin/bash
PINPATH=/path/to/pin
ZSIM_DIR=/path/to/zsim

RANK=${PMI_RANK:-0}
OUTDIR=/tmp/zsim_mpi_rank${RANK}
mkdir -p $OUTDIR
cd $OUTDIR

cat > zsim.cfg << EOF
sys = {
    cores = { simple = { type = "Simple"; cores = 1; icache = "l1i"; dcache = "l1d"; }; };
    lineSize = 64;
    caches = {
        l1d = { caches = 1; size = 32768; };
        l1i = { caches = 1; size = 32768; };
        l2 = { caches = 1; size = 1048576; children = "l1d|l1i"; };
    };
};
sim = { phaseLength = 10000; maxTotalInstrs = 500000000; };
process0 = { command = "$@"; };
EOF

export ZSIM_FORCE_POSIX_SHM=1
export PINPATH
exec ${ZSIM_DIR}/build/opt/zsim zsim.cfg
```

Run multi-rank:

```bash
chmod +x /tmp/pin_mpi_wrapper.sh
/opt/mpich/bin/mpirun -np 4 /tmp/pin_mpi_wrapper.sh /path/to/mpi_binary

# Results in /tmp/zsim_mpi_rank{0,1,2,3}/zsim.out
```

## Why ch3:nemesis Works with PIN

| Feature | ch3:nemesis | ch4:ucx (Ubuntu default) |
|---------|-------------|--------------------------|
| PMI Protocol | Simple PMI1 wire protocol | PMIx with callbacks |
| Process Startup | Direct fork, minimal setup | Complex multi-stage init |
| Signal Usage | Minimal | Heavy (progress engine) |
| Memory Setup | Basic TCP/shared mem | UCX multi-transport |
| PIN Compatibility | ✅ Good | ❌ Conflicts |

## Troubleshooting

### "Size=1" for all ranks
- Ubuntu's MPICH is broken, build from source with ch3:nemesis

### ZSim hangs during MPI_Init
- Ensure using MPICH built with ch3:nemesis, not ch4:ucx
- Check `mpichversion | grep device`

### Multiple ZSim instances conflict
- Use unique output directories per rank (wrapper script handles this)
- Set `ZSIM_FORCE_POSIX_SHM=1` to avoid SysV shm conflicts

## References

- MPICH Downloads: https://www.mpich.org/downloads/
- PIN User Guide: https://software.intel.com/sites/landingpage/pintool/docs/98749/Pin/html/
- ZSim: https://github.com/s5z/zsim

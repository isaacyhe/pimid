# PIMID Workloads

All PIMID benchmark workloads live here, organized by **where they run** and by
**how their parallel workers share data**.

```
workloads/
├── host/     # runs on host CPU cores (ooo/timing), main memory
├── device/   # runs on PIM device PEs (alu cores), near memory
└── cosim/    # a serial host offloads a parallel region to device PEs
```

Each folder provides every kernel in **three data-model variants**:

| variant | parallel workers share data via | models |
|---------|---------------------------------|--------|
| `serial` | (no parallelism — single worker) | baseline reference |
| `shared_memory` | one address space — workers read/write the **same buffers** (zero-copy) | a coherent, shared-memory machine |
| `message_passing` | **explicit copies** between disjoint buffers via a mailbox | a non-coherent / distributed machine |

> **Naming is by *data model*, not by API.** "shared_memory" and
> "message_passing" describe how data moves, not which threading library is
> used. PIMID models the *timing* of these two communication styles; it does
> not require (or fully implement) the OpenMP or MPI standards.

## How each variant is realized

The realization differs between `host`/`device` (standalone programs) and
`cosim` (host-offloads-to-device), because the offload model is single-process:

| folder | `shared_memory` | `message_passing` |
|--------|-----------------|-------------------|
| **host**, **device** | OpenMP threads over one address space (`#pragma omp`, `g++ -fopenmp`) | **forked ranks** + POSIX shm-mailbox transport (PIMID forks N QEMU children; `libpimid_mpi.so`), built with `mpicxx` |
| **cosim** | offload region spawns one pthread **per PE**, all reading/writing the host's buffers via shared pointers (zero-copy) | offload region spawns one pthread per PE in a **full-DMA** model: each PE `pimid_pe_recv`s its input chunk from host into a **private** buffer, computes on the copy, then `pimid_pe_send`s its result back — both transfers charged on the host↔device link |

**Why cosim/message_passing is in-process, not forked:** the host→device
offload (`pimid_offload_sync` + WORK_BEGIN/END domain switching) operates within
a single process. A forked MPI rank is a separate process with no host to
offload from, so forking is incompatible with the offload model. cosim therefore
expresses message-passing as explicit in-process **DMA copies** — `pimid_pe_recv`
/ `pimid_pe_send` in `cosim_pe_message.h`, each calling `zsim_work_begin_sized`
so the link's M/D/1 cost tracks the real payload — same *data model* (no shared
pointers; data moves by value across a costed link), different mechanism than the
forked host/device ranks.

> **Build note.** `host`/`device` `message_passing` variants need an MPI C++
> wrapper for `<mpi.h>`; the Makefile auto-discovers `mpicxx` (PATH, then the
> system MPICH at `/usr/lib64/mpich/bin`, then OpenMPI), and skips those targets
> with a note only if none is found. `serial`/`shared_memory` build with `g++`.
> The `cosim` variants need no MPI toolchain — all 15 build with `g++` alone
> (in-process pthreads + DMA).

## Running

- **host** / **device**: `--scope device`, select the data model with
  `--workload-type {serial|openmp|mpi}` (openmp→shared_memory binary,
  mpi→message_passing binary). host vs device is chosen by the config's core
  type + placement (`ooo_core`/`HOST_MC` vs `alu_core`/`BANK`).
- **cosim**: `--scope system` with a host+device config; the binary already
  encodes the device-side data model (the `_sm` / `_mp` suffix).

See the top-level `README.md` for the host↔device link model used by cosim.

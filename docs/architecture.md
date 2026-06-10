# Architecture

PIMID is an execution-driven, cycle-accurate simulator for Processing-in-Memory
architectures. A workload binary runs under QEMU user-mode emulation; a TCG
plugin (`libzsim_qemu.so`) intercepts every instruction and drives ZSim
microarchitectural models (cores, caches, memory controllers, NoC), which are
backed by Ramulator2, CACTI, NVSim, McPAT, and Garnet.

## Simulation flow (exec mode)

```
workload binary
   └─ QEMU user-mode (TCG)
        └─ libzsim_qemu.so plugin — every instruction/memory access
             └─ ZSim core model (alu / simple / in_order / ooo / null)
                  └─ caches → PE memory interface
                       ├─ NoC model (analytical | detailed Garnet)
                       └─ memory backend (Ramulator2 / CACTI / NVSim)
```

## Simulation methods

| Method | Description | Requires |
|---|---|---|
| `exec` | Execution-driven cycle-accurate (QEMU + ZSim) | plugin-capable `qemu-x86_64`, workload binary |
| `trace-gen` | Record an instruction trace for later replay | same as exec |
| `trace` | Replay a recorded trace through the models (no QEMU) | trace file |
| `synthetic` | Garnet synthetic traffic injection (no workload at all) | nothing |

## Scopes

- `scope: device` — a single PIM device: PEs in the memory hierarchy.
- `scope: system` — multi-node: hosts (with caches) + devices connected by a
  system interconnect; supports host->device offload ([cosim.md](cosim.md)).

## Two-Garnet hierarchy

The device-internal fabric and the host<->device system interconnect are two
separate Garnet instances; the memory hierarchy is their composition. The
device side of DRAM technologies is always a tree derived from the JEDEC
organization ([network.md](network.md)); the host network uses the classic
flat topologies.

## Execution models for parallel workloads

- **OpenMP**: one process; all threads share one simulator instance and one
  Garnet network (real cross-thread contention).
- **MPI**: PIMID forks N QEMU children with `libpimid_mpi.so` preloaded; ranks
  exchange data over a shared-memory mailbox (no external mpirun). Each rank
  simulates its own network instance.

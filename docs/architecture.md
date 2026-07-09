# Architecture

![PIMID system architecture](figures/PIMID_arch.png)

*(Regenerate with `python3 figures/gen_pimid_arch.py` -- PDF for the paper,
PNG for this page.)*

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

## Two-fabric system view (co-sim)

A co-sim system has **two fabrics joined by one bridge**:

```
        host cores                              device PEs
           |                                        |
      host crossbar          host<->device       device H-tree
   (analytic; 1-hop,        BRIDGE (protocol      (JEDEC-derived,
    contention at ports)  --  x phy; boundary  --  placement-driven
           |                traffic only)             tree)
       host main memory                          device memory tech
       (M/D/1, host-path adder)                  (PE-MI + Garnet)
```

- **Device H-tree** -- the device side of DRAM technologies is always a tree
  derived from the JEDEC organization ([network.md](network.md)). Every PE
  memory access is priced here (own unit = 0 hops; elsewhere = tree distance).
- **Host crossbar** -- classic flat topology; at 1 core it degenerates (no
  fabric), at multi-core it adds a fixed one-hop latency, with host memory
  bandwidth as an aggregate M/D/1 queue. Host core loads/stores are priced
  here (out-of-ROI code, and the whole kernel under NO_OFFLOAD baselines).
- **Bridge** -- the two-layer (`protocol` x `phy`) join. It carries ONLY
  boundary traffic: launch cmd/ack, Case-1 coherence flush, Case-2 DMA. It is
  where host-core traffic and device-PE traffic meet; neither fabric's ordinary
  traffic crosses it. The device-internal fabric and the host<->device
  interconnect remain two separate Garnet instances; the analytic host tier
  and bridge charges compose over them. See [cosim.md](cosim.md).

## Execution models for parallel workloads

- **OpenMP**: one process; all threads share one simulator instance and one
  Garnet network (real cross-thread contention).
- **MPI**: PIMID forks N QEMU children with `libpimid_mpi.so` preloaded; ranks
  exchange data over a shared-memory mailbox (no external mpirun). Each rank
  simulates its own network instance.

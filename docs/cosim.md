# Host-Device Co-simulation

Co-simulation (`scope: system`) composes the two models the simulator
already has -- **the host is the host, the device is the device** -- and
models only the interactions between them:

- **Out-of-ROI code executes on the host model** (OoO/in-order cores, the
  cache hierarchy, host memory through its own controller).
- **The ROI region executes on the device model**: PEs behind the device's
  own NoC in front of the device's own memory technology -- the *identical*
  code path a standalone `scope: device` simulation uses (same PE memory
  interfaces, same Garnet/analytical NoC, same JEDEC-derived memory timing).
  Threads spawned inside the region map onto distinct device PEs.
- **Boundary crossings are charged with the link + memory technology
  models.** A device with `attachment: internal` is the host's main memory
  (no outside transfer; data reorganization at the device's own bandwidth);
  any other attachment pays the configured link.

There are **no special co-sim workloads**: any ordinary workload runs in
co-sim unchanged. In a standalone device simulation, only the ROI (the
kernel) is counted; in co-sim, the host phases and boundary costs are added
around that same region:

```bash
./build/pimid --method exec --scope system \
    --config examples/cosim/host_device_basic.yaml \
    --workload benchmarks/pim_kernels/reduction/reduction_omp --size 4096 \
    --workload-type openmp
```

**Parity invariant** (enforced by validation): the device-side cycles of a
co-sim run agree with a standalone device-scope run of the same kernel.
If they ever diverge, a second device model has crept in -- file a bug.

## Interconnect link types (`system.network.links[].type`)

| type | base latency | bandwidth | models |
|---|---|---|---|
| `interposer` | ~5 ns | 256 GB/s | 2.5D on-package (HBM-on-interposer) |
| `cxl_3_0` / `cxl_2_0` | 100 / 200 ns | 126 / 63 GB/s | coherent expansion |
| `pcie_gen5` / `pcie_gen4` | 500 ns | 63 / 31.5 GB/s | discrete accelerator card |
| `nvlink_3_0` / `nvlink_4_0` / `nvlink_c2c` | 100-700 ns | 50-450 GB/s | proprietary fabrics |
| `ualink_1_0` | ~100 ns | ~450 GB/s | accelerator fabric |

Link latency/bandwidth/coherence drive both the inter-node hop latency and the
M/D/1 offload-transfer charge.

## Diagnostics

`PIMID_COSIM_TRACE=1` logs timestamped thread-lifecycle events (scheduler
start/join at thread birth, offload migrations) to stderr -- use it when
reporting a startup hang or scheduling anomaly.

**Coupled livelock with mismatched clocks: resolved in 1.2.0.** A `scope:
system` co-simulation with the host and device at different clocks (for
example host 2 GHz, device 500 MHz) could previously fail to terminate: the
device OpenMP team was sized to the host core count and oversubscribed the
device PEs, so the offload region never finished. 1.2.0 caps the team to the
device PE count. Host and device may run at independent clocks: as of 1.2.2
the device cycle count is independent of the host clock, so the host/device
clock difference shows up only as wall-clock time (cycles / frequency).
Device cycles for bandwidth-bound kernels scale with the device's own clock,
as expected.

## MPI semantics

`libpimid_mpi.so` provides the MPI ABI (Init, Send/Recv, Sendrecv, Bcast,
Reduce/Allreduce, Barrier, Scatter/Gather, Alltoall) over a POSIX
shared-memory mailbox. `MPI_PROC_NULL` send/recv are no-ops per the standard,
so halo-exchange patterns run without deadlock. Collectives are
**timing-faithful, not numerics-faithful**: reduce-style ops transmit all
chunks but do not apply the reduction operator — workloads needing actual
reduced values should compute them on the root from received chunks.

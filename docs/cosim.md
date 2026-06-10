# Host-Device Co-simulation

In `scope: system`, a host node (OoO/in-order core + caches) offloads an
annotated region to device PEs and reads results back.

- `pimid_offload_sync` switches the calling context into the device domain;
  threads spawned inside the region map onto distinct device PEs.
- Devices with `attachment: internal` share the host die (no link charge);
  any other attachment goes through the system interconnect.

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

## MPI semantics

`libpimid_mpi.so` provides the MPI ABI (Init, Send/Recv, Sendrecv, Bcast,
Reduce/Allreduce, Barrier, Scatter/Gather, Alltoall) over a POSIX
shared-memory mailbox. `MPI_PROC_NULL` send/recv are no-ops per the standard,
so halo-exchange patterns run without deadlock. Collectives are
**timing-faithful, not numerics-faithful**: reduce-style ops transmit all
chunks but do not apply the reduction operator — workloads needing actual
reduced values should compute them on the root from received chunks.

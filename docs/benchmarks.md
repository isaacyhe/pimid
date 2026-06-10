# Benchmarks

48 self-contained benchmarks across 7 suites, all with ROI markers
(`zsim_roi_begin/end`) and four input-size tiers (tiny/small/medium/large):

| Suite | Benchmarks | Parallelism |
|---|---|---|
| PIM kernels (8) | stream_triad, vector_add, gemv, spmv_csr, bfs, histogram, reduction, stencil_2d | serial + OpenMP + MPI |
| BabelStream (1) | babelstream | OpenMP |
| Rodinia (10) | hotspot, needle, pathfinder, srad, kmeans, lud, backprop, lavamd, particlefilter, myocyte | OpenMP |
| Classic (6) | dhrystone, whetstone, binary_search, quicksort, sha256, naive_matmul | serial |
| NPB (5) | IS, CG, EP, MG, FT | OpenMP |
| SPLASH-3 (12) | FFT, Radix, Barnes, Ocean, LU, Water-Nsq, Water-Sp, Cholesky, FMM, Radiosity, Raytrace, Volrend | pthreads |
| PARSEC (6) | blackscholes, canneal, streamcluster, swaptions, fluidanimate, freqmine | pthreads |

Additionally:
- `benchmarks/cosim/` — host-device co-simulation kernels (bfs_iterative,
  histogram_merge, reduction_tree, spmv_csr, vector_add) in serial /
  message-passing / shared-memory variants.
- `benchmarks/host/` — host-side variants of the core kernels.

```bash
make -C benchmarks all        # build everything
```

Per-benchmark run configs live in `examples/benchmarks/`.

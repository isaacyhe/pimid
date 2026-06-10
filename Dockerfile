# PIMID — fat distribution image (copy-prebuilt strategy)
#
# This image ships prebuilt PIMID binaries + QEMU 9.2 + plugins + example
# examples + benchmark workloads. Anyone can `docker pull` and use the
# simulator without setting up a build environment.
#
# Build:    docker build -t ghcr.io/isaacyhe/pimid:latest .
# Run:      docker run --rm -it ghcr.io/isaacyhe/pimid:latest
# Pull:     docker pull ghcr.io/isaacyhe/pimid:latest

FROM ubuntu:24.04

LABEL org.opencontainers.image.source="https://github.com/isaacyhe/pimid"
LABEL org.opencontainers.image.title="PIMID"
LABEL org.opencontainers.image.description="Full-System Simulator with Intricacy and Diversity for Processing-in-Memory"
LABEL org.opencontainers.image.licenses="GPL-2.0-or-later"

ENV DEBIAN_FRONTEND=noninteractive

# Runtime deps:
#   libyaml-cpp / libhdf5 / libboost-system / libgomp / libstdc++ — pimid links
#   libpixman / libglib2 — QEMU runtime
#   libmpich / mpich — MPI workloads (the MPI ABI shim libpimid_mpi.so still
#     needs libmpich for symbols workloads expect to resolve)
#   gcc / g++ / make / mpicc — for building user workloads inside the image
#   python3 — useful for parsing stats / running provided scripts
RUN apt-get update && apt-get install -y --no-install-recommends \
        libyaml-cpp0.8 \
        libhdf5-103-1t64 \
        libhdf5-hl-100t64 \
        libboost-system1.83.0 \
        libboost-filesystem1.83.0 \
        libconfig++9v5 \
        libcurl4t64 \
        libstdc++6 \
        libgomp1 \
        libpixman-1-0 \
        libglib2.0-0t64 \
        libcapstone4 \
        libmpich12 \
        mpich \
        gcc \
        g++ \
        make \
        python3 \
        python3-yaml \
        ca-certificates \
        less \
        vim-tiny \
    && rm -rf /var/lib/apt/lists/*

# PIMID binaries + libraries
RUN mkdir -p /opt/pimid/bin /opt/pimid/lib \
             /opt/external/qemu/build \
             /opt/build/external/zsim
COPY build/pimid                                /opt/pimid/bin/pimid
COPY build/libpimid_plugin.so                   /opt/pimid/lib/libpimid_plugin.so
COPY build/libpimid_mpi.so                      /opt/pimid/lib/libpimid_mpi.so
COPY external/ramulator/libramulator.so         /opt/pimid/lib/libramulator.so
# libpimid_mpi.so also at ${PIMID_ROOT}/build/ so findPimidMpiLib() finds it.
RUN mkdir -p /opt/pimid/build && \
    ln -s /opt/pimid/lib/libpimid_mpi.so /opt/build/libpimid_mpi.so
# Plugins must live at the same path pimid binary searches: ${PIMID_ROOT}/build/external/zsim
COPY build/external/zsim/libzsim_qemu.so        /opt/build/external/zsim/libzsim_qemu.so
COPY build/external/zsim/libpimid_trace.so      /opt/build/external/zsim/libpimid_trace.so
# QEMU at the path pimid binary searches: ${PIMID_ROOT}/external/qemu/build
COPY external/qemu/build/qemu-x86_64            /opt/external/qemu/build/qemu-x86_64
# QEMU needs its data files (bios, keymaps) shipped alongside the binary
COPY external/qemu/pc-bios/                     /opt/external/qemu/pc-bios/

# CACTI tech_params + NVSim sample cells. Baked CACTI_DATA_DIR / NVSim
# defaults reference the build-time absolute host path; we ship the data at
# /opt/external and symlink the baked host path to it for compat.
COPY external/cacti/tech_params/                /opt/external/cacti/tech_params/
COPY external/nvsim/sample_PCRAM.cell           /opt/external/nvsim/sample_PCRAM.cell
COPY external/nvsim/sample_RRAM.cell            /opt/external/nvsim/sample_RRAM.cell
COPY external/nvsim/sample_STTRAM.cell          /opt/external/nvsim/sample_STTRAM.cell
COPY external/nvsim/sample_STTRAM_aggressive.cell  /opt/external/nvsim/sample_STTRAM_aggressive.cell
COPY external/nvsim/sample_SLCNAND.cell         /opt/external/nvsim/sample_SLCNAND.cell
COPY external/nvsim/SRAM.cell                   /opt/external/nvsim/SRAM.cell
RUN mkdir -p /home/he/Workspace/pimid-dev/external && \
    ln -s /opt/external/cacti /home/he/Workspace/pimid-dev/external/cacti && \
    ln -s /opt/external/nvsim /home/he/Workspace/pimid-dev/external/nvsim

# Examples + workloads. Layout mirrors the repo (examples,
# benchmarks) so that example YAMLs with relative paths
# "benchmarks/..." resolve correctly when CWD is /opt/pimid.
COPY examples/                                  /opt/pimid/examples/
COPY benchmarks/                                /opt/benchmarks/

# Convenience symlinks at the top-level so users can find them without
# remembering the pimid/ prefix.
RUN ln -s examples /opt/pimid/configs && \
    ln -s benchmarks /opt/benchmarks && \
    ln -s benchmarks/cosim /opt/pimid/cosim_workloads

# In-image smoke harness (matches host harness, paths adjusted)
COPY docker/pimid_smoke.sh                            /opt/pimid/bin/pimid_smoke
COPY docker/entrypoint.sh                             /opt/pimid/bin/entrypoint
RUN chmod +x /opt/pimid/bin/pimid_smoke /opt/pimid/bin/entrypoint

# Docs
COPY README.md                                        /opt/pimid/README.md
COPY docs/                                            /opt/pimid/docs/

# Where PIMID expects to find its plugins/QEMU. main.cpp's findPimidMpiLib
# searches PIMID_ROOT-relative paths; pin PIMID_ROOT and PATH explicitly.
ENV PIMID_ROOT=/opt/pimid
ENV PATH=/opt/pimid/bin:$PATH
ENV LD_LIBRARY_PATH=/opt/pimid/lib:${LD_LIBRARY_PATH:-}

WORKDIR /opt/pimid

ENTRYPOINT ["/opt/pimid/bin/entrypoint"]
CMD ["bash"]

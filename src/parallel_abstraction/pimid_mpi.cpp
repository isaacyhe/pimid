/**
 * pimid_mpi.cpp — Multi-process MPI implementation for PIMID.
 *
 * Replaces the previous pthread-mailbox implementation. Each MPI rank now
 * runs as a separate process, with PIMID itself as the launcher (no mpirun
 * dependency). Inter-rank messaging uses POSIX shared memory + pshared
 * mutex/condvar, so workloads parallelize across host pCPUs (the previous
 * in-process design serialized under qemu-user's TCG core lock).
 *
 * Process-level rank assignment is via env vars set by the PIMID launcher:
 *   PIMID_MPI_RANKS  — total rank count (required, set by launcher)
 *   PIMID_MPI_RANK   — this process's rank id (0..N-1, set by launcher)
 *   PIMID_MPI_SHM    — name of the POSIX shm object backing the mailboxes
 *                      (optional; defaults to /pimid_mpi_<pid_of_rank0>)
 *
 * Workloads that never call MPI symbols simply ignore all of this — each
 * rank runs independently. Workloads that DO call MPI get a working
 * cross-process shm transport, with magic-op latency injection unchanged.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "parallel_abstraction/pimid_mpi.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <atomic>
#include <cstdint>
#include <cerrno>

/* ---- ZSim/QEMU magic ops (only effective under instrumentation) ---- */

#define COMPILER_BARRIER() { __asm__ __volatile__("" ::: "memory"); }

#define ZSIM_MAGIC_OP_MPI_REGISTER  2048
#define ZSIM_MAGIC_OP_MPI_SEND      2049
#define ZSIM_MAGIC_OP_MPI_RECV      2050
#define ZSIM_MAGIC_OP_MPI_BARRIER   2051

struct __attribute__((aligned(64))) PimidMpiParams {
    uint32_t src_pe;
    uint32_t dst_pe;
    uint64_t msg_size;
    uint64_t msg_id;
};

static thread_local PimidMpiParams tl_mpi_params;

static inline void zsim_magic_op(uint64_t op) {
#ifdef __x86_64__
    COMPILER_BARRIER();
    __asm__ __volatile__("xchg %%rcx, %%rcx;" : : "c"(op));
    COMPILER_BARRIER();
#else
    (void)op;
#endif
}

/* ---- Shared-memory mailbox layout ----
 *
 * Single POSIX shm object holds:
 *   SharedHeader (barrier state, init flags)
 *   Mailbox[N]   — one inbound queue per rank
 *
 * Each Mailbox is a fixed-size ring of MessageSlots, each MessageSlot
 * holding up to PIMID_MPI_MAX_MSG_BYTES of payload. Messages larger than
 * the slot are chunked (the workload sees a logical Send/Recv, but the
 * transport splits/joins on the wire). For typical PIM benchmarks msg
 * sizes are well under 64 KB; the ring depth tolerates burst traffic.
 */

#define PIMID_MPI_MAX_MSG_BYTES   (64 * 1024)   /* 64 KB per slot */
#define PIMID_MPI_RING_SLOTS      16            /* per mailbox */

struct MessageSlot {
    int       src;
    int       tag;
    uint32_t  size;       /* bytes used in payload */
    uint32_t  chunk_idx;  /* 0..n_chunks-1 */
    uint32_t  n_chunks;
    uint32_t  msg_id;
    char      payload[PIMID_MPI_MAX_MSG_BYTES];
};

struct Mailbox {
    pthread_mutex_t mu;
    pthread_cond_t  cv_nonempty;
    pthread_cond_t  cv_nonfull;
    uint32_t        head;          /* next slot to read */
    uint32_t        tail;          /* next slot to write */
    uint32_t        count;         /* slots currently filled */
    uint32_t        _pad;
    MessageSlot     slots[PIMID_MPI_RING_SLOTS];
};

struct SharedHeader {
    /* Barrier */
    pthread_mutex_t  barrier_mu;
    pthread_cond_t   barrier_cv;
    int              barrier_count;
    int              barrier_gen;
    int              nranks;
    int              _pad;
    /* Mailboxes follow: g_shared->mailboxes[0..nranks-1] */
    Mailbox          mailboxes[0];
};

/* ---- Process-local state ---- */

static int            g_rank = -1;
static int            g_nranks = 0;
static SharedHeader*  g_shared = nullptr;
static size_t         g_shared_bytes = 0;
static char           g_shm_name[256] = {0};
static int            g_shm_fd = -1;
static bool           g_we_created_shm = false;
static std::atomic<bool> g_initialized{false};
static std::atomic<int>  g_next_request{1};

/* ---- Helpers ---- */

static size_t dtype_size(MPI_Datatype dt) {
    switch (dt) {
        case MPI_CHAR: case MPI_BYTE: return 1;
        case MPI_INT: case MPI_UNSIGNED: case MPI_FLOAT: return 4;
        case MPI_DOUBLE: case MPI_LONG: case MPI_UNSIGNED_LONG:
        case MPI_LONG_LONG: return 8;
        default: return 4;
    }
}

static void inject_timing(int src_pe, int dst_pe, uint64_t msg_size) {
    tl_mpi_params.src_pe   = (uint32_t)src_pe;
    tl_mpi_params.dst_pe   = (uint32_t)dst_pe;
    tl_mpi_params.msg_size = msg_size;
    tl_mpi_params.msg_id++;
    zsim_magic_op(ZSIM_MAGIC_OP_MPI_SEND);
}

static void register_mpi_params(void) {
    /* Convey the address of this thread's params block in %rdx while the
     * REGISTER opcode goes in %rcx, so the simulator can read {src_pe, dst_pe,
     * msg_size} from it on every subsequent MPI_SEND/RECV (and charge the NoC).
     * %rdx is caller-clobbered, so this is safe. */
#ifdef __x86_64__
    void* pp = (void*)&tl_mpi_params;
    COMPILER_BARRIER();
    __asm__ __volatile__("xchg %%rcx, %%rcx;"
                         : : "c"((uint64_t)ZSIM_MAGIC_OP_MPI_REGISTER), "d"(pp)
                         : "memory");
    COMPILER_BARRIER();
#endif
}

static size_t shared_bytes_for(int nranks) {
    return sizeof(SharedHeader) + (size_t)nranks * sizeof(Mailbox);
}

static void init_pshared_mutex(pthread_mutex_t* m) {
    pthread_mutexattr_t a;
    pthread_mutexattr_init(&a);
    pthread_mutexattr_setpshared(&a, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(m, &a);
    pthread_mutexattr_destroy(&a);
}

static void init_pshared_cond(pthread_cond_t* c) {
    pthread_condattr_t a;
    pthread_condattr_init(&a);
    pthread_condattr_setpshared(&a, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(c, &a);
    pthread_condattr_destroy(&a);
}

/* Rank 0 creates and initializes the shm segment; other ranks attach.
 * Synchronization on attach: each non-zero rank busy-waits for the
 * "initialized" sentinel in the header (barrier_gen >= 0 after init,
 * we use barrier_gen=-1 to mean uninitialized) since they're spawned
 * concurrently and may race with rank 0. */
static bool open_or_create_shm(int rank, int nranks, const char* shm_name) {
    g_shared_bytes = shared_bytes_for(nranks);

    int oflag = (rank == 0) ? (O_CREAT | O_RDWR) : O_RDWR;
    int retries = (rank == 0) ? 1 : 200;  /* up to ~2s for rank0 to create */
    for (int i = 0; i < retries; i++) {
        g_shm_fd = shm_open(shm_name, oflag, 0600);
        if (g_shm_fd >= 0) break;
        if (rank == 0) {
            fprintf(stderr, "[pimid_mpi rank0] shm_open(%s) failed: %s\n",
                    shm_name, strerror(errno));
            return false;
        }
        usleep(10000);  /* 10ms */
    }
    if (g_shm_fd < 0) {
        fprintf(stderr, "[pimid_mpi rank%d] could not attach to %s after wait\n",
                rank, shm_name);
        return false;
    }

    if (rank == 0) {
        g_we_created_shm = true;
        if (ftruncate(g_shm_fd, (off_t)g_shared_bytes) < 0) {
            fprintf(stderr, "[pimid_mpi rank0] ftruncate failed: %s\n", strerror(errno));
            return false;
        }
    } else {
        /* Wait until rank 0 has finished ftruncating */
        struct stat st;
        for (int i = 0; i < 200; i++) {
            if (fstat(g_shm_fd, &st) == 0 && (size_t)st.st_size >= g_shared_bytes) break;
            usleep(10000);
        }
    }

    void* p = mmap(NULL, g_shared_bytes, PROT_READ | PROT_WRITE,
                   MAP_SHARED, g_shm_fd, 0);
    if (p == MAP_FAILED) {
        fprintf(stderr, "[pimid_mpi rank%d] mmap failed: %s\n", rank, strerror(errno));
        return false;
    }
    g_shared = (SharedHeader*)p;

    if (rank == 0) {
        /* Initialize pshared primitives + barrier state. */
        init_pshared_mutex(&g_shared->barrier_mu);
        init_pshared_cond(&g_shared->barrier_cv);
        g_shared->barrier_count = 0;
        g_shared->barrier_gen   = 0;
        g_shared->nranks        = nranks;
        for (int i = 0; i < nranks; i++) {
            Mailbox* mb = &g_shared->mailboxes[i];
            init_pshared_mutex(&mb->mu);
            init_pshared_cond(&mb->cv_nonempty);
            init_pshared_cond(&mb->cv_nonfull);
            mb->head = mb->tail = mb->count = 0;
        }
        /* Publish "ready" marker — non-zero ranks busy-wait on this. */
        __sync_synchronize();
        g_shared->barrier_gen = 1;   /* >0 == initialized */
    } else {
        for (int i = 0; i < 200; i++) {
            __sync_synchronize();
            if (g_shared->barrier_gen >= 1) break;
            usleep(10000);
        }
        if (g_shared->nranks != nranks) {
            fprintf(stderr, "[pimid_mpi rank%d] nranks mismatch: %d (env) vs %d (shm)\n",
                    rank, nranks, g_shared->nranks);
        }
    }
    return true;
}

/* ---- Send / Recv core ---- */

/* Push one chunk to the dest mailbox. Blocks if ring is full. */
static int send_chunk(int dest, int tag, const char* data, uint32_t size,
                      uint32_t chunk_idx, uint32_t n_chunks, uint32_t msg_id) {
    Mailbox* mb = &g_shared->mailboxes[dest];
    pthread_mutex_lock(&mb->mu);
    while (mb->count >= PIMID_MPI_RING_SLOTS) {
        pthread_cond_wait(&mb->cv_nonfull, &mb->mu);
    }
    MessageSlot* slot = &mb->slots[mb->tail];
    slot->src       = g_rank;
    slot->tag       = tag;
    slot->size      = size;
    slot->chunk_idx = chunk_idx;
    slot->n_chunks  = n_chunks;
    slot->msg_id    = msg_id;
    if (data && size > 0) memcpy(slot->payload, data, size);
    mb->tail = (mb->tail + 1) % PIMID_MPI_RING_SLOTS;
    mb->count++;
    pthread_cond_signal(&mb->cv_nonempty);
    pthread_mutex_unlock(&mb->mu);
    return 0;
}

/* Pop one message from the local mailbox. Blocks if empty. */
static void recv_slot(MessageSlot* out) {
    Mailbox* mb = &g_shared->mailboxes[g_rank];
    pthread_mutex_lock(&mb->mu);
    while (mb->count == 0) {
        pthread_cond_wait(&mb->cv_nonempty, &mb->mu);
    }
    *out = mb->slots[mb->head];
    mb->head = (mb->head + 1) % PIMID_MPI_RING_SLOTS;
    mb->count--;
    pthread_cond_signal(&mb->cv_nonfull);
    pthread_mutex_unlock(&mb->mu);
}

/* ---- MPI API ---- */

extern "C" {

int MPI_Init(int *argc, char ***argv) {
    (void)argc; (void)argv;

    if (g_initialized.exchange(true)) return MPI_SUCCESS;

    const char* env_n = getenv("PIMID_MPI_RANKS");
    const char* env_r = getenv("PIMID_MPI_RANK");
    g_nranks = env_n ? atoi(env_n) : 1;
    g_rank   = env_r ? atoi(env_r) : 0;
    if (g_nranks < 1) g_nranks = 1;
    if (g_rank < 0)   g_rank = 0;

    if (g_nranks == 1) {
        /* Solo run: no shm needed, MPI is a no-op transport. */
        register_mpi_params();
        return MPI_SUCCESS;
    }

    const char* shm_env = getenv("PIMID_MPI_SHM");
    if (shm_env && *shm_env) {
        snprintf(g_shm_name, sizeof(g_shm_name), "%s", shm_env);
    } else {
        /* Fallback: derive a name from PPID (the pimid launcher pid),
         * so siblings of the same launch agree on the same shm. */
        snprintf(g_shm_name, sizeof(g_shm_name), "/pimid_mpi_%d", (int)getppid());
    }

    if (!open_or_create_shm(g_rank, g_nranks, g_shm_name)) {
        return -1;
    }
    register_mpi_params();
    return MPI_SUCCESS;
}

int MPI_Finalize(void) {
    if (g_shared) {
        munmap(g_shared, g_shared_bytes);
        g_shared = nullptr;
    }
    if (g_shm_fd >= 0) {
        close(g_shm_fd);
        g_shm_fd = -1;
    }
    /* Rank 0 also unlinks the shm name so it doesn't outlive the launch. */
    if (g_we_created_shm && g_shm_name[0]) {
        shm_unlink(g_shm_name);
    }
    g_initialized.store(false);
    return MPI_SUCCESS;
}

int MPI_Comm_rank(MPI_Comm comm, int *rank) {
    (void)comm; *rank = g_rank;
    return MPI_SUCCESS;
}

int MPI_Comm_size(MPI_Comm comm, int *size) {
    (void)comm; *size = g_nranks;
    return MPI_SUCCESS;
}

/* ---- Point-to-point ---- */

int MPI_Send(const void *buf, int count, MPI_Datatype datatype,
             int dest, int tag, MPI_Comm comm) {
    (void)comm;
    if (g_nranks <= 1) return MPI_SUCCESS;
    if (dest < 0 || dest >= g_nranks) return MPI_SUCCESS;

    size_t nbytes = (size_t)count * dtype_size(datatype);
    const char* p = (const char*)buf;

    if (nbytes == 0) {
        send_chunk(dest, tag, nullptr, 0, 0, 1, (uint32_t)tl_mpi_params.msg_id + 1);
    } else {
        uint32_t n_chunks = (uint32_t)((nbytes + PIMID_MPI_MAX_MSG_BYTES - 1)
                                       / PIMID_MPI_MAX_MSG_BYTES);
        uint32_t msg_id = (uint32_t)tl_mpi_params.msg_id + 1;
        size_t left = nbytes;
        for (uint32_t i = 0; i < n_chunks; i++) {
            uint32_t take = (uint32_t)((left < PIMID_MPI_MAX_MSG_BYTES) ? left : PIMID_MPI_MAX_MSG_BYTES);
            send_chunk(dest, tag, p, take, i, n_chunks, msg_id);
            p += take; left -= take;
        }
    }

    inject_timing(g_rank, dest, nbytes);
    return MPI_SUCCESS;
}

int MPI_Recv(void *buf, int count, MPI_Datatype datatype,
             int source, int tag, MPI_Comm comm, MPI_Status *status) {
    (void)comm; (void)status; (void)source; (void)tag;
    if (g_nranks <= 1) return MPI_SUCCESS;
    /* MPI_PROC_NULL (-1) recv is a no-op in real MPI; without this guard an
     * edge rank (e.g. stencil halo exchange) blocks forever on a mailbox no
     * rank will ever send to. Mirrors the dest guard in MPI_Send. */
    if (source < 0 || source >= g_nranks) return MPI_SUCCESS;

    size_t nbytes = (size_t)count * dtype_size(datatype);
    char* p = (char*)buf;
    size_t written = 0;
    int last_src = -1;

    /* Receive until we've assembled a complete message (1+ chunks). For
     * the FIFO-matching policy this implementation has always used, we
     * receive chunks of the next message in order. */
    MessageSlot slot;
    do {
        recv_slot(&slot);
        if (last_src < 0) last_src = slot.src;
        size_t space = (nbytes > written) ? (nbytes - written) : 0;
        size_t take = (slot.size < space) ? slot.size : space;
        if (p && take > 0) memcpy(p + written, slot.payload, take);
        written += take;
    } while (slot.chunk_idx + 1 < slot.n_chunks);

    inject_timing(last_src, g_rank, written);
    return MPI_SUCCESS;
}

int MPI_Sendrecv(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                 int dest, int sendtag,
                 void *recvbuf, int recvcount, MPI_Datatype recvtype,
                 int source, int recvtag,
                 MPI_Comm comm, MPI_Status *status) {
    MPI_Send(sendbuf, sendcount, sendtype, dest, sendtag, comm);
    MPI_Recv(recvbuf, recvcount, recvtype, source, recvtag, comm, status);
    return MPI_SUCCESS;
}

/* ---- Non-blocking (synchronous semantics — shm is fast enough) ---- */

int MPI_Isend(const void *buf, int count, MPI_Datatype datatype,
              int dest, int tag, MPI_Comm comm, MPI_Request *request) {
    MPI_Send(buf, count, datatype, dest, tag, comm);
    *request = g_next_request.fetch_add(1);
    return MPI_SUCCESS;
}

int MPI_Irecv(void *buf, int count, MPI_Datatype datatype,
              int source, int tag, MPI_Comm comm, MPI_Request *request) {
    MPI_Recv(buf, count, datatype, source, tag, comm, MPI_STATUS_IGNORE);
    *request = g_next_request.fetch_add(1);
    return MPI_SUCCESS;
}

int MPI_Wait(MPI_Request *request, MPI_Status *status) {
    (void)request; (void)status;
    return MPI_SUCCESS;
}

int MPI_Waitall(int count, MPI_Request requests[], MPI_Status statuses[]) {
    (void)count; (void)requests; (void)statuses;
    return MPI_SUCCESS;
}

/* ---- Collectives ---- */

int MPI_Barrier(MPI_Comm comm) {
    (void)comm;
    if (g_nranks <= 1) {
        zsim_magic_op(ZSIM_MAGIC_OP_MPI_BARRIER);
        return MPI_SUCCESS;
    }

    pthread_mutex_lock(&g_shared->barrier_mu);
    int gen = g_shared->barrier_gen;
    g_shared->barrier_count++;
    if (g_shared->barrier_count >= g_nranks) {
        g_shared->barrier_count = 0;
        g_shared->barrier_gen++;
        pthread_cond_broadcast(&g_shared->barrier_cv);
    } else {
        while (g_shared->barrier_gen == gen) {
            pthread_cond_wait(&g_shared->barrier_cv, &g_shared->barrier_mu);
        }
    }
    pthread_mutex_unlock(&g_shared->barrier_mu);

    zsim_magic_op(ZSIM_MAGIC_OP_MPI_BARRIER);
    return MPI_SUCCESS;
}

int MPI_Bcast(void *buffer, int count, MPI_Datatype datatype,
              int root, MPI_Comm comm) {
    size_t nbytes = (size_t)count * dtype_size(datatype);
    if (g_rank == root) {
        for (int i = 0; i < g_nranks; i++) {
            if (i != root) MPI_Send(buffer, count, datatype, i, 0, comm);
        }
    } else {
        MPI_Recv(buffer, count, datatype, root, 0, comm, MPI_STATUS_IGNORE);
    }
    (void)nbytes;
    return MPI_SUCCESS;
}

int MPI_Reduce(const void *sendbuf, void *recvbuf, int count,
               MPI_Datatype datatype, MPI_Op op, int root, MPI_Comm comm) {
    (void)op;
    size_t nbytes = (size_t)count * dtype_size(datatype);

    if (g_rank != root) {
        MPI_Send(sendbuf, count, datatype, root, 0, comm);
    } else {
        if (sendbuf && recvbuf) memcpy(recvbuf, sendbuf, nbytes);
        /* Receive from each non-root rank in turn. Reduction op not applied
         * — this is a timing-accurate simulation, not a numerics oracle.
         * Workloads that need actual reduction values should compute them
         * on rank 0 from the received chunks. */
        char* tmp = (char*)malloc(nbytes ? nbytes : 1);
        for (int i = 0; i < g_nranks; i++) {
            if (i != root) {
                MPI_Recv(tmp, count, datatype, i, 0, comm, MPI_STATUS_IGNORE);
            }
        }
        free(tmp);
    }
    return MPI_SUCCESS;
}

int MPI_Allreduce(const void *sendbuf, void *recvbuf, int count,
                  MPI_Datatype datatype, MPI_Op op, MPI_Comm comm) {
    MPI_Reduce(sendbuf, recvbuf, count, datatype, op, 0, comm);
    MPI_Bcast(recvbuf, count, datatype, 0, comm);
    return MPI_SUCCESS;
}

int MPI_Scatter(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                void *recvbuf, int recvcount, MPI_Datatype recvtype,
                int root, MPI_Comm comm) {
    size_t send_sz = (size_t)sendcount * dtype_size(sendtype);
    if (g_rank == root) {
        const char* sbuf = (const char*)sendbuf;
        for (int i = 0; i < g_nranks; i++) {
            if (i == root) {
                if (recvbuf && sbuf) memcpy(recvbuf, sbuf + (size_t)i * send_sz, send_sz);
            } else {
                MPI_Send(sbuf + (size_t)i * send_sz, sendcount, sendtype, i, 0, comm);
            }
        }
    } else {
        MPI_Recv(recvbuf, recvcount, recvtype, root, 0, comm, MPI_STATUS_IGNORE);
    }
    return MPI_SUCCESS;
}

int MPI_Gather(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
               void *recvbuf, int recvcount, MPI_Datatype recvtype,
               int root, MPI_Comm comm) {
    size_t recv_sz = (size_t)recvcount * dtype_size(recvtype);

    if (g_rank != root) {
        MPI_Send(sendbuf, sendcount, sendtype, root, 0, comm);
    } else {
        char* rbuf = (char*)recvbuf;
        if (sendbuf && rbuf) {
            size_t send_sz = (size_t)sendcount * dtype_size(sendtype);
            memcpy(rbuf + (size_t)root * recv_sz, sendbuf, send_sz);
        }
        for (int i = 0; i < g_nranks; i++) {
            if (i != root) {
                MPI_Recv(rbuf + (size_t)i * recv_sz, recvcount, recvtype,
                         i, 0, comm, MPI_STATUS_IGNORE);
            }
        }
    }
    return MPI_SUCCESS;
}

int MPI_Alltoall(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                 void *recvbuf, int recvcount, MPI_Datatype recvtype,
                 MPI_Comm comm) {
    size_t send_sz = (size_t)sendcount * dtype_size(sendtype);
    size_t recv_sz = (size_t)recvcount * dtype_size(recvtype);
    const char* sbuf = (const char*)sendbuf;
    char* rbuf = (char*)recvbuf;

    for (int i = 0; i < g_nranks; i++) {
        if (i == g_rank) {
            if (sbuf && rbuf) {
                memcpy(rbuf + (size_t)i * recv_sz,
                       sbuf + (size_t)i * send_sz, send_sz);
            }
        } else {
            MPI_Send(sbuf + (size_t)i * send_sz, sendcount, sendtype, i, 0, comm);
        }
    }
    for (int i = 0; i < g_nranks; i++) {
        if (i != g_rank) {
            MPI_Recv(rbuf + (size_t)i * recv_sz, recvcount, recvtype,
                     i, 0, comm, MPI_STATUS_IGNORE);
        }
    }
    return MPI_SUCCESS;
}

/* ---- Utility ---- */

int MPI_Type_size(MPI_Datatype datatype, int *size) {
    *size = (int)dtype_size(datatype);
    return MPI_SUCCESS;
}

double MPI_Wtime(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

}  /* extern "C" */

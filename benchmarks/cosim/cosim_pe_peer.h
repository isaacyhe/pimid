/**
 * @file cosim_pe_peer.h
 * @brief Peer message-passing for cosim device PEs: PEs exchange messages with
 *        EACH OTHER over the device NoC (charged on Garnet), not via the host.
 *
 * Data model:
 *   - shared_memory : one memory space seen by host and device; PEs read/write
 *                     shared buffers (zero-copy). See cosim_pe_parallel.h.
 *   - message_passing (THIS): the device PEs are peers that send/recv messages
 *                     to one another; the host only launches the region and
 *                     collects the final result. Each peer message is issued via
 *                     zsim_mpi_send/recv, which the simulator charges on the
 *                     device-internal Garnet NoC (PE-to-PE routing + contention),
 *                     scaled by the message size.
 *
 * Mechanism: each PE runs on its own pthread inside the offload region (so it
 * maps to a distinct device PE). Each PE-thread registers a thread-local params
 * block once, then every peer message stamps {src_pe, dst_pe, msg_size} into
 * that block and fires the MPI_SEND magic op. The actual bytes still move in
 * shared process memory (single-process offload), but the *timing* is the
 * NoC cost of a src->dst transfer of msg_size bytes.
 */
#ifndef PIMID_COSIM_PE_PEER_H
#define PIMID_COSIM_PE_PEER_H

#include <pthread.h>
#include <vector>
#include <cstdint>

/* Magic ops — must match the plugin / zsim_hooks. */
#ifndef ZSIM_MAGIC_OP_MPI_REGISTER
#define ZSIM_MAGIC_OP_MPI_REGISTER  2048
#define ZSIM_MAGIC_OP_MPI_SEND      2049
#define ZSIM_MAGIC_OP_MPI_RECV      2050
#define ZSIM_MAGIC_OP_MPI_BARRIER   2051
#endif

/* Per-thread params block the plugin reads on MPI_REGISTER. Layout must match
 * the plugin's MpiParamsBlock {src_pe, dst_pe, msg_size, msg_id}. */
struct __attribute__((aligned(64))) PimidPeerParams {
    uint32_t src_pe;
    uint32_t dst_pe;
    uint64_t msg_size;
    uint64_t msg_id;
};
static __thread PimidPeerParams tl_peer_params;

static inline void pimid_peer_magic(uint64_t op) {
#ifdef __x86_64__
    __asm__ __volatile__("" ::: "memory");
    __asm__ __volatile__("xchg %%rcx, %%rcx;" : : "c"(op));
    __asm__ __volatile__("" ::: "memory");
#else
    (void)op;
#endif
}

/* Call once per PE-thread, before any peer message. Registers this thread's
 * thread-local params block with the simulator: the block address goes in %rdx
 * while the REGISTER opcode goes in %rcx, so subsequent SEND/RECV are charged on
 * the device NoC from {src_pe, dst_pe, msg_size}. */
static inline void pimid_peer_register(void) {
#ifdef __x86_64__
    void* pp = (void*)&tl_peer_params;
    __asm__ __volatile__("" ::: "memory");
    __asm__ __volatile__("xchg %%rcx, %%rcx;"
                         : : "c"((uint64_t)ZSIM_MAGIC_OP_MPI_REGISTER), "d"(pp)
                         : "memory");
    __asm__ __volatile__("" ::: "memory");
#endif
}

/* Peer send: model a src_pe -> dst_pe transfer of nbytes over the device NoC.
 * The byte copy itself (shared process memory) is the caller's responsibility;
 * this charges the NoC timing for the message. */
static inline void pimid_peer_send(int src_pe, int dst_pe, uint64_t nbytes) {
    tl_peer_params.src_pe   = (uint32_t)src_pe;
    tl_peer_params.dst_pe   = (uint32_t)dst_pe;
    tl_peer_params.msg_size = nbytes;
    tl_peer_params.msg_id++;
    pimid_peer_magic(ZSIM_MAGIC_OP_MPI_SEND);
}

static inline void pimid_peer_recv(int src_pe, int dst_pe, uint64_t nbytes) {
    tl_peer_params.src_pe   = (uint32_t)src_pe;
    tl_peer_params.dst_pe   = (uint32_t)dst_pe;
    tl_peer_params.msg_size = nbytes;
    tl_peer_params.msg_id++;
    pimid_peer_magic(ZSIM_MAGIC_OP_MPI_RECV);
}

/* One pthread per PE; each calls body(pe_id). body should pimid_peer_register()
 * first, then drive peer messages. Joins all before returning to the host. */
template <typename F>
struct PimidPeerArg { F* body; int pe_id; };

template <typename F>
static void* pimid_peer_trampoline(void* a) {
    PimidPeerArg<F>* arg = static_cast<PimidPeerArg<F>*>(a);
    (*arg->body)(arg->pe_id);
    return nullptr;
}

template <typename F>
static void pimid_parallel_pes_peer(int num_pes, F body) {
    if (num_pes <= 0) return;
    std::vector<pthread_t> tids(num_pes);
    std::vector<PimidPeerArg<F> > args(num_pes);
    for (int i = 0; i < num_pes; i++) {
        args[i].body = &body;
        args[i].pe_id = i;
        pthread_create(&tids[i], nullptr, pimid_peer_trampoline<F>, &args[i]);
    }
    for (int i = 0; i < num_pes; i++) pthread_join(tids[i], nullptr);
}

#endif /* PIMID_COSIM_PE_PEER_H */

/**
 * @file cosim_pe_message.h
 * @brief Message-passing data model for cosim device PEs.
 *
 * Companion to cosim_pe_parallel.h (the shared-memory model). Both spawn one
 * pthread per device PE inside the offload region (so each PE accrues its own
 * cycles), but they differ in HOW a PE's result reaches the host:
 *
 *   - shared_memory  : the PE writes directly into a host-owned buffer through a
 *                      shared pointer (zero-copy; coherent machine).
 *   - message_passing: the PE computes into a PRIVATE buffer, then explicitly
 *                      *copies* its bytes back to the host through a mailbox.
 *                      The copy is announced to the simulator with
 *                      zsim_work_begin_sized(nbytes) so the host<->device link's
 *                      M/D/1 transfer cost reflects the actual payload, modeling
 *                      a non-coherent device that must DMA results out.
 *
 * This is in-process (pthreads), not forked MPI ranks: the offload model is
 * single-process, so message passing is expressed as explicit value copies
 * rather than separate address spaces. The DATA MODEL (no shared pointers; data
 * moves by value with a sized transfer cost) is what "message_passing" denotes.
 */
#ifndef PIMID_COSIM_PE_MESSAGE_H
#define PIMID_COSIM_PE_MESSAGE_H

#include <pthread.h>
#include <vector>
#include <cstring>
#include "zsim_hooks.h"

template <typename F>
struct PimidMsgArg {
    F* body;
    int pe_id;
};

template <typename F>
static void* pimid_msg_trampoline(void* a) {
    PimidMsgArg<F>* arg = static_cast<PimidMsgArg<F>*>(a);
    (*arg->body)(arg->pe_id);
    return nullptr;
}

/**
 * Run body(pe_id) on one pthread per PE. body should compute into a PRIVATE
 * local and then call pimid_pe_send(dst, src, nbytes) to transfer its result
 * back to the host buffer with a modeled link cost.
 */
template <typename F>
static void pimid_parallel_pes_msg(int num_pes, F body) {
    if (num_pes <= 0) return;
    std::vector<pthread_t> tids(num_pes);
    std::vector<PimidMsgArg<F> > args(num_pes);
    for (int i = 0; i < num_pes; i++) {
        args[i].body = &body;
        args[i].pe_id = i;
        pthread_create(&tids[i], nullptr, pimid_msg_trampoline<F>, &args[i]);
    }
    for (int i = 0; i < num_pes; i++) {
        pthread_join(tids[i], nullptr);
    }
}

/**
 * Explicit device->host message: copy `nbytes` from a PE-private source into a
 * host-visible destination, charging the host<->device link for the transfer.
 * Use this instead of writing through a shared pointer in the message-passing
 * model.
 */
static inline void pimid_pe_send(void* dst, const void* src, unsigned nbytes) {
    zsim_work_begin_sized(nbytes);   /* model the sized device->host transfer */
    std::memcpy(dst, src, nbytes);
}

/**
 * Explicit host->device message: copy `nbytes` of input from host memory into a
 * PE-private buffer, charging the host<->device link. Use at the start of a PE's
 * work in the full-DMA message-passing model so the device computes on its OWN
 * copy of the input rather than dereferencing host pointers.
 */
static inline void pimid_pe_recv(void* dst, const void* src, unsigned nbytes) {
    zsim_work_begin_sized(nbytes);   /* model the sized host->device transfer */
    std::memcpy(dst, src, nbytes);
}

#endif /* PIMID_COSIM_PE_MESSAGE_H */

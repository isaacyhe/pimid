/**
 * @file cosim_pe_parallel.h
 * @brief Run a device offload region's per-PE work on one pthread per PE.
 *
 * In the PIMID host/device co-simulation harness the offloaded kernel runs
 * inside pimid_offload_sync(), which switches the calling thread into
 * DOMAIN_DEVICE. Any pthread it spawns there is born DOMAIN_DEVICE too (see
 * the plugin's vcpu_init_cb / g_in_device_region), so each PE thread is
 * scheduled onto a distinct device PE and accrues its own cycles. A plain
 * serial `for` loop would attribute every PE's work to device PE-0.
 *
 * Usage inside a device_kernel():
 *     pimid_parallel_pes(shared->num_device_pes, [&](int i) {
 *         DeviceWorker pe(i, *shared);
 *         pe.doWork();
 *     });
 */
#ifndef PIMID_COSIM_PE_PARALLEL_H
#define PIMID_COSIM_PE_PARALLEL_H

#include <pthread.h>
#include <vector>

template <typename F>
struct PimidPEArg {
    F* body;
    int pe_id;
};

template <typename F>
static void* pimid_pe_trampoline(void* a) {
    PimidPEArg<F>* arg = static_cast<PimidPEArg<F>*>(a);
    (*arg->body)(arg->pe_id);
    return nullptr;
}

// Spawn one pthread per PE; each calls body(pe_id). Joins all before return,
// preserving the synchronous-offload contract (host resumes after all PEs).
template <typename F>
static void pimid_parallel_pes(int num_pes, F body) {
    if (num_pes <= 0) return;
    std::vector<pthread_t> tids(num_pes);
    std::vector<PimidPEArg<F> > args(num_pes);
    for (int i = 0; i < num_pes; i++) {
        args[i].body = &body;
        args[i].pe_id = i;
        pthread_create(&tids[i], nullptr, pimid_pe_trampoline<F>, &args[i]);
    }
    for (int i = 0; i < num_pes; i++) {
        pthread_join(tids[i], nullptr);
    }
}

#endif /* PIMID_COSIM_PE_PARALLEL_H */

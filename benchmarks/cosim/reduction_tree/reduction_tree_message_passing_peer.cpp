/**
 * @file reduction_tree_message_passing_peer.cpp
 * @brief Cosim reduction where device PEs message EACH OTHER (peer-to-peer) to
 *        combine their partials, then PE 0 hands the host the final result.
 *
 * Fundamental: message_passing = messages exchanged between PEs. Each PE reduces
 * its own chunk, then a pairwise reduction TREE runs ACROSS the PEs: in round r,
 * PE (i + 2^r) sends its running partial to PE i, which combines it. After
 * log2(N) rounds PE 0 holds the global result. Every inter-PE message is charged
 * on the device NoC via pimid_peer_send/recv. The host is not in the data path.
 */
#include <iostream>
#include <vector>
#include <cstdlib>
#include <cmath>
#include "zsim_hooks.h"
#include "../cosim_pe_peer.h"

enum ReductionOp { SUM, MAX, MIN, PRODUCT };

struct SharedReductionData {
    double* data;
    int data_size;
    int num_device_pes;
    double* partials;        // per-PE running partial (shared buffer for the bytes)
    double final_result;
    ReductionOp operation;
    pthread_barrier_t round_barrier;
};

static double combine(ReductionOp op, double a, double b) {
    switch (op) {
        case SUM:     return a + b;
        case MAX:     return a > b ? a : b;
        case MIN:     return a < b ? a : b;
        default:      return a * b;
    }
}

static void device_kernel(void* raw) {
    SharedReductionData* s = (SharedReductionData*)raw;
    int n = s->num_device_pes;

    pimid_parallel_pes_peer(n, [&](int pe) {
        pimid_peer_register();

        // 1. local reduction of this PE's chunk
        int chunk = s->data_size / n;
        int start = pe * chunk;
        int end   = (pe == n - 1) ? s->data_size : (pe + 1) * chunk;
        double acc;
        switch (s->operation) {
            case SUM:     acc = 0.0; break;
            case MAX:
            case MIN:     acc = s->data[start]; break;
            default:      acc = 1.0; break;
        }
        for (int i = start; i < end; i++) acc = combine(s->operation, acc, s->data[i]);
        s->partials[pe] = acc;

        // 2. pairwise reduction TREE across PEs (inter-PE messages over the NoC)
        for (int stride = 1; stride < n; stride <<= 1) {
            pthread_barrier_wait(&s->round_barrier);
            if (pe % (stride << 1) == 0) {
                int peer = pe + stride;
                if (peer < n) {
                    // peer sends its partial to `pe`; charge a src->dst NoC message
                    pimid_peer_recv(peer, pe, sizeof(double));   // I receive from peer
                    s->partials[pe] = combine(s->operation, s->partials[pe], s->partials[peer]);
                    if (pe == 0)
                        std::cout << "[DEVICE] round stride=" << stride
                                  << ": PE0 <- PE" << peer << std::endl;
                }
            } else if (pe % stride == 0) {
                int dst = pe - stride;
                pimid_peer_send(pe, dst, sizeof(double));        // I send to dst
            }
            pthread_barrier_wait(&s->round_barrier);
        }
        if (pe == 0) s->final_result = s->partials[0];
    });
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0]
                  << " <array_size> <num_device_pes> <operation>" << std::endl;
        std::cerr << "  operation: 0=SUM, 1=MAX, 2=MIN, 3=PRODUCT" << std::endl;
        return 1;
    }
    int data_size = std::atoi(argv[1]);
    int n         = std::atoi(argv[2]);
    int op        = std::atoi(argv[3]);
    if (op < 0 || op > 3) op = 0;

    std::cout << "=== HOST/DEVICE CO-SIM (peer message-passing): Reduction ==="
              << std::endl;
    std::cout << "Array size: " << data_size << "  Device PEs: " << n
              << "  Operation: " << (op==0?"SUM":op==1?"MAX":op==2?"MIN":"PRODUCT")
              << std::endl << std::endl;

    SharedReductionData s;
    s.data_size = data_size;
    s.num_device_pes = n;
    s.operation = static_cast<ReductionOp>(op);
    s.data = new double[data_size];
    s.partials = new double[n];
    for (int i = 0; i < data_size; i++) s.data[i] = (rand() % 100) / 10.0;
    pthread_barrier_init(&s.round_barrier, nullptr, n);

    std::cout << "--- OFFLOADING: PEs reduce + exchange partials peer-to-peer ---\n"
              << std::endl;
    pimid_offload_sync(device_kernel, &s);

    // host only collects the final result
    double expected;
    switch (op) {
        case SUM:  expected = 0.0; for (int i=0;i<data_size;i++) expected += s.data[i]; break;
        case MAX:  expected = s.data[0]; for (int i=1;i<data_size;i++) if(s.data[i]>expected) expected=s.data[i]; break;
        case MIN:  expected = s.data[0]; for (int i=1;i<data_size;i++) if(s.data[i]<expected) expected=s.data[i]; break;
        default:   expected = 1.0; for (int i=0;i<data_size;i++) expected *= s.data[i]; break;
    }
    std::cout << "\n[HOST] Final result: " << s.final_result
              << "  Expected: " << expected << std::endl;
    if (std::abs(s.final_result - expected) < 0.001)
        std::cout << "[HOST] Result verified!" << std::endl;
    else
        std::cout << "[HOST] Verification failed!" << std::endl;

    pthread_barrier_destroy(&s.round_barrier);
    delete[] s.data; delete[] s.partials;
    std::cout << "\n=== Co-Simulation Complete ===" << std::endl;
    return 0;
}

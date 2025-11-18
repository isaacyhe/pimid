/**
 * @file network_contention.h
 * @brief Network contention model for H-tree and LIBCom with Virtual Channel support
 *
 * Models:
 * - H-tree: Hierarchical network with N-1 switches for N subarrays
 * - LIBCom: Direct crossbar (minimal contention)
 * - Virtual Channels: 1, 2, or 4 VCs per physical link
 *
 * Key effects modeled:
 * 1. Link contention (multiple transfers sharing same link)
 * 2. Head-of-line blocking (without VCs)
 * 3. VC benefit: Independent buffers reduce blocking
 */

#ifndef DAC26_NETWORK_CONTENTION_H
#define DAC26_NETWORK_CONTENTION_H

#include <vector>
#include <queue>
#include <map>
#include <set>
#include <algorithm>
#include <cmath>
#include <cassert>
#include <cstdint>
#include <iostream>

namespace dac26 {

/**
 * Network transfer request
 */
struct Transfer {
    int src_subarray;
    int dst_subarray;
    int data_bytes;
    int start_cycle;
    int vc;  // Virtual channel assigned to this transfer

    // H-tree path (list of switch IDs this transfer uses)
    std::vector<int> switch_path;
};

/**
 * Virtual Channel state
 */
struct VirtualChannel {
    bool busy;
    std::queue<Transfer> buffer;
    int credits;  // Flow control credits

    VirtualChannel(int buffer_depth = 4)
        : busy(false), credits(buffer_depth) {}
};

/**
 * Physical link state (contains multiple VCs)
 */
struct PhysicalLink {
    int link_id;
    int num_vcs;
    std::vector<VirtualChannel> vcs;
    int link_width_bytes;

    PhysicalLink(int id, int vcs, int width)
        : link_id(id), num_vcs(vcs), link_width_bytes(width) {
        this->vcs.resize(vcs);
    }

    bool hasAvailableVC() const {
        for (const auto& vc : vcs) {
            if (!vc.busy && vc.credits > 0) return true;
        }
        return false;
    }

    int allocateVC() {
        for (int i = 0; i < num_vcs; i++) {
            if (!vcs[i].busy && vcs[i].credits > 0) {
                vcs[i].busy = true;
                return i;
            }
        }
        return -1;  // No VC available
    }
};

/**
 * Network contention simulator
 */
class NetworkContentionModel {
public:
    enum class Topology {
        H_TREE_BUS_ONLY,  // Traditional H-tree: all transfers through central port (no switches)
        H_TREE,           // H-tree with switches at each node
        LIBCOM            // Direct crossbar with LIBCom switches
    };

    NetworkContentionModel(int num_subarrays, int num_vcs, Topology topo)
        : num_subarrays_(num_subarrays),
          num_vcs_(num_vcs),
          topology_(topo),
          current_cycle_(0) {

        initializeNetwork();
    }

    /**
     * Submit a transfer request
     * Returns: Transfer latency (in cycles)
     */
    int submitTransfer(int src, int dst, int data_bytes) {
        assert(src >= 0 && src < num_subarrays_);
        assert(dst >= 0 && dst < num_subarrays_);
        assert(src != dst);

        Transfer t;
        t.src_subarray = src;
        t.dst_subarray = dst;
        t.data_bytes = data_bytes;
        t.start_cycle = current_cycle_;
        t.vc = 0;  // Initialize VC

        // Compute path and latency based on topology
        int latency = computeTransferLatency(t);

        // Track for contention analysis
        pending_transfers_.push_back(t);

        // Return latency (not absolute completion cycle)
        return latency;
    }

    /**
     * Advance simulation by one cycle
     */
    void tick() {
        current_cycle_++;

        // Clean up completed transfers
        auto it = pending_transfers_.begin();
        while (it != pending_transfers_.end()) {
            if (current_cycle_ >= it->start_cycle +
                computeTransferLatency(*it)) {
                it = pending_transfers_.erase(it);
            } else {
                ++it;
            }
        }
    }

    /**
     * Get current contention level (for metrics)
     */
    double getCurrentContention() const {
        if (topology_ == Topology::LIBCOM) {
            return 0.0;  // Direct crossbar, no contention
        }

        // For H-tree, count concurrent transfers per switch
        std::map<int, int> switch_usage;

        for (const auto& t : pending_transfers_) {
            for (int sw : t.switch_path) {
                switch_usage[sw]++;
            }
        }

        // Average contention across all switches
        double total_contention = 0.0;
        for (const auto& pair : switch_usage) {
            total_contention += pair.second;
        }

        int num_switches = 0;
        if (topology_ == Topology::H_TREE) {
            num_switches = num_subarrays_ - 1;
        } else if (topology_ == Topology::H_TREE_BUS_ONLY) {
            num_switches = 1;  // Central port only
        }

        return num_switches > 0 ? total_contention / num_switches : 0.0;
    }

    /**
     * Get statistics
     */
    struct Stats {
        uint64_t total_transfers;
        uint64_t blocked_cycles;  // Cycles lost to head-of-line blocking
        double avg_contention;
        uint64_t total_cycles;
    };

    Stats getStats() const {
        Stats s;
        s.total_transfers = total_transfers_;
        s.blocked_cycles = blocked_cycles_;
        s.avg_contention = total_transfers_ > 0 ?
                          (double)total_contention_sum_ / total_transfers_ : 0.0;
        s.total_cycles = current_cycle_;
        return s;
    }

    int getCurrentCycle() const { return current_cycle_; }

private:
    void initializeNetwork() {
        if (topology_ == Topology::H_TREE) {
            // Create N-1 switches for N subarrays
            int num_switches = num_subarrays_ - 1;

            for (int i = 0; i < num_switches; i++) {
                switches_.emplace_back(i, num_vcs_, 128);  // 128-byte links
            }
        }
    }

    /**
     * Compute H-tree switch path for a transfer
     */
    std::vector<int> computeHTreePath(int src, int dst) {
        std::vector<int> path;

        // For H-tree, find common ancestor in binary tree
        // Switches are numbered in breadth-first order
        int tree_height = static_cast<int>(std::ceil(std::log2(num_subarrays_)));

        // Simplified: Use XOR distance to estimate path length
        int distance = src ^ dst;
        int hops = 0;
        while (distance > 0) {
            hops++;
            distance >>= 1;
        }

        // Path goes through 'hops' switches
        // Simplified: Just return number of switches
        for (int i = 0; i < hops; i++) {
            path.push_back(i);  // Actual switch IDs would be computed differently
        }

        return path;
    }

    /**
     * Compute transfer latency accounting for VCs and contention
     */
    int computeTransferLatency(Transfer& t) {
        if (topology_ == Topology::LIBCOM) {
            // Direct crossbar: 1 cycle switch latency
            return 1;
        }

        int tree_height = static_cast<int>(std::ceil(std::log2(num_subarrays_)));

        if (topology_ == Topology::H_TREE_BUS_ONLY) {
            // Traditional H-tree: All transfers must go through central port
            // Path: src → up to root → down to dst
            // Latency: 2 × tree_height + 2 (subarray accesses)
            int base_latency = 2 * tree_height + 2;

            // ALL transfers go through the central port (massive contention)
            // Mark this with a special switch path to the root
            t.switch_path = {0};  // Central port bottleneck

            // Check for contention at central port
            int contention_penalty = computeContentionPenalty(t);

            total_transfers_++;
            total_contention_sum_ += contention_penalty;
            blocked_cycles_ += contention_penalty;

            return base_latency + contention_penalty;
        }

        // H-tree with switches: Base latency = log2(N) + 2 (subarray accesses)
        int base_latency = tree_height + 2;

        // Compute switch path
        t.switch_path = computeHTreePath(t.src_subarray, t.dst_subarray);

        // Check for contention with pending transfers
        int contention_penalty = computeContentionPenalty(t);

        total_transfers_++;
        total_contention_sum_ += contention_penalty;
        blocked_cycles_ += contention_penalty;

        return base_latency + contention_penalty;
    }

    /**
     * Compute contention penalty based on VCs
     *
     * Key insight:
     * - With 1 VC: Full head-of-line blocking (serialization)
     * - With 2 VCs: 50% reduction in blocking (2 concurrent streams)
     * - With 4 VCs: 75% reduction in blocking (4 concurrent streams)
     */
    int computeContentionPenalty(const Transfer& new_transfer) {
        int conflicts = 0;

        // Count how many pending transfers share links
        for (const auto& pending : pending_transfers_) {
            // Check if paths overlap
            for (int sw : new_transfer.switch_path) {
                if (std::find(pending.switch_path.begin(),
                             pending.switch_path.end(), sw) !=
                    pending.switch_path.end()) {
                    conflicts++;
                    break;  // Count each transfer once
                }
            }
        }

        if (conflicts == 0) return 0;

        // Contention penalty depends on VC count
        // With K VCs, we can handle K concurrent transfers
        // Penalty = max(0, conflicts - num_vcs) serialization cycles

        int penalty = 0;
        if (conflicts > num_vcs_) {
            // Some transfers must wait
            penalty = (conflicts - num_vcs_) * 2;  // 2 cycles per blocked transfer
        } else {
            // All transfers can proceed concurrently via different VCs
            penalty = 0;
        }

        return penalty;
    }

    int num_subarrays_;
    int num_vcs_;
    Topology topology_;
    int current_cycle_;

    std::vector<PhysicalLink> switches_;
    std::vector<Transfer> pending_transfers_;

    // Statistics
    uint64_t total_transfers_ = 0;
    uint64_t blocked_cycles_ = 0;
    uint64_t total_contention_sum_ = 0;
};

} // namespace dac26

#endif // DAC26_NETWORK_CONTENTION_H

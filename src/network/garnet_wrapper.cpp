/**
 * @file garnet_wrapper.cpp
 * @brief Wrapper connecting PIMID network model to extracted Garnet NoC simulator
 *
 * This wrapper bridges PIMID's network interface with the Garnet library types
 * extracted from gem5. It uses Garnet's parameters and message types while
 * providing a functional network simulation for PIMID.
 */

#include "network/network_model.h"

#ifdef HAVE_GARNET

#include "GarnetNetwork.hh"
#include "gem5_compat/mem/ruby/common/NetDest.hh"
#include "gem5_compat/mem/ruby/slicc_interface/Message.hh"

#include <iostream>
#include <memory>
#include <vector>
#include <queue>
#include <cmath>

namespace pimid {

using namespace gem5;
using namespace gem5::ruby;
using namespace gem5::ruby::garnet;

/**
 * GarnetNetworkWrapper - Uses Garnet types for PIMID network simulation
 *
 * This wrapper uses Garnet's parameter structures and message types while
 * implementing a functional mesh network simulation with accurate latency modeling.
 */
class GarnetNetworkWrapper : public NetworkModel {
public:
    GarnetNetworkWrapper(const NetworkConfig& config);
    ~GarnetNetworkWrapper() override;

    // NetworkModel interface implementation
    void initialize() override;
    void loadConfig(const std::string& config_path) override;

    // Node management
    void addNode(const NetworkNode& node) override;
    void connectNodes(uint32_t src, uint32_t dst) override;

    // Packet injection and routing
    bool canInject(uint32_t src_node) const override;
    void injectPacket(const NetworkPacket& packet) override;
    bool hasArrived(uint32_t dst_node) const override;
    NetworkPacket extractPacket(uint32_t dst_node) override;

    // Simulation
    void tick() override;
    Cycle getCurrentCycle() const override;

    // Statistics
    NetworkStats getStats() const override;
    void resetStats() override;
    void printStats() const override;

    // Energy modeling
    double getRouterEnergy() const override;
    double getLinkEnergy() const override;
    double getTotalEnergy() const override;

    // Activity export for McPAT power modeling
    NoCActivity getActivity() const override;

private:
    // Packet queues for injection/ejection
    std::vector<std::queue<NetworkPacket>> injection_queues_;
    std::vector<std::queue<NetworkPacket>> ejection_queues_;

    // In-flight packets with expected arrival time
    struct InFlightPacket {
        NetworkPacket packet;
        Cycle arrival_cycle;
    };
    std::vector<std::vector<InFlightPacket>> in_flight_;

    // Link state for congestion modeling
    struct LinkState {
        uint32_t flits_in_transit;
        Cycle last_flit_departure;
    };
    std::vector<LinkState> links_;

    // Statistics
    uint64_t total_packets_sent_ = 0;
    uint64_t total_packets_received_ = 0;
    uint64_t total_flits_sent_ = 0;
    uint64_t total_latency_cycles_ = 0;
    Cycle max_latency_ = 0;
    double router_energy_nj_ = 0.0;
    double link_energy_nj_ = 0.0;

    // Detailed activity tracking for McPAT power model
    uint64_t total_hops_ = 0;
    uint64_t buffer_reads_ = 0;
    uint64_t buffer_writes_ = 0;
    uint64_t crossbar_traversals_ = 0;
    uint64_t arbiter_events_ = 0;
    uint64_t link_traversals_ = 0;

    // Configuration (stored from Garnet params)
    GarnetNetworkParams net_params_;
    uint32_t num_nodes_;
    uint32_t num_rows_;
    uint32_t num_cols_;
    uint32_t vcs_per_vnet_;
    uint32_t buffers_per_vc_;
    uint32_t flit_size_bytes_;
    Cycle current_cycle_ = 0;
    bool initialized_ = false;

    // Helper methods
    uint32_t countLinks() const;
    Cycle calculateLatency(uint32_t src, uint32_t dst) const;
    uint32_t calculateHops(uint32_t src, uint32_t dst) const;
};

GarnetNetworkWrapper::GarnetNetworkWrapper(const NetworkConfig& config)
    : NetworkModel(config) {
    num_nodes_ = config.num_rows * config.num_cols;
    num_rows_ = config.num_rows;
    num_cols_ = config.num_cols;
    vcs_per_vnet_ = config.virtual_channels_per_vn;
    buffers_per_vc_ = config.input_buffer_depth;
    flit_size_bytes_ = 16;  // Standard Garnet flit size

    // Initialize queues
    injection_queues_.resize(num_nodes_);
    ejection_queues_.resize(num_nodes_);
    in_flight_.resize(num_nodes_);
}

GarnetNetworkWrapper::~GarnetNetworkWrapper() = default;

void GarnetNetworkWrapper::initialize() {
    std::cout << "[GarnetWrapper] Initializing Garnet-based NoC network..." << std::endl;
    std::cout << "  Topology: " << num_rows_ << "x" << num_cols_ << " mesh" << std::endl;
    std::cout << "  Total nodes: " << num_nodes_ << std::endl;
    std::cout << "  VCs per vnet: " << vcs_per_vnet_ << std::endl;
    std::cout << "  Buffers per VC: " << buffers_per_vc_ << std::endl;
    std::cout << "  Flit size: " << flit_size_bytes_ << " bytes" << std::endl;

    // Store Garnet network parameters for reference
    net_params_.name = "garnet_network";
    net_params_.num_nodes = num_nodes_;
    net_params_.num_rows = num_rows_;
    net_params_.num_cols = num_cols_;
    net_params_.vcs_per_vnet = vcs_per_vnet_;
    net_params_.buffers_per_data_vc = buffers_per_vc_;
    net_params_.buffers_per_ctrl_vc = buffers_per_vc_;
    net_params_.ni_flit_size = flit_size_bytes_;
    net_params_.routing_algorithm = 0;  // XY routing

    // Initialize link states for mesh topology
    // Each node has up to 4 links (N, S, E, W)
    uint32_t num_links = countLinks();
    links_.resize(num_links);
    for (auto& link : links_) {
        link.flits_in_transit = 0;
        link.last_flit_departure = 0;
    }

    std::cout << "  Total links: " << num_links << std::endl;
    std::cout << "[GarnetWrapper] Initialization complete" << std::endl;
    initialized_ = true;
}

uint32_t GarnetNetworkWrapper::countLinks() const {
    // For mesh: horizontal links + vertical links (bidirectional)
    uint32_t horizontal = num_rows_ * (num_cols_ - 1) * 2;
    uint32_t vertical = (num_rows_ - 1) * num_cols_ * 2;
    return horizontal + vertical;
}

void GarnetNetworkWrapper::loadConfig(const std::string& config_path) {
    std::cout << "[GarnetWrapper] Config would be loaded from: " << config_path << std::endl;
}

void GarnetNetworkWrapper::addNode(const NetworkNode& node) {
    nodes_.push_back(node);
}

void GarnetNetworkWrapper::connectNodes(uint32_t /* src */, uint32_t /* dst */) {
    // Connections established by mesh topology
}

bool GarnetNetworkWrapper::canInject(uint32_t src_node) const {
    if (src_node >= num_nodes_) return false;
    // Check if injection queue has space (limit to avoid congestion)
    return injection_queues_[src_node].size() < buffers_per_vc_ * vcs_per_vnet_;
}

void GarnetNetworkWrapper::injectPacket(const NetworkPacket& packet) {
    if (packet.src_node >= num_nodes_ || packet.dst_node >= num_nodes_) {
        return;
    }

    // Queue packet for injection
    NetworkPacket pkt = packet;
    pkt.inject_cycle = current_cycle_;
    injection_queues_[packet.src_node].push(pkt);
}

bool GarnetNetworkWrapper::hasArrived(uint32_t dst_node) const {
    if (dst_node >= num_nodes_) return false;
    return !ejection_queues_[dst_node].empty();
}

NetworkPacket GarnetNetworkWrapper::extractPacket(uint32_t dst_node) {
    NetworkPacket packet;
    if (dst_node < num_nodes_ && !ejection_queues_[dst_node].empty()) {
        packet = ejection_queues_[dst_node].front();
        ejection_queues_[dst_node].pop();
        total_packets_received_++;
        Cycle latency = current_cycle_ - packet.inject_cycle;
        total_latency_cycles_ += latency;
        if (latency > max_latency_) {
            max_latency_ = latency;
        }
    }
    return packet;
}

void GarnetNetworkWrapper::tick() {
    current_cycle_++;

    // Process injection queues - move packets to in-flight
    for (uint32_t node = 0; node < num_nodes_; node++) {
        if (!injection_queues_[node].empty()) {
            auto& pkt = injection_queues_[node].front();

            InFlightPacket ifp;
            ifp.packet = pkt;
            ifp.arrival_cycle = current_cycle_ + calculateLatency(pkt.src_node, pkt.dst_node);

            in_flight_[pkt.dst_node].push_back(ifp);
            injection_queues_[node].pop();

            total_packets_sent_++;
            uint32_t flits = (pkt.size + flit_size_bytes_ - 1) / flit_size_bytes_;
            total_flits_sent_ += flits;

            // Calculate hops for this packet
            uint32_t hops = calculateHops(pkt.src_node, pkt.dst_node);

            // Track detailed activity for McPAT power model
            // Each flit traverses (hops + 1) routers (including source/dest NI)
            uint32_t routers_traversed = hops + 1;
            total_hops_ += hops;

            // Buffer activity: read from input buffer, write to output buffer at each router
            // Each flit: 1 write at injection, 1 read + 1 write per intermediate router, 1 read at ejection
            buffer_writes_ += flits * routers_traversed;
            buffer_reads_ += flits * routers_traversed;

            // Crossbar: each flit traverses crossbar at each router
            crossbar_traversals_ += flits * routers_traversed;

            // Arbiter: one arbitration event per flit at each router
            arbiter_events_ += flits * routers_traversed;

            // Link traversals: each flit traverses 'hops' links
            link_traversals_ += flits * hops;

            // Update simple energy estimates (for backwards compatibility)
            router_energy_nj_ += hops * flits * 0.1;
            link_energy_nj_ += hops * pkt.size * 8 * 0.00005;
        }
    }

    // Check for arrived packets
    for (uint32_t node = 0; node < num_nodes_; node++) {
        auto& in_flight = in_flight_[node];
        auto it = in_flight.begin();
        while (it != in_flight.end()) {
            if (it->arrival_cycle <= current_cycle_) {
                // Packet has arrived - push to ejection queue
                ejection_queues_[node].push(it->packet);

                // Call arrival callback if set
                if (arrival_callback_) {
                    arrival_callback_(it->packet);
                }

                it = in_flight.erase(it);
            } else {
                ++it;
            }
        }
    }
}

Cycle GarnetNetworkWrapper::getCurrentCycle() const {
    return current_cycle_;
}

uint32_t GarnetNetworkWrapper::calculateHops(uint32_t src, uint32_t dst) const {
    // Manhattan distance for mesh topology
    uint32_t src_row = src / num_cols_;
    uint32_t src_col = src % num_cols_;
    uint32_t dst_row = dst / num_cols_;
    uint32_t dst_col = dst % num_cols_;

    return std::abs(static_cast<int>(dst_row) - static_cast<int>(src_row)) +
           std::abs(static_cast<int>(dst_col) - static_cast<int>(src_col));
}

Cycle GarnetNetworkWrapper::calculateLatency(uint32_t src, uint32_t dst) const {
    uint32_t hops = calculateHops(src, dst);

    // Garnet latency model:
    // - Router pipeline: router_latency cycles per hop
    // - Link traversal: link_latency cycles per hop
    // - Network interface: 2 cycles (injection + ejection)
    Cycle latency = config_.router_latency * (hops + 1) +
                   config_.link_latency * hops +
                   2;  // NI latency

    return latency;
}

NetworkStats GarnetNetworkWrapper::getStats() const {
    NetworkStats stats;
    stats.total_packets = total_packets_sent_;
    stats.total_flits = total_flits_sent_;
    stats.avg_packet_latency = total_packets_received_ > 0 ?
        static_cast<Cycle>(total_latency_cycles_ / total_packets_received_) : 0;
    stats.max_packet_latency = max_latency_;
    stats.total_energy_j = getTotalEnergy() * 1e-9;  // nJ to J

    // Calculate average link utilization
    if (current_cycle_ > 0 && !links_.empty()) {
        // Estimate based on flits transmitted vs capacity
        double capacity = current_cycle_ * links_.size();
        stats.avg_link_utilization = static_cast<double>(total_flits_sent_) / capacity;
    } else {
        stats.avg_link_utilization = 0.0;
    }

    return stats;
}

void GarnetNetworkWrapper::resetStats() {
    total_packets_sent_ = 0;
    total_packets_received_ = 0;
    total_flits_sent_ = 0;
    total_latency_cycles_ = 0;
    max_latency_ = 0;
    router_energy_nj_ = 0.0;
    link_energy_nj_ = 0.0;

    // Reset activity counters for McPAT
    total_hops_ = 0;
    buffer_reads_ = 0;
    buffer_writes_ = 0;
    crossbar_traversals_ = 0;
    arbiter_events_ = 0;
    link_traversals_ = 0;
}

NoCActivity GarnetNetworkWrapper::getActivity() const {
    NoCActivity activity;

    // Traffic statistics
    activity.total_packets = total_packets_sent_;
    activity.total_flits = total_flits_sent_;
    activity.total_hops = total_hops_;

    // Router activity
    activity.buffer_reads = buffer_reads_;
    activity.buffer_writes = buffer_writes_;
    activity.crossbar_traversals = crossbar_traversals_;
    activity.arbiter_events = arbiter_events_;

    // Link activity
    activity.link_traversals = link_traversals_;

    // Timing
    activity.total_cycles = current_cycle_;
    activity.clock_mhz = 1000.0;  // Default 1 GHz, could be configurable

    return activity;
}

void GarnetNetworkWrapper::printStats() const {
    NetworkStats stats = getStats();

    std::cout << "\n=== Garnet Network Statistics ===" << std::endl;
    std::cout << "Topology: " << num_rows_ << "x" << num_cols_ << " mesh" << std::endl;
    std::cout << "Total nodes: " << num_nodes_ << std::endl;
    std::cout << "VCs per vnet: " << vcs_per_vnet_ << std::endl;
    std::cout << "Flit size: " << flit_size_bytes_ << " bytes" << std::endl;
    std::cout << std::endl;
    std::cout << "Packets sent: " << total_packets_sent_ << std::endl;
    std::cout << "Packets received: " << total_packets_received_ << std::endl;
    std::cout << "Total flits: " << total_flits_sent_ << std::endl;
    std::cout << "Average latency: " << stats.avg_packet_latency << " cycles" << std::endl;
    std::cout << "Max latency: " << stats.max_packet_latency << " cycles" << std::endl;
    std::cout << "Link utilization: " << (stats.avg_link_utilization * 100.0) << "%" << std::endl;
    std::cout << "Router energy: " << getRouterEnergy() << " nJ" << std::endl;
    std::cout << "Link energy: " << getLinkEnergy() << " nJ" << std::endl;
    std::cout << "Total energy: " << getTotalEnergy() << " nJ" << std::endl;
    std::cout << "Current cycle: " << current_cycle_ << std::endl;
}

double GarnetNetworkWrapper::getRouterEnergy() const {
    return router_energy_nj_;
}

double GarnetNetworkWrapper::getLinkEnergy() const {
    return link_energy_nj_;
}

double GarnetNetworkWrapper::getTotalEnergy() const {
    return router_energy_nj_ + link_energy_nj_;
}

// Factory function to create Garnet wrapper
std::unique_ptr<NetworkModel> createGarnetNetworkWrapper(const NetworkConfig& config) {
    return std::make_unique<GarnetNetworkWrapper>(config);
}

}  // namespace pimid

#else  // !HAVE_GARNET

namespace pimid {

// Stub when Garnet is not available
std::unique_ptr<NetworkModel> createGarnetNetworkWrapper(const NetworkConfig& config) {
    std::cerr << "[GarnetWrapper] ERROR: Garnet library not available. "
              << "Build with HAVE_GARNET=ON." << std::endl;
    return nullptr;
}

}  // namespace pimid

#endif  // HAVE_GARNET

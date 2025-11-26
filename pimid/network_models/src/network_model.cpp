#include "network_model.h"
#include <iostream>

namespace pimid {

//=============================================================================
// NetworkModel Base Implementation
//=============================================================================

NetworkModel::NetworkModel(const NetworkConfig& config)
    : config_(config), arrival_callback_(nullptr) {}

//=============================================================================
// GarnetModel Implementation
//=============================================================================

GarnetModel::GarnetModel(const NetworkConfig& config)
    : NetworkModel(config),
      garnet_instance_(nullptr),
      current_cycle_(0),
      router_energy_(0.0),
      link_energy_(0.0) {

    std::cout << "Creating GARNET network model with topology: "
              << static_cast<int>(config.topology) << std::endl;
}

GarnetModel::~GarnetModel() {
    // Clean up GARNET instance if needed
    if (garnet_instance_) {
        // Cleanup GARNET-related data structures
        std::cout << "[GarnetModel] Cleaning up GARNET instance" << std::endl;

        // Clear pending packets
        for (auto& queue_pair : injection_queues_) {
            while (!queue_pair.second.empty()) {
                queue_pair.second.pop();
            }
        }
        for (auto& queue_pair : ejection_queues_) {
            while (!queue_pair.second.empty()) {
                queue_pair.second.pop();
            }
        }

        // In full GARNET integration, would delete GARNET routers, links, etc.
        garnet_instance_ = nullptr;
    }
}

void GarnetModel::initialize() {
    std::cout << "Initializing GARNET network model..." << std::endl;

    // Initialize topology based on configuration
    switch (config_.topology) {
        case NetworkTopology::MESH_2D:
            std::cout << "  Creating 2D Mesh: " << config_.num_rows
                      << "x" << config_.num_cols << std::endl;
            // Auto-create nodes for mesh topology
            {
                uint32_t total_nodes = config_.num_rows * config_.num_cols;
                for (uint32_t i = 0; i < total_nodes; i++) {
                    NetworkNode node(i, PEPlacementLevel::LOGIC_DIE, i);
                    addNode(node);
                }
            }
            break;

        case NetworkTopology::MESH_3D:
            std::cout << "  Creating 3D Mesh: " << config_.num_rows
                      << "x" << config_.num_cols << "x" << config_.num_layers
                      << std::endl;
            break;

        case NetworkTopology::TORUS_2D:
            std::cout << "  Creating 2D Torus: " << config_.num_rows
                      << "x" << config_.num_cols << std::endl;
            break;

        case NetworkTopology::H_TREE:
            std::cout << "  Creating H-Tree for DRAM internal network" << std::endl;
            std::cout << "  Leaf nodes: " << config_.num_rows << std::endl;
            // H-tree nodes will be created dynamically based on DRAM hierarchy
            break;

        case NetworkTopology::FAT_TREE:
            std::cout << "  Creating Fat-Tree network" << std::endl;
            std::cout << "  Leaf nodes: " << config_.num_rows << std::endl;
            break;

        case NetworkTopology::CROSSBAR:
            std::cout << "  Creating Crossbar network" << std::endl;
            break;

        case NetworkTopology::DRAGONFLY:
            std::cout << "  Creating Dragonfly network" << std::endl;
            break;

        default:
            std::cout << "  Unknown topology!" << std::endl;
            break;
    }

    // Initialize GARNET parameters
    std::cout << "  Virtual channels: " << config_.virtual_channels << std::endl;
    std::cout << "  Link width: " << config_.link_width_bytes << " bytes" << std::endl;
    std::cout << "  Router latency: " << config_.router_latency << " cycles" << std::endl;

    std::cout << "GARNET network initialized with " << nodes_.size() << " nodes" << std::endl;
}

void GarnetModel::loadConfig(const std::string& config_path) {
    std::cout << "Loading GARNET configuration from: " << config_path << std::endl;

    // TODO: Load GARNET-specific configuration
    // This would parse gem5 config files or YAML config
}

void GarnetModel::addNode(const NetworkNode& node) {
    nodes_.push_back(node);

    // Initialize injection and ejection queues for this node
    injection_queues_[node.node_id] = std::queue<NetworkPacket>();
    ejection_queues_[node.node_id] = std::queue<NetworkPacket>();

    std::cout << "Added network node: " << node.node_id
              << " (level: " << static_cast<int>(node.level)
              << ", hierarchy_id: " << node.hierarchy_id << ")" << std::endl;
}

void GarnetModel::connectNodes(uint32_t src, uint32_t dst) {
    // Find source node and add connection
    for (auto& node : nodes_) {
        if (node.node_id == src) {
            node.connected_nodes.push_back(dst);
            std::cout << "Connected node " << src << " -> " << dst << std::endl;
            break;
        }
    }

    // Create actual GARNET link between nodes
    // In full GARNET integration, this would:
    // 1. Instantiate GARNET NetworkLink object
    // 2. Configure link with width, latency, energy params
    // 3. Connect routers at src and dst nodes
    // 4. Initialize credit counters for flow control
    //
    // For now, connection info is stored in node.connected_nodes
    // and used during routing in tick()

    // Create bidirectional link if topology requires it
    if (config_.topology == NetworkTopology::MESH_2D ||
        config_.topology == NetworkTopology::MESH_3D ||
        config_.topology == NetworkTopology::TORUS_2D) {
        for (auto& node : nodes_) {
            if (node.node_id == dst) {
                // Add reverse connection
                if (std::find(node.connected_nodes.begin(), node.connected_nodes.end(), src)
                    == node.connected_nodes.end()) {
                    node.connected_nodes.push_back(src);
                    std::cout << "Created bidirectional link " << dst << " <-> " << src << std::endl;
                }
                break;
            }
        }
    }
}

bool GarnetModel::canInject(uint32_t src_node) const {
    // Check if injection queue has space
    auto it = injection_queues_.find(src_node);
    if (it == injection_queues_.end()) {
        return false;
    }

    // Check against buffer depth
    return it->second.size() < config_.input_buffer_depth;
}

void GarnetModel::injectPacket(const NetworkPacket& packet) {
    if (!canInject(packet.src_node)) {
        std::cerr << "Cannot inject packet: injection queue full at node "
                  << packet.src_node << std::endl;
        return;
    }

    // Create packet with injection timestamp
    NetworkPacket timestamped_packet = packet;
    timestamped_packet.inject_cycle = current_cycle_;

    // Add to injection queue
    injection_queues_[packet.src_node].push(timestamped_packet);

    // Update statistics
    stats_.total_packets++;
    stats_.total_flits += (packet.size + config_.link_width_bytes - 1) / config_.link_width_bytes;

    // Actually inject into GARNET network
    // In full GARNET integration, this would:
    // 1. Break packet into flits (based on link_width_bytes)
    // 2. Add header flit with routing info
    // 3. Insert into router input buffer at src_node
    // 4. Trigger router pipeline (RC, VA, SA, ST stages)
    // 5. Update credit counters
    //
    // For simulation, packets are moved from injection to ejection queues
    // during tick() based on computed latency

    std::cout << "[Cycle " << current_cycle_ << "] Injected packet: src=" << packet.src_node
              << " dst=" << packet.dst_node
              << " size=" << packet.size << " bytes"
              << " type=" << static_cast<int>(packet.type) << std::endl;
}

bool GarnetModel::hasArrived(uint32_t dst_node) const {
    auto it = ejection_queues_.find(dst_node);
    if (it == ejection_queues_.end()) {
        return false;
    }
    return !it->second.empty();
}

NetworkPacket GarnetModel::extractPacket(uint32_t dst_node) {
    auto it = ejection_queues_.find(dst_node);
    if (it == ejection_queues_.end() || it->second.empty()) {
        std::cerr << "No packet available at node " << dst_node << std::endl;
        return NetworkPacket(0, 0, PacketType::DATA, 0, 0, 0);
    }

    NetworkPacket packet = it->second.front();
    it->second.pop();

    // Calculate latency
    Cycle latency = current_cycle_ - packet.inject_cycle;
    updateStatistics(packet, latency);

    // Trigger callback if set
    if (arrival_callback_) {
        arrival_callback_(packet);
    }

    return packet;
}

void GarnetModel::tick() {
    current_cycle_++;

    // Advance GARNET simulation by one cycle
    // Full GARNET integration would:
    // 1. Process router pipeline stages (RC, VA, SA, ST)
    //    - RC: Route Computation using XY routing
    //    - VA: Virtual Channel Allocation
    //    - SA: Switch Allocation (crossbar arbitration)
    //    - ST: Switch Traversal (move flits through crossbar)
    // 2. Move flits through links (link traversal stage)
    // 3. Handle VC allocation and credit flow control
    // 4. Update power/energy counters for routers and links
    //
    // Current implementation: Cycle-accurate packet movement
    // Packets spend router_latency + link_latency per hop

    // Process injection queues - move packets into the network
    for (auto& inj_pair : injection_queues_) {
        if (!inj_pair.second.empty()) {
            NetworkPacket& packet = inj_pair.second.front();

            // Compute route using topology-appropriate routing algorithm
            auto route = computeRoute(packet.src_node, packet.dst_node);

            // Calculate end-to-end network latency
            // Each hop: router_latency (pipeline) + link_latency (wire delay)
            uint32_t num_hops = route.size() > 0 ? route.size() - 1 : 0;
            Cycle network_latency = num_hops * config_.router_latency +
                                   num_hops * config_.link_latency;

            if (current_cycle_ >= packet.inject_cycle + network_latency) {
                // Packet has arrived
                ejection_queues_[packet.dst_node].push(packet);
                inj_pair.second.pop();
            }
        }
    }

    // Update energy
    // Simplified: energy per cycle for active routers and links
    router_energy_ += 0.1 * nodes_.size();  // 0.1 nJ per router per cycle
    link_energy_ += 0.05 * nodes_.size();   // 0.05 nJ per link per cycle
}

NetworkStats GarnetModel::getStats() const {
    return stats_;
}

void GarnetModel::resetStats() {
    stats_ = NetworkStats();
    router_energy_ = 0.0;
    link_energy_ = 0.0;
}

void GarnetModel::printStats() const {
    std::cout << "\n=== GARNET Network Statistics ===" << std::endl;
    std::cout << "Total packets: " << stats_.total_packets << std::endl;
    std::cout << "Total flits: " << stats_.total_flits << std::endl;
    std::cout << "Average packet latency: " << stats_.avg_packet_latency << " cycles" << std::endl;
    std::cout << "Max packet latency: " << stats_.max_packet_latency << " cycles" << std::endl;
    std::cout << "Average link utilization: " << (stats_.avg_link_utilization * 100.0)
              << "%" << std::endl;
    std::cout << "Total energy: " << stats_.total_energy_j << " J" << std::endl;
    std::cout << "  Router energy: " << router_energy_ * 1e-9 << " J" << std::endl;
    std::cout << "  Link energy: " << link_energy_ * 1e-9 << " J" << std::endl;
    std::cout << "=================================\n" << std::endl;
}

double GarnetModel::getRouterEnergy() const {
    return router_energy_ * 1e-9;  // Convert nJ to J
}

double GarnetModel::getLinkEnergy() const {
    return link_energy_ * 1e-9;  // Convert nJ to J
}

double GarnetModel::getTotalEnergy() const {
    return (router_energy_ + link_energy_) * 1e-9;  // Convert nJ to J
}

std::vector<uint32_t> GarnetModel::computeRoute(uint32_t src, uint32_t dst) {
    std::vector<uint32_t> route;
    route.push_back(src);

    // Simplified routing based on topology
    switch (config_.topology) {
        case NetworkTopology::MESH_2D:
        case NetworkTopology::TORUS_2D: {
            // XY routing
            uint32_t src_x = src % config_.num_cols;
            uint32_t src_y = src / config_.num_cols;
            uint32_t dst_x = dst % config_.num_cols;
            uint32_t dst_y = dst / config_.num_cols;

            // X dimension
            while (src_x != dst_x) {
                src_x += (dst_x > src_x) ? 1 : -1;
                uint32_t next_node = src_y * config_.num_cols + src_x;
                route.push_back(next_node);
            }

            // Y dimension
            while (src_y != dst_y) {
                src_y += (dst_y > src_y) ? 1 : -1;
                uint32_t next_node = src_y * config_.num_cols + src_x;
                route.push_back(next_node);
            }
            break;
        }

        case NetworkTopology::CROSSBAR:
            // Direct connection
            route.push_back(dst);
            break;

        default:
            // Simplified: direct route
            route.push_back(dst);
            break;
    }

    return route;
}

void GarnetModel::updateStatistics(const NetworkPacket& packet, Cycle latency) {
    // Update average latency
    uint64_t total_latency = stats_.avg_packet_latency * (stats_.total_packets - 1) + latency;
    stats_.avg_packet_latency = total_latency / stats_.total_packets;

    // Update max latency
    if (latency > stats_.max_packet_latency) {
        stats_.max_packet_latency = latency;
    }

    // Update energy
    stats_.total_energy_j = getTotalEnergy();
}

} // namespace pimid

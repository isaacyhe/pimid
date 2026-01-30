#include "host_engine/host_interconnect.h"
#include <iostream>
#include <cstring>
#include <cmath>

namespace pimid {

//=============================================================================
// GarnetHostInterconnect Implementation
//=============================================================================

GarnetHostInterconnect::GarnetHostInterconnect(const HostInterconnectConfig& config)
    : config_(config),
      total_requests_(0),
      total_responses_(0),
      total_latency_(0) {

    std::cout << "Creating GARNET-based host interconnect" << std::endl;

    // Create GARNET network configuration for host interconnect
    NetworkConfig net_config = config.garnet_config;

    // If not explicitly configured, set up default mesh for host
    if (!config.use_garnet) {
        // Calculate mesh dimensions based on number of components
        uint32_t total_nodes = config.num_cores + config.num_memory_controllers + 1; // +1 for LLC
        uint32_t rows = static_cast<uint32_t>(std::sqrt(total_nodes));
        uint32_t cols = (total_nodes + rows - 1) / rows;

        net_config.topology = NetworkTopology::MESH_2D;
        net_config.num_rows = rows;
        net_config.num_cols = cols;
        net_config.routing = RoutingAlgorithm::XY;
        net_config.virtual_channels = 4;
        net_config.link_width_bytes = 8;
        net_config.link_latency = 1;
        net_config.router_latency = 2;
        net_config.input_buffer_depth = 8;
        net_config.output_buffer_depth = 4;
    }

    // Create GARNET network model
    garnet_ = std::make_unique<GarnetModel>(net_config);
}

GarnetHostInterconnect::~GarnetHostInterconnect() = default;

void GarnetHostInterconnect::initialize() {
    std::cout << "Initializing GARNET host interconnect..." << std::endl;

    // Setup topology and node mappings
    setupTopology();

    // Initialize GARNET network
    garnet_->initialize();

    std::cout << "GARNET host interconnect initialized with "
              << component_to_node_.size() << " components" << std::endl;
}

void GarnetHostInterconnect::setupTopology() {
    uint32_t node_id = 0;

    // Map cores to network nodes
    for (uint32_t i = 0; i < config_.num_cores; i++) {
        component_to_node_[i] = node_id;
        node_to_component_[node_id] = i;

        NetworkNode node(node_id, PEPlacementLevel::LOGIC_DIE, i);
        garnet_->addNode(node);

        std::cout << "  Mapped core " << i << " to network node " << node_id << std::endl;
        node_id++;
    }

    // Map memory controllers to network nodes
    uint32_t mc_base = 1000;  // Base ID for memory controllers
    for (uint32_t i = 0; i < config_.num_memory_controllers; i++) {
        uint32_t mc_id = mc_base + i;
        component_to_node_[mc_id] = node_id;
        node_to_component_[node_id] = mc_id;

        NetworkNode node(node_id, PEPlacementLevel::LOGIC_DIE, mc_id);
        garnet_->addNode(node);

        std::cout << "  Mapped memory controller " << i << " to network node "
                  << node_id << std::endl;
        node_id++;
    }

    // Map LLC (Last Level Cache) to network node
    uint32_t llc_id = 2000;
    component_to_node_[llc_id] = node_id;
    node_to_component_[node_id] = llc_id;

    NetworkNode llc_node(node_id, PEPlacementLevel::LOGIC_DIE, llc_id);
    garnet_->addNode(llc_node);

    std::cout << "  Mapped LLC to network node " << node_id << std::endl;

    // Set up connections based on topology
    uint32_t total_nodes = node_id + 1;
    uint32_t rows = config_.garnet_config.num_rows;
    uint32_t cols = config_.garnet_config.num_cols;

    // Adjust dimensions if not explicitly set
    if (rows == 0 || cols == 0) {
        rows = static_cast<uint32_t>(std::sqrt(total_nodes));
        cols = (total_nodes + rows - 1) / rows;
    }

    switch (config_.garnet_config.topology) {
        case NetworkTopology::MESH_2D:
            // Connect adjacent nodes in 2D mesh (no wrap-around)
            std::cout << "  Setting up 2D mesh connections (" << rows << "x" << cols << ")" << std::endl;
            for (uint32_t r = 0; r < rows; r++) {
                for (uint32_t c = 0; c < cols; c++) {
                    uint32_t current = r * cols + c;
                    if (current >= total_nodes) break;

                    // Connect to east neighbor
                    if (c < cols - 1) {
                        uint32_t east = r * cols + (c + 1);
                        if (east < total_nodes) {
                            garnet_->connectNodes(current, east);
                            garnet_->connectNodes(east, current);
                        }
                    }

                    // Connect to south neighbor
                    if (r < rows - 1) {
                        uint32_t south = (r + 1) * cols + c;
                        if (south < total_nodes) {
                            garnet_->connectNodes(current, south);
                            garnet_->connectNodes(south, current);
                        }
                    }
                }
            }
            break;

        case NetworkTopology::TORUS_2D:
            // Connect adjacent nodes with wrap-around
            std::cout << "  Setting up 2D torus connections (" << rows << "x" << cols << ")" << std::endl;
            for (uint32_t r = 0; r < rows; r++) {
                for (uint32_t c = 0; c < cols; c++) {
                    uint32_t current = r * cols + c;
                    if (current >= total_nodes) break;

                    // Connect to east neighbor (with wrap-around)
                    uint32_t east_c = (c + 1) % cols;
                    uint32_t east = r * cols + east_c;
                    if (east < total_nodes && east != current) {
                        garnet_->connectNodes(current, east);
                        garnet_->connectNodes(east, current);
                    }

                    // Connect to south neighbor (with wrap-around)
                    uint32_t south_r = (r + 1) % rows;
                    uint32_t south = south_r * cols + c;
                    if (south < total_nodes && south != current) {
                        garnet_->connectNodes(current, south);
                        garnet_->connectNodes(south, current);
                    }
                }
            }
            break;

        case NetworkTopology::CROSSBAR:
            // Full connectivity - every node connects to every other node
            std::cout << "  Setting up crossbar connections (" << total_nodes << " nodes)" << std::endl;
            for (uint32_t i = 0; i < total_nodes; i++) {
                for (uint32_t j = i + 1; j < total_nodes; j++) {
                    garnet_->connectNodes(i, j);
                    garnet_->connectNodes(j, i);
                }
            }
            break;

        case NetworkTopology::H_TREE:
        case NetworkTopology::FAT_TREE:
            // Tree-based topologies - connect nodes in hierarchical fashion
            std::cout << "  Setting up tree connections (" << total_nodes << " nodes)" << std::endl;
            // Simple binary tree: each node i connects to children 2i+1 and 2i+2
            for (uint32_t i = 0; i < total_nodes; i++) {
                uint32_t left_child = 2 * i + 1;
                uint32_t right_child = 2 * i + 2;

                if (left_child < total_nodes) {
                    garnet_->connectNodes(i, left_child);
                    garnet_->connectNodes(left_child, i);
                }
                if (right_child < total_nodes) {
                    garnet_->connectNodes(i, right_child);
                    garnet_->connectNodes(right_child, i);
                }
            }
            break;

        default:
            // Default to mesh topology
            std::cout << "  Setting up default mesh connections" << std::endl;
            for (uint32_t r = 0; r < rows; r++) {
                for (uint32_t c = 0; c < cols; c++) {
                    uint32_t current = r * cols + c;
                    if (current >= total_nodes) break;

                    if (c < cols - 1) {
                        uint32_t east = r * cols + (c + 1);
                        if (east < total_nodes) {
                            garnet_->connectNodes(current, east);
                            garnet_->connectNodes(east, current);
                        }
                    }
                    if (r < rows - 1) {
                        uint32_t south = (r + 1) * cols + c;
                        if (south < total_nodes) {
                            garnet_->connectNodes(current, south);
                            garnet_->connectNodes(south, current);
                        }
                    }
                }
            }
            break;
    }

    std::cout << "  Topology connections established" << std::endl;
}

bool GarnetHostInterconnect::canSend(uint32_t src_id) {
    auto it = component_to_node_.find(src_id);
    if (it == component_to_node_.end()) {
        return false;
    }

    return garnet_->canInject(it->second);
}

void GarnetHostInterconnect::sendRequest(const HostInterconnectRequest& req) {
    // Convert request to network packet
    NetworkPacket packet = requestToPacket(req);

    // Inject into GARNET network
    garnet_->injectPacket(packet);

    total_requests_++;

    std::cout << "Host interconnect: sent request from component " << req.src_id
              << " to component " << req.dst_id << std::endl;
}

bool GarnetHostInterconnect::hasResponse(uint32_t dst_id) {
    auto it = component_to_node_.find(dst_id);
    if (it == component_to_node_.end()) {
        return false;
    }

    // Check if there are packets at this node
    return garnet_->hasArrived(it->second);
}

HostInterconnectRequest GarnetHostInterconnect::getResponse(uint32_t dst_id) {
    auto it = component_to_node_.find(dst_id);
    if (it == component_to_node_.end()) {
        std::cerr << "Invalid destination component ID: " << dst_id << std::endl;
        return HostInterconnectRequest();
    }

    // Extract packet from GARNET
    NetworkPacket packet = garnet_->extractPacket(it->second);

    // Convert packet back to request
    HostInterconnectRequest req = packetToRequest(packet);

    total_responses_++;
    total_latency_ += (garnet_->getCurrentCycle() - req.issue_cycle);

    return req;
}

void GarnetHostInterconnect::tick() {
    // Advance GARNET network by one cycle
    garnet_->tick();
}

void GarnetHostInterconnect::printStats() const {
    std::cout << "\n=== Host Interconnect (GARNET) Statistics ===" << std::endl;
    std::cout << "Total requests: " << total_requests_ << std::endl;
    std::cout << "Total responses: " << total_responses_ << std::endl;

    if (total_responses_ > 0) {
        std::cout << "Average latency: "
                  << (static_cast<double>(total_latency_) / total_responses_)
                  << " cycles" << std::endl;
    }

    std::cout << "\nUnderlying GARNET network stats:" << std::endl;
    garnet_->printStats();
}

void GarnetHostInterconnect::resetStats() {
    total_requests_ = 0;
    total_responses_ = 0;
    total_latency_ = 0;
    garnet_->resetStats();
}

uint32_t GarnetHostInterconnect::mapComponentToNode(uint32_t component_id) {
    auto it = component_to_node_.find(component_id);
    if (it != component_to_node_.end()) {
        return it->second;
    }
    return 0;  // Default to node 0
}

HostInterconnectRequest GarnetHostInterconnect::packetToRequest(
    const NetworkPacket& packet) {

    HostInterconnectRequest req;

    // Map network nodes back to component IDs
    auto src_it = node_to_component_.find(packet.src_node);
    auto dst_it = node_to_component_.find(packet.dst_node);

    req.src_id = (src_it != node_to_component_.end()) ? src_it->second : 0;
    req.dst_id = (dst_it != node_to_component_.end()) ? dst_it->second : 0;
    req.addr = packet.addr;
    req.size = packet.size;
    req.issue_cycle = packet.inject_cycle;

    // Decode type from packet type
    req.type = (packet.type == PacketType::DATA) ? MemoryRequestType::READ
                                                  : MemoryRequestType::WRITE;

    return req;
}

NetworkPacket GarnetHostInterconnect::requestToPacket(
    const HostInterconnectRequest& req) {

    // Map component IDs to network nodes
    uint32_t src_node = mapComponentToNode(req.src_id);
    uint32_t dst_node = mapComponentToNode(req.dst_id);

    // Convert request type to packet type
    PacketType pkt_type = (req.type == MemoryRequestType::READ) ? PacketType::DATA
                                                                 : PacketType::CONTROL;

    return NetworkPacket(src_node, dst_node, pkt_type, req.size, req.addr,
                         req.issue_cycle);
}

//=============================================================================
// CrossbarInterconnect Implementation (Baseline)
//=============================================================================

CrossbarInterconnect::CrossbarInterconnect(const HostInterconnectConfig& config)
    : config_(config),
      current_cycle_(0),
      total_requests_(0),
      total_responses_(0) {

    std::cout << "Creating simple crossbar host interconnect" << std::endl;
}

void CrossbarInterconnect::initialize() {
    std::cout << "Initializing crossbar interconnect..." << std::endl;

    // Initialize response queues for all components
    for (uint32_t i = 0; i < config_.num_cores; i++) {
        response_queues_[i] = std::queue<HostInterconnectRequest>();
    }

    for (uint32_t i = 0; i < config_.num_memory_controllers; i++) {
        response_queues_[1000 + i] = std::queue<HostInterconnectRequest>();
    }

    response_queues_[2000] = std::queue<HostInterconnectRequest>();  // LLC

    std::cout << "Crossbar interconnect initialized" << std::endl;
}

bool CrossbarInterconnect::canSend(uint32_t src_id) {
    // Crossbar can always accept (infinite bandwidth model)
    return true;
}

void CrossbarInterconnect::sendRequest(const HostInterconnectRequest& req) {
    InFlightRequest inflight;
    inflight.req = req;
    inflight.completion_cycle = current_cycle_ + config_.latency_cycles;

    in_flight_.push_back(inflight);
    total_requests_++;
}

bool CrossbarInterconnect::hasResponse(uint32_t dst_id) {
    auto it = response_queues_.find(dst_id);
    if (it == response_queues_.end()) {
        return false;
    }
    return !it->second.empty();
}

HostInterconnectRequest CrossbarInterconnect::getResponse(uint32_t dst_id) {
    auto it = response_queues_.find(dst_id);
    if (it == response_queues_.end() || it->second.empty()) {
        return HostInterconnectRequest();
    }

    HostInterconnectRequest req = it->second.front();
    it->second.pop();
    total_responses_++;

    return req;
}

void CrossbarInterconnect::tick() {
    current_cycle_++;

    // Check for completed requests
    auto it = in_flight_.begin();
    while (it != in_flight_.end()) {
        if (current_cycle_ >= it->completion_cycle) {
            // Request completed, add to response queue
            response_queues_[it->req.src_id].push(it->req);
            it = in_flight_.erase(it);
        } else {
            ++it;
        }
    }
}

void CrossbarInterconnect::printStats() const {
    std::cout << "\n=== Host Interconnect (Crossbar) Statistics ===" << std::endl;
    std::cout << "Total requests: " << total_requests_ << std::endl;
    std::cout << "Total responses: " << total_responses_ << std::endl;
    std::cout << "In-flight requests: " << in_flight_.size() << std::endl;
    std::cout << "Average latency: " << config_.latency_cycles << " cycles (fixed)" << std::endl;
    std::cout << "================================================\n" << std::endl;
}

void CrossbarInterconnect::resetStats() {
    total_requests_ = 0;
    total_responses_ = 0;
}

//=============================================================================
// HostInterconnectFactory Implementation
//=============================================================================

std::unique_ptr<HostInterconnect> HostInterconnectFactory::create(
    const HostInterconnectConfig& config) {

    switch (config.type) {
        case HostInterconnectType::MESH_2D:
        case HostInterconnectType::CUSTOM_GARNET:
            return createGarnet(config);

        case HostInterconnectType::CROSSBAR:
        case HostInterconnectType::BUS:
        case HostInterconnectType::RING:
        default:
            return createCrossbar(config);
    }
}

std::unique_ptr<HostInterconnect> HostInterconnectFactory::createGarnet(
    const HostInterconnectConfig& config) {

    return std::make_unique<GarnetHostInterconnect>(config);
}

std::unique_ptr<HostInterconnect> HostInterconnectFactory::createCrossbar(
    const HostInterconnectConfig& config) {

    return std::make_unique<CrossbarInterconnect>(config);
}

} // namespace pimid

#ifndef PIMID_NETWORK_MODEL_H
#define PIMID_NETWORK_MODEL_H

#include "common/types.h"
#include <string>
#include <queue>
#include <vector>
#include <map>
#include <functional>

namespace pimid {

/**
 * Network topology types
 */
enum class NetworkTopology {
    MESH_2D,
    MESH_3D,
    TORUS_2D,
    TORUS_3D,
    DRAGONFLY,
    FAT_TREE,
    CROSSBAR
};

/**
 * Routing algorithms
 */
enum class RoutingAlgorithm {
    XY,
    XYZ,
    ADAPTIVE,
    WEST_FIRST,
    NORTH_LAST,
    MINIMAL,
    VALIANT
};

/**
 * Flow control mechanisms
 */
enum class FlowControl {
    CREDIT_BASED,
    ON_OFF,
    VIRTUAL_CHANNEL
};

/**
 * Network configuration
 */
struct NetworkConfig {
    NetworkTopology topology;
    RoutingAlgorithm routing;
    FlowControl flow_control;

    uint32_t num_rows;
    uint32_t num_cols;
    uint32_t num_layers;        // For 3D topologies
    uint32_t virtual_channels;
    uint32_t link_width_bytes;
    Cycle link_latency;
    Cycle router_latency;

    uint32_t input_buffer_depth;
    uint32_t output_buffer_depth;

    std::string garnet_config_path;
};

/**
 * Network node representation
 * Maps memory hierarchy components to network nodes
 */
struct NetworkNode {
    uint32_t node_id;
    PEPlacementLevel level;     // What this node represents
    uint32_t hierarchy_id;      // ID within the hierarchy (bank ID, chip ID, etc.)
    std::vector<uint32_t> connected_nodes;

    NetworkNode(uint32_t id, PEPlacementLevel lvl, uint32_t hier_id)
        : node_id(id), level(lvl), hierarchy_id(hier_id) {}
};

/**
 * Network statistics
 */
struct NetworkStats {
    uint64_t total_packets;
    uint64_t total_flits;
    Cycle avg_packet_latency;
    Cycle max_packet_latency;
    double avg_link_utilization;
    double total_energy_j;

    NetworkStats() : total_packets(0), total_flits(0),
                     avg_packet_latency(0), max_packet_latency(0),
                     avg_link_utilization(0.0), total_energy_j(0.0) {}
};

/**
 * Abstract network model interface
 * Integrates GARNET for cycle-accurate NoC simulation
 */
class NetworkModel {
public:
    NetworkModel(const NetworkConfig& config);
    virtual ~NetworkModel() = default;

    // Initialization
    virtual void initialize() = 0;
    virtual void loadConfig(const std::string& config_path) = 0;

    // Node management
    virtual void addNode(const NetworkNode& node) = 0;
    virtual void connectNodes(uint32_t src, uint32_t dst) = 0;

    // Packet injection and routing
    virtual bool canInject(uint32_t src_node) const = 0;
    virtual void injectPacket(const NetworkPacket& packet) = 0;
    virtual bool hasArrived(uint32_t dst_node) const = 0;
    virtual NetworkPacket extractPacket(uint32_t dst_node) = 0;

    // Simulation
    virtual void tick() = 0;
    virtual Cycle getCurrentCycle() const = 0;

    // Statistics
    virtual NetworkStats getStats() const = 0;
    virtual void resetStats() = 0;
    virtual void printStats() const = 0;

    // Energy modeling
    virtual double getRouterEnergy() const = 0;
    virtual double getLinkEnergy() const = 0;
    virtual double getTotalEnergy() const = 0;

    // Configuration queries
    const NetworkConfig& getConfig() const { return config_; }

    // Set callback for packet arrival
    void setArrivalCallback(std::function<void(const NetworkPacket&)> cb) {
        arrival_callback_ = cb;
    }

protected:
    NetworkConfig config_;
    std::vector<NetworkNode> nodes_;
    std::function<void(const NetworkPacket&)> arrival_callback_;
};

/**
 * GARNET-based network model implementation
 */
class GarnetModel : public NetworkModel {
public:
    explicit GarnetModel(const NetworkConfig& config);
    ~GarnetModel() override;

    // NetworkModel interface implementation
    void initialize() override;
    void loadConfig(const std::string& config_path) override;

    void addNode(const NetworkNode& node) override;
    void connectNodes(uint32_t src, uint32_t dst) override;

    bool canInject(uint32_t src_node) const override;
    void injectPacket(const NetworkPacket& packet) override;
    bool hasArrived(uint32_t dst_node) const override;
    NetworkPacket extractPacket(uint32_t dst_node) override;

    void tick() override;
    Cycle getCurrentCycle() const override { return current_cycle_; }

    NetworkStats getStats() const override;
    void resetStats() override;
    void printStats() const override;

    double getRouterEnergy() const override;
    double getLinkEnergy() const override;
    double getTotalEnergy() const override;

private:
    // GARNET interface (placeholder - will integrate actual GARNET)
    void* garnet_instance_;

    // Packet queues
    std::map<uint32_t, std::queue<NetworkPacket>> injection_queues_;
    std::map<uint32_t, std::queue<NetworkPacket>> ejection_queues_;

    // Statistics
    NetworkStats stats_;
    Cycle current_cycle_;

    // Energy tracking
    double router_energy_;
    double link_energy_;

    // Helper functions
    std::vector<uint32_t> computeRoute(uint32_t src, uint32_t dst);
    void updateStatistics(const NetworkPacket& packet, Cycle latency);
};

} // namespace pimid

#endif // PIMID_NETWORK_MODEL_H

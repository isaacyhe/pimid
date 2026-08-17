#ifndef PIMID_NETWORK_MODEL_H
#define PIMID_NETWORK_MODEL_H

#include "common/types.h"
#include <string>
#include <queue>
#include <vector>
#include <map>
#include <functional>
#include <memory>

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
    H_TREE,        // H-tree for DRAM internal interconnects
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
    VALIANT,
    TREE_BASED      // For H-tree and Fat-tree (route up then down)
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
 * Router pipeline complexity
 * Controls how many stages are in the router pipeline
 */
enum class RouterPipelineComplexity {
    FULL,       // RC -> VA -> SA -> ST (4-stage, full NoC router with VC allocation)
    REDUCED,    // RC -> SA -> ST (3-stage, no VC allocation, simpler)
    SIMPLE,     // SA -> ST (2-stage, just arbitration + traversal)
    MINIMAL     // ST only (1-stage, just a mux/switch, for DRAM)
};

/**
 * Network configuration
 */
struct NetworkConfig {
    NetworkTopology topology;
    RoutingAlgorithm routing;
    FlowControl flow_control;

    // Topology parameters
    uint32_t num_rows;
    uint32_t num_cols;
    uint32_t num_layers;        // For 3D topologies

    // Virtual Networks (VN) and Virtual Channels (VC)
    uint32_t virtual_networks;  // Number of VNs (message classes: req/resp, read/write, etc.)
    uint32_t virtual_channels_per_vn;  // Number of VCs per VN (for deadlock avoidance)
    uint32_t virtual_channels;  // DEPRECATED: Use virtual_networks * virtual_channels_per_vn

    // Link parameters
    uint32_t link_width_bytes;
    Cycle link_latency;

    // Router parameters
    RouterPipelineComplexity router_pipeline;  // Pipeline complexity
    Cycle router_latency;      // Total router latency (overrides pipeline if set)
    bool enable_router_bypass; // Bypass router for single-hop transfers

    // Buffer parameters
    uint32_t input_buffer_depth;
    uint32_t output_buffer_depth;

    std::string garnet_config_path;

    // Constructor with defaults
    NetworkConfig()
        : topology(NetworkTopology::MESH_2D),
          routing(RoutingAlgorithm::XY),
          flow_control(FlowControl::CREDIT_BASED),
          num_rows(4), num_cols(4), num_layers(1),
          virtual_networks(1),
          virtual_channels_per_vn(1),
          virtual_channels(1),
          link_width_bytes(4),
          link_latency(1),
          router_pipeline(RouterPipelineComplexity::FULL),
          router_latency(0),  // 0 = use pipeline stages
          enable_router_bypass(false),
          input_buffer_depth(4),
          output_buffer_depth(4) {}
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
 * NoC activity statistics for power modeling (McPAT integration)
 * Tracks detailed component-level activity for accurate power estimation
 */
struct NoCActivity {
    // Traffic statistics
    uint64_t total_packets;
    uint64_t total_flits;
    uint64_t total_hops;           // Sum of hops across all packets

    // Router activity
    uint64_t buffer_reads;         // VC buffer reads
    uint64_t buffer_writes;        // VC buffer writes
    uint64_t crossbar_traversals;  // Crossbar switch activity
    uint64_t arbiter_events;       // Switch allocator arbitrations

    // Link activity
    uint64_t link_traversals;      // Total link traversals

    // Timing
    uint64_t total_cycles;
    double clock_mhz;

    NoCActivity()
        : total_packets(0), total_flits(0), total_hops(0)
        , buffer_reads(0), buffer_writes(0)
        , crossbar_traversals(0), arbiter_events(0)
        , link_traversals(0), total_cycles(0), clock_mhz(1000.0)
    {}
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

    // Activity export for McPAT power modeling
    virtual NoCActivity getActivity() const = 0;

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

    NoCActivity getActivity() const override;

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

/**
 * Factory function to create Garnet-based network wrapper
 * Uses the actual extracted Garnet library from gem5
 */
std::unique_ptr<NetworkModel> createGarnetNetworkWrapper(const NetworkConfig& config);

} // namespace pimid

#endif // PIMID_NETWORK_MODEL_H

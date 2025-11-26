#ifndef PIMID_HOST_INTERCONNECT_H
#define PIMID_HOST_INTERCONNECT_H

#include "common/types.h"
#include "network_model.h"
#include <memory>
#include <string>

namespace pimid {

/**
 * Host interconnect types
 */
enum class HostInterconnectType {
    BUS,           // Simple shared bus
    CROSSBAR,      // Full crossbar
    RING,          // Ring interconnect
    MESH_2D,       // 2D mesh (using GARNET)
    CUSTOM_GARNET  // Custom GARNET-based interconnect
};

/**
 * Host interconnect configuration
 */
struct HostInterconnectConfig {
    HostInterconnectType type;
    uint32_t num_cores;
    uint32_t num_memory_controllers;
    uint64_t bandwidth_gbs;
    Cycle latency_cycles;

    // GARNET-specific configuration (if using GARNET)
    NetworkConfig garnet_config;
    bool use_garnet;

    HostInterconnectConfig()
        : type(HostInterconnectType::CROSSBAR),
          num_cores(4),
          num_memory_controllers(2),
          bandwidth_gbs(64),
          latency_cycles(10),
          use_garnet(false) {}
};

/**
 * Host interconnect request
 */
struct HostInterconnectRequest {
    uint32_t src_id;      // Source (core, cache, etc.)
    uint32_t dst_id;      // Destination (memory controller, LLC, etc.)
    Address addr;
    uint64_t size;
    MemoryRequestType type;
    Cycle issue_cycle;

    HostInterconnectRequest()
        : src_id(0), dst_id(0), addr(0), size(0),
          type(MemoryRequestType::READ), issue_cycle(0) {}
};

/**
 * Abstract host interconnect interface
 */
class HostInterconnect {
public:
    virtual ~HostInterconnect() = default;

    // Initialization
    virtual void initialize() = 0;

    // Request handling
    virtual bool canSend(uint32_t src_id) = 0;
    virtual void sendRequest(const HostInterconnectRequest& req) = 0;
    virtual bool hasResponse(uint32_t dst_id) = 0;
    virtual HostInterconnectRequest getResponse(uint32_t dst_id) = 0;

    // Simulation
    virtual void tick() = 0;

    // Statistics
    virtual void printStats() const = 0;
    virtual void resetStats() = 0;

    // Configuration
    virtual const HostInterconnectConfig& getConfig() const = 0;
};

/**
 * GARNET-based host interconnect
 * Uses GARNET NoC for cache-coherent host interconnect
 */
class GarnetHostInterconnect : public HostInterconnect {
public:
    explicit GarnetHostInterconnect(const HostInterconnectConfig& config);
    ~GarnetHostInterconnect() override;

    // HostInterconnect interface
    void initialize() override;
    bool canSend(uint32_t src_id) override;
    void sendRequest(const HostInterconnectRequest& req) override;
    bool hasResponse(uint32_t dst_id) override;
    HostInterconnectRequest getResponse(uint32_t dst_id) override;
    void tick() override;
    void printStats() const override;
    void resetStats() override;
    const HostInterconnectConfig& getConfig() const override { return config_; }

private:
    HostInterconnectConfig config_;
    std::unique_ptr<GarnetModel> garnet_;

    // Mapping: core/cache IDs to network node IDs
    std::map<uint32_t, uint32_t> component_to_node_;
    std::map<uint32_t, uint32_t> node_to_component_;

    // Pending requests
    std::map<uint32_t, std::queue<HostInterconnectRequest>> pending_responses_;

    // Statistics
    uint64_t total_requests_;
    uint64_t total_responses_;
    Cycle total_latency_;

    // Helper functions
    void setupTopology();
    uint32_t mapComponentToNode(uint32_t component_id);
    HostInterconnectRequest packetToRequest(const NetworkPacket& packet);
    NetworkPacket requestToPacket(const HostInterconnectRequest& req);
};

/**
 * Simple crossbar interconnect (baseline)
 */
class CrossbarInterconnect : public HostInterconnect {
public:
    explicit CrossbarInterconnect(const HostInterconnectConfig& config);
    ~CrossbarInterconnect() override = default;

    void initialize() override;
    bool canSend(uint32_t src_id) override;
    void sendRequest(const HostInterconnectRequest& req) override;
    bool hasResponse(uint32_t dst_id) override;
    HostInterconnectRequest getResponse(uint32_t dst_id) override;
    void tick() override;
    void printStats() const override;
    void resetStats() override;
    const HostInterconnectConfig& getConfig() const override { return config_; }

private:
    HostInterconnectConfig config_;

    // In-flight requests
    struct InFlightRequest {
        HostInterconnectRequest req;
        Cycle completion_cycle;
    };
    std::vector<InFlightRequest> in_flight_;

    // Response queues
    std::map<uint32_t, std::queue<HostInterconnectRequest>> response_queues_;

    Cycle current_cycle_;
    uint64_t total_requests_;
    uint64_t total_responses_;
};

/**
 * Factory for creating host interconnects
 */
class HostInterconnectFactory {
public:
    static std::unique_ptr<HostInterconnect> create(
        const HostInterconnectConfig& config);

    static std::unique_ptr<HostInterconnect> createGarnet(
        const HostInterconnectConfig& config);

    static std::unique_ptr<HostInterconnect> createCrossbar(
        const HostInterconnectConfig& config);
};

} // namespace pimid

#endif // PIMID_HOST_INTERCONNECT_H

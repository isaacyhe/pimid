/**
 * @file Network.hh
 * @brief Base network class for Ruby
 */

#ifndef __GARNET_COMPAT_MEM_RUBY_NETWORK_NETWORK_HH__
#define __GARNET_COMPAT_MEM_RUBY_NETWORK_NETWORK_HH__

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>

#include "base/types.hh"
#include "sim/clocked_object.hh"
#include "../common/Consumer.hh"
#include "../common/MachineID.hh"
#include "../common/NetDest.hh"
#include "../common/TypeDefines.hh"
#include "BasicLink.hh"
#include "BasicRouter.hh"
#include "MessageBuffer.hh"
#include "Topology.hh"

namespace gem5 {
namespace ruby {

// Forward declarations
class RubySystem;

/**
 * Network - abstract base class for all network implementations
 */
class Network : public ClockedObject {
public:
    struct Params : public ClockedObject::Params {
        uint32_t num_nodes = 1;
        std::vector<std::string> vnet_type_names;
        Topology* topology = nullptr;
        RubySystem* ruby_system = nullptr;
    };

    Network(const Params& p)
        : ClockedObject(p),
          m_nodes(p.num_nodes),
          m_topology_ptr(p.topology),
          rubySystem_(p.ruby_system) {
        // Initialize virtual network info
        m_vnet_type_names = p.vnet_type_names;
        m_virtual_networks = static_cast<uint32_t>(m_vnet_type_names.size());
        if (m_virtual_networks == 0) m_virtual_networks = 3; // Default: request, response, data

        // Initialize message queues and ordered flags
        m_toNetQueues.resize(m_nodes);
        m_fromNetQueues.resize(m_nodes);
        m_ordered.resize(m_virtual_networks, false);
        for (uint32_t i = 0; i < m_nodes; i++) {
            m_toNetQueues[i].resize(m_virtual_networks, nullptr);
            m_fromNetQueues[i].resize(m_virtual_networks, nullptr);
        }
    }

    virtual ~Network() = default;

    // Get number of virtual networks
    static uint32_t getNumberOfVirtualNetworks() { return m_virtual_networks; }

    // Get number of nodes
    uint32_t getNumNodes() const { return m_nodes; }

    // Message size conversion (stub)
    static uint32_t MessageSizeType_to_int(MessageSizeType size_type) {
        switch (size_type) {
            case MessageSizeType::Control:
            case MessageSizeType::Request_Control:
            case MessageSizeType::Response_Control:
                return 8;  // Control message = 8 bytes
            case MessageSizeType::Data:
            case MessageSizeType::Response_Data:
            case MessageSizeType::Writeback_Data:
                return 72; // Data message = 8 + 64 bytes
            default:
                return 8;
        }
    }

    // Set message buffers
    void setToNetQueue(NodeID id, bool ordered, int vnet,
                       std::string vnet_type, MessageBuffer* buf) {
        if (id < m_nodes && vnet < static_cast<int>(m_virtual_networks)) {
            m_toNetQueues[id][vnet] = buf;
        }
    }

    virtual void setFromNetQueue(NodeID id, bool ordered, int vnet,
                                 std::string vnet_type, MessageBuffer* buf) {
        if (id < m_nodes && vnet < static_cast<int>(m_virtual_networks)) {
            m_fromNetQueues[id][vnet] = buf;
        }
    }

    virtual void checkNetworkAllocation(NodeID id, bool ordered,
                                        int vnet, std::string vnet_type) {
        // Override in derived class
    }

    // Virtual functions for link creation (must be implemented by derived class)
    virtual void makeExtOutLink(SwitchID src, NodeID dest, BasicLink* link,
                                std::vector<NetDest>& routing_table_entry) = 0;
    virtual void makeExtInLink(NodeID src, SwitchID dest, BasicLink* link,
                               std::vector<NetDest>& routing_table_entry) = 0;
    virtual void makeInternalLink(SwitchID src, SwitchID dest, BasicLink* link,
                                  std::vector<NetDest>& routing_table_entry,
                                  PortDirection src_outport,
                                  PortDirection dst_inport) = 0;

    // Statistics
    virtual void collateStats() = 0;
    virtual void print(std::ostream& out) const = 0;
    virtual void regStats() {}  // Statistics registration (no-op for standalone)

    // Topology access
    Topology* getTopology() { return m_topology_ptr; }
    const Topology* getTopology() const { return m_topology_ptr; }

    // Ruby system access
    RubySystem* getRubySystem() const { return rubySystem_; }

    // Get local node ID (for multi-network setups)
    NodeID getLocalNodeID(NodeID global_id) const {
        auto it = globalToLocalMap_.find(global_id);
        if (it != globalToLocalMap_.end())
            return it->second;
        return global_id;
    }

    // Warmup/randomization flags (stub)
    bool getRandomization() const { return false; }
    bool getWarmupEnabled() const { return false; }

protected:
    // Use gem5-style names for compatibility with Garnet source
    uint32_t m_nodes;  // Number of nodes
    static uint32_t m_virtual_networks;
    std::vector<std::string> m_vnet_type_names;
    Topology* m_topology_ptr;
    RubySystem* rubySystem_;

    // Message queues (gem5 naming)
    std::vector<std::vector<MessageBuffer*>> m_toNetQueues;
    std::vector<std::vector<MessageBuffer*>> m_fromNetQueues;

    // Node mapping
    std::unordered_map<NodeID, NodeID> globalToLocalMap_;

    // Ordered vnet flags
    std::vector<bool> m_ordered;

    // Control/data message sizes
    static uint32_t m_control_msg_size;
    static uint32_t m_data_msg_size;
};

// Static member initialization
inline uint32_t Network::m_virtual_networks = 3;
inline uint32_t Network::m_control_msg_size = 8;
inline uint32_t Network::m_data_msg_size = 72;

inline std::ostream& operator<<(std::ostream& out, const Network& net) {
    net.print(out);
    return out;
}

} // namespace ruby
} // namespace gem5

#endif // __GARNET_COMPAT_MEM_RUBY_NETWORK_NETWORK_HH__

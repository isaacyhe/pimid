/**
 * @file Topology.hh
 * @brief Network topology configuration
 */

#ifndef __GARNET_COMPAT_MEM_RUBY_NETWORK_TOPOLOGY_HH__
#define __GARNET_COMPAT_MEM_RUBY_NETWORK_TOPOLOGY_HH__

#include <string>
#include <vector>
#include <map>

#include "base/types.hh"
#include "../common/NetDest.hh"
#include "BasicLink.hh"
#include "BasicRouter.hh"

namespace gem5 {
namespace ruby {

// Forward declarations
class Network;

/**
 * Topology - defines network structure
 */
class Topology {
public:
    Topology(uint32_t num_routers)
        : numRouters_(num_routers) {}

    virtual ~Topology() = default;

    // Get number of routers
    uint32_t numRouters() const { return numRouters_; }

    // Add internal link (router to router)
    void addInternalLink(BasicIntLink* link) {
        internalLinks_.push_back(link);
    }

    // Add external link (router to NI)
    void addExternalLink(BasicExtLink* link) {
        externalLinks_.push_back(link);
    }

    // Get links
    const std::vector<BasicIntLink*>& getInternalLinks() const {
        return internalLinks_;
    }

    const std::vector<BasicExtLink*>& getExternalLinks() const {
        return externalLinks_;
    }

    // Create routing table
    void createRoutingTable(Network* net);

    // Create links in the network
    void createLinks(Network* net) {
        // This is called during network init to set up all links
        // For standalone use, the links should already be set up via
        // addInternalLink/addExternalLink during topology construction
    }

    // Get routing table entry
    const std::vector<NetDest>& getRoutingEntry(SwitchID src, SwitchID dst) const {
        auto key = std::make_pair(src, dst);
        auto it = routingTable_.find(key);
        if (it != routingTable_.end())
            return it->second;
        static std::vector<NetDest> empty;
        return empty;
    }

    // Print topology
    void print(std::ostream& out) const {
        out << "Topology [routers=" << numRouters_
            << ", int_links=" << internalLinks_.size()
            << ", ext_links=" << externalLinks_.size() << "]";
    }

protected:
    uint32_t numRouters_;
    std::vector<BasicIntLink*> internalLinks_;
    std::vector<BasicExtLink*> externalLinks_;
    std::map<std::pair<SwitchID, SwitchID>, std::vector<NetDest>> routingTable_;
};

inline std::ostream& operator<<(std::ostream& out, const Topology& topo) {
    topo.print(out);
    return out;
}

} // namespace ruby
} // namespace gem5

#endif // __GARNET_COMPAT_MEM_RUBY_NETWORK_TOPOLOGY_HH__

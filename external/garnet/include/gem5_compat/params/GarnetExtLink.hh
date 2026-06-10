/**
 * @file GarnetExtLink.hh
 * @brief Parameters for GarnetExtLink
 */

#ifndef __GARNET_COMPAT_PARAMS_GARNETEXTLINK_HH__
#define __GARNET_COMPAT_PARAMS_GARNETEXTLINK_HH__

#include <vector>
#include "mem/ruby/network/BasicLink.hh"

namespace gem5 {
namespace ruby {

// Forward declarations
namespace garnet {
class NetworkLink;
class CreditLink;
class NetworkBridge;
}

struct GarnetExtLinkParams : public BasicExtLink::Params {
    uint32_t link_id = 0;
    Cycles link_latency = Cycles(1);
    uint32_t vcs_per_vnet = 4;
    uint32_t virt_nets = 3;
    uint32_t channel_width = 128;  // bits

    // Network and credit links (bidirectional - index 0=in, 1=out)
    std::vector<garnet::NetworkLink*> network_links{nullptr, nullptr};
    std::vector<garnet::CreditLink*> credit_links{nullptr, nullptr};

    // CDC (clock domain crossing) flags
    bool ext_cdc = false;
    bool int_cdc = false;

    // SerDes (serializer/deserializer) flags
    bool ext_serdes = false;
    bool int_serdes = false;

    // Bridge pointers for CDC/SerDes (bidirectional)
    std::vector<garnet::NetworkBridge*> ext_net_bridge{nullptr, nullptr};
    std::vector<garnet::NetworkBridge*> ext_cred_bridge{nullptr, nullptr};
    std::vector<garnet::NetworkBridge*> int_net_bridge{nullptr, nullptr};
    std::vector<garnet::NetworkBridge*> int_cred_bridge{nullptr, nullptr};
};

} // namespace ruby
} // namespace gem5

#endif // __GARNET_COMPAT_PARAMS_GARNETEXTLINK_HH__

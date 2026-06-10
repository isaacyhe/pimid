/**
 * @file GarnetIntLink.hh
 * @brief Parameters for GarnetIntLink
 */

#ifndef __GARNET_COMPAT_PARAMS_GARNETINTLINK_HH__
#define __GARNET_COMPAT_PARAMS_GARNETINTLINK_HH__

#include "mem/ruby/network/BasicLink.hh"

namespace gem5 {
namespace ruby {

// Forward declarations
namespace garnet {
class NetworkLink;
class CreditLink;
class NetworkBridge;
}

struct GarnetIntLinkParams : public BasicIntLink::Params {
    uint32_t link_id = 0;
    Cycles link_latency = Cycles(1);
    uint32_t vcs_per_vnet = 4;
    uint32_t virt_nets = 3;
    uint32_t channel_width = 128;  // bits

    // Link pointers (set during topology creation)
    garnet::NetworkLink* network_link = nullptr;
    garnet::CreditLink* credit_link = nullptr;

    // CDC (clock domain crossing) flags
    bool src_cdc = false;
    bool dst_cdc = false;

    // SerDes (serializer/deserializer) flags
    bool src_serdes = false;
    bool dst_serdes = false;

    // Bridge pointers for CDC/SerDes
    garnet::NetworkBridge* src_net_bridge = nullptr;
    garnet::NetworkBridge* src_cred_bridge = nullptr;
    garnet::NetworkBridge* dst_net_bridge = nullptr;
    garnet::NetworkBridge* dst_cred_bridge = nullptr;
};

} // namespace ruby
} // namespace gem5

#endif // __GARNET_COMPAT_PARAMS_GARNETINTLINK_HH__

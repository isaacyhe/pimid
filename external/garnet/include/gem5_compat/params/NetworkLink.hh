/**
 * @file NetworkLink.hh
 * @brief Parameters for NetworkLink
 */

#ifndef __GARNET_COMPAT_PARAMS_NETWORKLINK_HH__
#define __GARNET_COMPAT_PARAMS_NETWORKLINK_HH__

#include <vector>
#include "sim/clocked_object.hh"

namespace gem5 {
namespace ruby {

struct NetworkLinkParams : public ClockedObject::Params {
    uint32_t link_id = 0;
    Cycles link_latency = Cycles(1);
    uint32_t vcs_per_vnet = 4;
    uint32_t virt_nets = 3;
    uint32_t channel_width = 128;  // bits
    uint32_t width = 128;  // bits (link width)
    std::vector<int> supported_vnets = {0, 1, 2};  // Supported virtual networks
};

} // namespace ruby
} // namespace gem5

#endif // __GARNET_COMPAT_PARAMS_NETWORKLINK_HH__

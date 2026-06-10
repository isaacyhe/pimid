/**
 * @file GarnetNetworkInterface.hh
 * @brief Parameters for GarnetNetworkInterface
 */

#ifndef __GARNET_COMPAT_PARAMS_GARNETNETWORKINTERFACE_HH__
#define __GARNET_COMPAT_PARAMS_GARNETNETWORKINTERFACE_HH__

#include "sim/clocked_object.hh"

namespace gem5 {
namespace ruby {

struct GarnetNetworkInterfaceParams : public ClockedObject::Params {
    uint32_t id = 0;
    uint32_t vcs_per_vnet = 4;
    uint32_t virt_nets = 3;  // Virtual networks
    uint32_t garnet_deadlock_threshold = 50000;  // Deadlock detection threshold
};

} // namespace ruby
} // namespace gem5

#endif // __GARNET_COMPAT_PARAMS_GARNETNETWORKINTERFACE_HH__

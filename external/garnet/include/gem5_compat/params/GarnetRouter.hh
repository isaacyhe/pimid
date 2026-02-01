/**
 * @file GarnetRouter.hh
 * @brief Parameters for GarnetRouter
 */

#ifndef __GARNET_COMPAT_PARAMS_GARNETROUTER_HH__
#define __GARNET_COMPAT_PARAMS_GARNETROUTER_HH__

#include "mem/ruby/network/BasicRouter.hh"

namespace gem5 {
namespace ruby {

struct GarnetRouterParams : public BasicRouter::Params {
    uint32_t vcs_per_vnet = 4;
    uint32_t virt_nets = 3;  // Virtual networks
    uint32_t routing_algorithm = 0;
    Cycles latency = Cycles(1);
    uint32_t width = 128;  // bits
};

} // namespace ruby
} // namespace gem5

#endif // __GARNET_COMPAT_PARAMS_GARNETROUTER_HH__

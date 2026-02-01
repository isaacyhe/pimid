/**
 * @file GarnetNetwork.hh
 * @brief Parameters for GarnetNetwork (standalone version)
 */

#ifndef __GARNET_COMPAT_PARAMS_GARNETNETWORK_HH__
#define __GARNET_COMPAT_PARAMS_GARNETNETWORK_HH__

#include "mem/ruby/network/Network.hh"

namespace gem5 {
namespace ruby {

// Forward declarations
class FaultModel;
namespace garnet {
class Router;
class NetworkInterface;
}

struct GarnetNetworkParams : public Network::Params {
    // Network dimensions
    uint32_t num_rows = 4;
    uint32_t num_cols = 4;

    // Virtual channels
    uint32_t vcs_per_vnet = 4;

    // Buffer depth
    uint32_t buffers_per_data_vc = 4;
    uint32_t buffers_per_ctrl_vc = 1;

    // Routing
    uint32_t routing_algorithm = 0;  // 0 = XY, 1 = custom

    // Network interface
    uint32_t ni_flit_size = 16;  // bytes

    // Enable features
    uint32_t garnet_deadlock_threshold = 50000;
    bool enable_fault_model = false;
    FaultModel* fault_model = nullptr;

    // Network components (populated during topology creation)
    std::vector<garnet::Router*> routers;
    std::vector<garnet::NetworkInterface*> netifs;
};

} // namespace ruby
} // namespace gem5

// Macro for parameter access (gem5 style)
#define PARAMS(type) using Params = type##Params

#endif // __GARNET_COMPAT_PARAMS_GARNETNETWORK_HH__

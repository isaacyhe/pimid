/**
 * @file RubyNetwork.hh
 * @brief Parameters for RubyNetwork base class
 */

#ifndef __GARNET_COMPAT_PARAMS_RUBYNETWORK_HH__
#define __GARNET_COMPAT_PARAMS_RUBYNETWORK_HH__

#include "mem/ruby/network/Network.hh"

namespace gem5 {
namespace ruby {

// RubyNetworkParams is same as Network::Params
using RubyNetworkParams = Network::Params;

} // namespace ruby
} // namespace gem5

#endif // __GARNET_COMPAT_PARAMS_RUBYNETWORK_HH__

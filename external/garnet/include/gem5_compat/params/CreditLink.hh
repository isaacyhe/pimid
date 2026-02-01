/**
 * @file CreditLink.hh
 * @brief Parameters for CreditLink
 */

#ifndef __GARNET_COMPAT_PARAMS_CREDITLINK_HH__
#define __GARNET_COMPAT_PARAMS_CREDITLINK_HH__

#include "params/NetworkLink.hh"

namespace gem5 {
namespace ruby {

struct CreditLinkParams : public NetworkLinkParams {
    // Credit links use same params as network links
};

} // namespace ruby
} // namespace gem5

#endif // __GARNET_COMPAT_PARAMS_CREDITLINK_HH__

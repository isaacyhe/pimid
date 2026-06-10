/**
 * @file FaultModel.hh
 * @brief Fault injection model stub
 */

#ifndef __GARNET_COMPAT_MEM_RUBY_NETWORK_FAULT_MODEL_FAULTMODEL_HH__
#define __GARNET_COMPAT_MEM_RUBY_NETWORK_FAULT_MODEL_FAULTMODEL_HH__

#include <string>
#include "sim/clocked_object.hh"

namespace gem5 {
namespace ruby {

// Baseline temperature for fault calculations (in Celsius)
constexpr int BASELINE_TEMPERATURE_CELCIUS = 25;

/**
 * FaultModel - stub for fault injection
 *
 * In full gem5, this models network faults for reliability studies.
 * For standalone Garnet, we provide a no-op implementation.
 */
class FaultModel : public ClockedObject {
public:
    struct Params : public ClockedObject::Params {
        bool enable = false;
    };

    FaultModel(const Params& p) : ClockedObject(p), enabled_(p.enable) {}
    virtual ~FaultModel() = default;

    // Check if link is faulty (always returns false in stub)
    bool linkFault(int link_id) const { return false; }

    // Check if router is faulty
    bool routerFault(int router_id) const { return false; }

    // Get fault probability (always 0 in stub)
    double getFaultProbability() const { return 0.0; }

    bool isEnabled() const { return enabled_; }

    // Fault vector for router (stub - always returns false/no fault)
    bool fault_vector(int router_id, int temperature, float* fault_vec) const {
        return false;
    }

    // Fault probability for router (stub - always returns false/no fault)
    bool fault_prob(int router_id, int temperature, float* prob) const {
        return false;
    }

    // Declare a router to the fault model (stub)
    // Returns the assigned router_id (incremental counter)
    int declare_router(int num_inports, int num_outports,
                       int vc_per_vnet, int buffers_per_data_vc,
                       int buffers_per_ctrl_vc) {
        return router_count_++;
    }

    // Number of fault types (stub)
    static constexpr int number_of_fault_types = 0;

    // Convert fault type to string (stub)
    std::string fault_type_to_string(int fault_type) const {
        return "unknown";
    }

private:
    bool enabled_;
    int router_count_ = 0;
};

} // namespace ruby
} // namespace gem5

#endif // __GARNET_COMPAT_MEM_RUBY_NETWORK_FAULT_MODEL_FAULTMODEL_HH__

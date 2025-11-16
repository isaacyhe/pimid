#include "power/power_model.h"
#include <iostream>

namespace pimid {

//=============================================================================
// PowerModel Base Implementation
//=============================================================================

PowerModel::PowerModel(const TechnologyParams& params)
    : tech_params_(params) {

    std::cout << "Creating power model:" << std::endl;
    std::cout << "  Technology node: " << tech_params_.tech_node_nm << " nm" << std::endl;
    std::cout << "  Device type: " << tech_params_.device_type << std::endl;
    std::cout << "  Temperature: " << tech_params_.temperature_k << " K" << std::endl;
    std::cout << "  Frequency: " << tech_params_.frequency_ghz << " GHz" << std::endl;
}

} // namespace pimid

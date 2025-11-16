#include "memory_models/memory_model.h"
#include <iostream>

namespace pimid {

//=============================================================================
// MemoryModelFactory Implementation
//=============================================================================

std::shared_ptr<MemoryModel> MemoryModelFactory::createMemoryModel(
    MemoryTechnology tech,
    const std::string& config_path) {

    std::cout << "Creating memory model for technology: "
              << static_cast<int>(tech) << std::endl;
    std::cout << "Configuration path: " << config_path << std::endl;

    switch (tech) {
        case MemoryTechnology::DRAM:
            // TODO: Return actual DRAMModel when implemented
            std::cout << "  Creating DRAM model (Ramulator)" << std::endl;
            return nullptr;

        case MemoryTechnology::SRAM:
            // TODO: Return actual SRAMModel when implemented
            std::cout << "  Creating SRAM model (CACTI)" << std::endl;
            return nullptr;

        case MemoryTechnology::STT_MRAM:
            // TODO: Return actual NVMModel when implemented
            std::cout << "  Creating STT-MRAM model (NVSim)" << std::endl;
            return nullptr;

        default:
            std::cerr << "Unknown memory technology: " << static_cast<int>(tech) << std::endl;
            return nullptr;
    }
}

} // namespace pimid

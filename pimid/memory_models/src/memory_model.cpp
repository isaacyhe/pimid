#include "memory_model.h"
#include "dram_model.h"
#include "sram_model.h"
#include "nvm_model.h"
#include <iostream>
#include <stdexcept>

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

    try {
        switch (tech) {
            case MemoryTechnology::DRAM:
                std::cout << "  Creating DRAM model (Ramulator)" << std::endl;
                return std::make_shared<DRAMModel>(config_path);

            case MemoryTechnology::SRAM:
                std::cout << "  Creating SRAM model (CACTI)" << std::endl;
                return std::make_shared<SRAMModel>(config_path);

            case MemoryTechnology::STT_MRAM:
                std::cout << "  Creating STT-MRAM model (NVSim)" << std::endl;
                return std::make_shared<NVMModel>(config_path);

            default:
                throw std::runtime_error("Unknown memory technology: " +
                                       std::to_string(static_cast<int>(tech)));
        }
    } catch (const std::exception& e) {
        std::cerr << "Error creating memory model: " << e.what() << std::endl;
        throw;
    }
}

} // namespace pimid

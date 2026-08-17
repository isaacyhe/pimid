#include "memory/memory_model.h"
#include "memory/dram_model.h"
#include "memory/sram_model.h"
/* 1.11.57 (latent D051): memory/nvm_model.h is gone -- NVMModel was never
 * constructed by this factory or anything else, and carried its own set of
 * hard-coded fallback energies and areas. STT_MRAM/PCM/ReRAM route to the
 * three models below, which is what they have always done. */
#include "memory/sttmram_model.h"
#include "memory/pcm_model.h"
#include "memory/reram_model.h"
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
            case MemoryTechnology::DDR3:
            case MemoryTechnology::DDR4:
            case MemoryTechnology::DDR5:
            case MemoryTechnology::LPDDR5:
            case MemoryTechnology::GDDR6:
            case MemoryTechnology::HBM2:
            case MemoryTechnology::HBM3:
                /* 1.11.52 (D024): pass the TECHNOLOGY. Seven DRAM
                 * generations arrive here and all seven used to become a
                 * DDR4 model. */
                std::cout << "  Creating DRAM model (Ramulator) for "
                          << static_cast<int>(tech) << std::endl;
                return std::make_shared<DRAMModel>(config_path, tech);

            case MemoryTechnology::SRAM:
                std::cout << "  Creating SRAM model (CACTI) with inner-bank timing" << std::endl;
                return std::make_shared<SRAMModel>(config_path);

            case MemoryTechnology::STT_MRAM:
                std::cout << "  Creating STT-MRAM model (NVSim) with inner-bank timing" << std::endl;
                return std::make_shared<STTMRAMModel>(config_path);

            case MemoryTechnology::PCM:
                std::cout << "  Creating PCM model (NVSim) with inner-bank timing" << std::endl;
                return std::make_shared<PCMModel>(config_path);

            case MemoryTechnology::ReRAM:
                std::cout << "  Creating ReRAM model (NVSim) with analog compute support" << std::endl;
                return std::make_shared<ReRAMModel>(config_path);

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

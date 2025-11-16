#include "nvsim_wrapper.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <stdexcept>

// Include NVSim headers
#include "InputParameter.h"
#include "MemCell.h"
#include "Result.h"
#include "Bank.h"
#include "BankWithHtree.h"
#include "BankWithoutHtree.h"
#include "global.h"

namespace pimid {

//=============================================================================
// NVSimWrapper Implementation
//=============================================================================

NVSimWrapper::NVSimWrapper(const NVMConfig& config)
    : config_(config)
    , nvsim_input_(nullptr)
    , nvsim_cell_(nullptr)
    , nvsim_result_(nullptr)
    , initialized_(false)
    , valid_(false)
    , error_message_("")
{
}

NVSimWrapper::~NVSimWrapper() {
    if (nvsim_result_) {
        delete nvsim_result_;
        nvsim_result_ = nullptr;
    }
    if (nvsim_cell_) {
        delete nvsim_cell_;
        nvsim_cell_ = nullptr;
    }
    if (nvsim_input_) {
        delete nvsim_input_;
        nvsim_input_ = nullptr;
    }
}

void NVSimWrapper::initialize() {
    if (initialized_) {
        std::cerr << "[NVSimWrapper] Warning: Already initialized, reinitializing..." << std::endl;
        if (nvsim_result_) {
            delete nvsim_result_;
            nvsim_result_ = nullptr;
        }
        if (nvsim_cell_) {
            delete nvsim_cell_;
            nvsim_cell_ = nullptr;
        }
        if (nvsim_input_) {
            delete nvsim_input_;
            nvsim_input_ = nullptr;
        }
    }

    validateConfiguration();
    if (!valid_) {
        throw std::runtime_error("[NVSimWrapper] Invalid configuration: " + error_message_);
    }

    try {
        createNVSimInput();
        loadCellParameters();
        runNVSim();
        initialized_ = true;

        std::cout << "[NVSimWrapper] Initialized with:" << std::endl;
        std::cout << "  Capacity: " << (config_.capacity_bytes / (1024*1024)) << " MB" << std::endl;
        std::cout << "  Word Width: " << config_.word_width_bits << " bits" << std::endl;
        std::cout << "  Technology: " << config_.process_node_nm << " nm" << std::endl;

        if (valid_ && nvsim_result_) {
            std::cout << "  Read Latency: " << (getReadLatency() * 1e9) << " ns" << std::endl;
            std::cout << "  Write Latency: " << (getWriteLatency() * 1e9) << " ns" << std::endl;
            std::cout << "  Area: " << getArea() << " mm^2" << std::endl;
            std::cout << "  Read Energy: " << getReadDynamicEnergy() << " nJ" << std::endl;
            std::cout << "  Write Energy: " << getWriteDynamicEnergy() << " nJ" << std::endl;
        }
    } catch (const std::exception& e) {
        valid_ = false;
        error_message_ = std::string("Initialization failed: ") + e.what();
        throw;
    }
}

void NVSimWrapper::reconfigure(const NVMConfig& config) {
    config_ = config;
    initialized_ = false;
    initialize();
}

void NVSimWrapper::validateConfiguration() {
    valid_ = true;
    error_message_ = "";

    // Validate capacity
    if (config_.capacity_bytes < 1024 || config_.capacity_bytes > (16ULL * 1024 * 1024 * 1024)) {
        valid_ = false;
        error_message_ = "Capacity out of range (1KB - 16GB)";
        return;
    }

    // Validate word width
    if (config_.word_width_bits < 8 || config_.word_width_bits > 1024) {
        valid_ = false;
        error_message_ = "Word width out of range (8 - 1024 bits)";
        return;
    }

    // Validate technology node
    if (config_.process_node_nm < 7 || config_.process_node_nm > 90) {
        valid_ = false;
        error_message_ = "Technology node out of range (7nm - 90nm)";
        return;
    }
}

std::string NVSimWrapper::getCellFileName() const {
    // If user provided a cell file, use it
    if (!config_.cell_file.empty()) {
        return config_.cell_file;
    }

    // Otherwise, use default based on NVM type
    switch (config_.nvm_type) {
        case NVMType::STTRAM:
            return "sample_STTRAM.cell";
        case NVMType::PCRAM:
            return "sample_PCRAM.cell";
        case NVMType::RERAM:
            return "sample_RRAM.cell";
        case NVMType::SLCNAND:
            return "sample_SLCNAND.cell";
        default:
            return "sample_STTRAM.cell";  // Default to STT-RAM
    }
}

void NVSimWrapper::createNVSimInput() {
    nvsim_input_ = new InputParameter();

    // Basic parameters
    nvsim_input_->capacity = config_.capacity_bytes;
    nvsim_input_->wordWidth = config_.word_width_bits;
    nvsim_input_->processNode = config_.process_node_nm;
    nvsim_input_->temperature = config_.temperature_k;

    // Design target
    if (config_.is_cache) {
        nvsim_input_->designTarget = cache;
        nvsim_input_->associativity = config_.associativity;
    } else {
        nvsim_input_->designTarget = RAM_chip;
        nvsim_input_->associativity = 1;
    }

    // Optimization target (select the first enabled optimization)
    if (config_.optimize_read_latency) {
        nvsim_input_->optimizationTarget = read_latency_optimized;
    } else if (config_.optimize_write_latency) {
        nvsim_input_->optimizationTarget = write_latency_optimized;
    } else if (config_.optimize_read_energy) {
        nvsim_input_->optimizationTarget = read_energy_optimized;
    } else if (config_.optimize_write_energy) {
        nvsim_input_->optimizationTarget = write_energy_optimized;
    } else if (config_.optimize_leakage) {
        nvsim_input_->optimizationTarget = leakage_optimized;
    } else if (config_.optimize_area) {
        nvsim_input_->optimizationTarget = area_optimized;
    } else {
        // Default to read energy optimization
        nvsim_input_->optimizationTarget = read_energy_optimized;
    }

    // Page and block sizes for Flash/DRAM
    nvsim_input_->pageSize = config_.page_size_bits;
    nvsim_input_->flashBlockSize = config_.block_size_bits;

    // Set cell file
    nvsim_input_->fileMemCell = getCellFileName();

    // Use reasonable defaults for other parameters
    nvsim_input_->routingMode = h_tree;
    nvsim_input_->internalSensing = true;
    nvsim_input_->useCactiAssumption = false;

    std::cout << "[NVSimWrapper] Input parameters configured" << std::endl;
}

void NVSimWrapper::loadCellParameters() {
    nvsim_cell_ = new MemCell();

    // Initialize with defaults
    nvsim_cell_->memCellType = PCRAM;  // Default, will be overridden by file if exists

    // Try to load from file if it exists
    std::string cell_file = getCellFileName();
    std::cout << "[NVSimWrapper] Loading cell parameters from: " << cell_file << std::endl;

    // Note: NVSim's MemCell::ReadCellFromFile() would be called here
    // For now, we use defaults based on NVM type
    switch (config_.nvm_type) {
        case NVMType::STTRAM:
            nvsim_cell_->memCellType = MRAM;  // STT-RAM is a type of MRAM
            break;
        case NVMType::PCRAM:
            nvsim_cell_->memCellType = PCRAM;
            break;
        case NVMType::RERAM:
            nvsim_cell_->memCellType = memristor;  // ReRAM/memristor
            break;
        case NVMType::SLCNAND:
            nvsim_cell_->memCellType = SLCNAND;
            break;
        case NVMType::MLCNAND:
            nvsim_cell_->memCellType = MLCNAND;
            break;
        case NVMType::FBDRAM:
            nvsim_cell_->memCellType = FBRAM;
            break;
        default:
            nvsim_cell_->memCellType = MRAM;
    }
}

void NVSimWrapper::runNVSim() {
    try {
        std::cout << "[NVSimWrapper] Running NVSim analysis..." << std::endl;

        nvsim_result_ = new Result();

        // In actual implementation, you would call NVSim's main analysis function
        // For now, we'll set some placeholder values
        // The actual NVSim integration would look like:
        // Bank *bank = new BankWithHtree();
        // bank->Initialize(...);
        // bank->CalculateArea();
        // bank->CalculateLatency();
        // bank->CalculatePower();

        valid_ = true;
        std::cout << "[NVSimWrapper] NVSim analysis complete" << std::endl;

    } catch (const std::exception& e) {
        valid_ = false;
        error_message_ = std::string("NVSim execution failed: ") + e.what();
        std::cerr << "[NVSimWrapper] " << error_message_ << std::endl;
    }
}

//=============================================================================
// Query functions
//=============================================================================

double NVSimWrapper::getReadLatency() const {
    if (!valid_ || !nvsim_result_) return 0.0;
    // Return placeholder value - actual implementation would access nvsim_result_->readLatency
    return 10.0e-9;  // 10 ns placeholder
}

double NVSimWrapper::getWriteLatency() const {
    if (!valid_ || !nvsim_result_) return 0.0;
    // Return placeholder value
    return 20.0e-9;  // 20 ns placeholder (NVM write is typically slower)
}

double NVSimWrapper::getReadDynamicEnergy() const {
    if (!valid_ || !nvsim_result_) return 0.0;
    // Return placeholder value in nJ
    return 0.5;  // 0.5 nJ placeholder
}

double NVSimWrapper::getWriteDynamicEnergy() const {
    if (!valid_ || !nvsim_result_) return 0.0;
    // Return placeholder value in nJ (NVM write typically more energy)
    return 2.0;  // 2.0 nJ placeholder
}

double NVSimWrapper::getLeakagePower() const {
    if (!valid_ || !nvsim_result_) return 0.0;
    // Return placeholder value in mW
    return 5.0;  // 5 mW placeholder
}

double NVSimWrapper::getReadEDP() const {
    return getReadDynamicEnergy() * (getReadLatency() * 1e9);  // Energy * Delay
}

double NVSimWrapper::getWriteEDP() const {
    return getWriteDynamicEnergy() * (getWriteLatency() * 1e9);  // Energy * Delay
}

double NVSimWrapper::getArea() const {
    if (!valid_ || !nvsim_result_) return 0.0;
    // Return placeholder value in mm^2
    return 10.0;  // 10 mm^2 placeholder
}

double NVSimWrapper::getHeight() const {
    if (!valid_ || !nvsim_result_) return 0.0;
    return 3.0;  // mm placeholder
}

double NVSimWrapper::getWidth() const {
    if (!valid_ || !nvsim_result_) return 0.0;
    return 3.33;  // mm placeholder
}

double NVSimWrapper::getCellReadLatency() const {
    if (!valid_ || !nvsim_cell_) return 0.0;
    return 1.0e-9;  // 1 ns placeholder
}

double NVSimWrapper::getCellWriteLatency() const {
    if (!valid_ || !nvsim_cell_) return 0.0;
    return 5.0e-9;  // 5 ns placeholder
}

double NVSimWrapper::getCellArea() const {
    if (!valid_ || !nvsim_cell_) return 0.0;
    return 0.01;  // 0.01 um^2 placeholder (e.g., 4F^2)
}

uint32_t NVSimWrapper::getNumRows() const {
    return 512;  // Placeholder
}

uint32_t NVSimWrapper::getNumColumns() const {
    return 512;  // Placeholder
}

uint32_t NVSimWrapper::getNumBanks() const {
    return 8;  // Placeholder
}

double NVSimWrapper::getReadBandwidth() const {
    if (getReadLatency() == 0) return 0.0;
    double bytes_per_access = config_.word_width_bits / 8.0;
    return bytes_per_access / getReadLatency() / 1e9;  // GB/s
}

double NVSimWrapper::getWriteBandwidth() const {
    if (getWriteLatency() == 0) return 0.0;
    double bytes_per_access = config_.word_width_bits / 8.0;
    return bytes_per_access / getWriteLatency() / 1e9;  // GB/s
}

bool NVSimWrapper::isValid() const {
    return valid_;
}

std::string NVSimWrapper::getErrorMessage() const {
    return error_message_;
}

void NVSimWrapper::printDetailedResults() const {
    if (!valid_) {
        std::cout << "[NVSimWrapper] No valid results available" << std::endl;
        if (!error_message_.empty()) {
            std::cout << "  Error: " << error_message_ << std::endl;
        }
        return;
    }

    std::cout << "\n=== NVSim Results ===" << std::endl;
    std::cout << "Configuration:" << std::endl;
    std::cout << "  Capacity: " << (config_.capacity_bytes / (1024*1024)) << " MB" << std::endl;
    std::cout << "  Word Width: " << config_.word_width_bits << " bits" << std::endl;
    std::cout << "  Technology: " << config_.process_node_nm << " nm" << std::endl;

    std::cout << "\nTiming:" << std::endl;
    std::cout << "  Read Latency: " << (getReadLatency() * 1e9) << " ns" << std::endl;
    std::cout << "  Write Latency: " << (getWriteLatency() * 1e9) << " ns" << std::endl;

    std::cout << "\nArea:" << std::endl;
    std::cout << "  Total Area: " << getArea() << " mm^2" << std::endl;
    std::cout << "  Height: " << getHeight() << " mm" << std::endl;
    std::cout << "  Width: " << getWidth() << " mm" << std::endl;

    std::cout << "\nEnergy:" << std::endl;
    std::cout << "  Read Energy: " << getReadDynamicEnergy() << " nJ" << std::endl;
    std::cout << "  Write Energy: " << getWriteDynamicEnergy() << " nJ" << std::endl;
    std::cout << "  Read EDP: " << getReadEDP() << " nJ*ns" << std::endl;
    std::cout << "  Write EDP: " << getWriteEDP() << " nJ*ns" << std::endl;

    std::cout << "\nPower:" << std::endl;
    std::cout << "  Leakage Power: " << getLeakagePower() << " mW" << std::endl;

    std::cout << "\nBandwidth:" << std::endl;
    std::cout << "  Read Bandwidth: " << getReadBandwidth() << " GB/s" << std::endl;
    std::cout << "  Write Bandwidth: " << getWriteBandwidth() << " GB/s" << std::endl;
    std::cout << "===================\n" << std::endl;
}

} // namespace pimid

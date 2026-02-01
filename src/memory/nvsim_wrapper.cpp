#include "memory/nvsim_wrapper.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstring>
#include <stdexcept>

// Include NVSim headers if available
#ifdef HAVE_NVSIM
#include "InputParameter.h"
#include "MemCell.h"
#include "Result.h"
#include "Bank.h"
#include "BankWithHtree.h"
#include "BankWithoutHtree.h"
#include "Technology.h"
#include "Wire.h"
#include "global.h"

// NVSim global pointers are declared as 'extern' in global.h and defined
// in nvsim_globals.cpp. We access them here - do NOT redefine them.
// The globals are: nvsim::inputParameter, nvsim::tech, nvsim::cell, etc.

// Use nvsim namespace to access NVSim types and globals
using namespace nvsim;
#endif

namespace pimid {

#ifdef HAVE_NVSIM

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
    // Clean up in reverse order of creation
    // Result first (it may reference Bank which uses other objects)
    if (nvsim_result_) {
        delete nvsim_result_;
        nvsim_result_ = nullptr;
    }

    // Clean up Technology and Wire that we allocated in runNVSim()
    // These are owned by us since we created them
    if (tech != nullptr) {
        delete tech;
        tech = nullptr;
    }
    if (localWire != nullptr) {
        delete localWire;
        localWire = nullptr;
    }
    if (globalWire != nullptr) {
        delete globalWire;
        globalWire = nullptr;
    }

    // Clean up cell - reset global first to avoid dangling pointer
    if (nvsim_cell_) {
        if (cell == nvsim_cell_) {
            cell = nullptr;
        }
        delete nvsim_cell_;
        nvsim_cell_ = nullptr;
    }

    // Clean up input - reset global first to avoid dangling pointer
    if (nvsim_input_) {
        if (inputParameter == nvsim_input_) {
            inputParameter = nullptr;
        }
        delete nvsim_input_;
        nvsim_input_ = nullptr;
    }
}

void NVSimWrapper::initialize() {
    if (initialized_) {
        std::cerr << "[NVSimWrapper] Warning: Already initialized, reinitializing..." << std::endl;

        // Clean up in reverse order of creation (same as destructor)
        if (nvsim_result_) {
            delete nvsim_result_;
            nvsim_result_ = nullptr;
        }

        // Clean up Technology and Wire
        if (tech != nullptr) {
            delete tech;
            tech = nullptr;
        }
        if (localWire != nullptr) {
            delete localWire;
            localWire = nullptr;
        }
        if (globalWire != nullptr) {
            delete globalWire;
            globalWire = nullptr;
        }

        // Clean up cell and reset global
        if (nvsim_cell_) {
            if (cell == nvsim_cell_) {
                cell = nullptr;
            }
            delete nvsim_cell_;
            nvsim_cell_ = nullptr;
        }

        // Clean up input and reset global
        if (nvsim_input_) {
            if (inputParameter == nvsim_input_) {
                inputParameter = nullptr;
            }
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
    // NVSim classes are now in the nvsim:: namespace (local copy in external/nvsim)
    // to avoid symbol collision with CACTI's global namespace classes.
    nvsim_input_ = new InputParameter();

    // Basic parameters
    nvsim_input_->capacity = config_.capacity_bytes;
    nvsim_input_->wordWidth = config_.word_width_bits;
    nvsim_input_->processNode = config_.process_node_nm;
    nvsim_input_->temperature = config_.temperature_k;

    // Device roadmap - IMPORTANT: must be set for Technology initialization
    // Default to HP (high performance) if not specified
    nvsim_input_->deviceRoadmap = HP;

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

    // NOTE: Don't set global inputParameter here - do it in runNVSim() after all
    // objects are created, to match NVSim's main() initialization order

    std::cout << "[NVSimWrapper] Input parameters configured" << std::endl;
}

void NVSimWrapper::loadCellParameters() {
    // MemCell is in nvsim:: namespace (no collision with other libraries)
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

    // NOTE: Don't set global cell here - do it in runNVSim() to match
    // NVSim's main() initialization order (after Technology is initialized)
}

void NVSimWrapper::runNVSim() {
    try {
        std::cout << "[NVSimWrapper] Running NVSim analysis..." << std::endl;

        // Set global pointers in the same order as NVSim's main():
        // 1. inputParameter
        // 2. tech (Technology)
        // 3. cell
        // 4. localWire/globalWire
        // 5. Then Result can be created

        // Set global inputParameter first
        inputParameter = nvsim_input_;
        std::cout << "[NVSimWrapper] Global inputParameter set" << std::endl;

        // Initialize global Technology object
        if (tech == nullptr) {
            tech = new Technology();
        }
        tech->Initialize(nvsim_input_->processNode, nvsim_input_->deviceRoadmap);
        std::cout << "[NVSimWrapper] Technology initialized" << std::endl;

        // Set global cell pointer
        cell = nvsim_cell_;
        std::cout << "[NVSimWrapper] Global cell set" << std::endl;

        // Initialize global Wire objects
        if (localWire == nullptr) {
            localWire = new Wire();
        }
        if (globalWire == nullptr) {
            globalWire = new Wire();
        }
        std::cout << "[NVSimWrapper] Wire objects created" << std::endl;

        // Now safe to create Result - all global dependencies are initialized
        // Result::Result() accesses inputParameter->routingMode to decide
        // whether to create BankWithHtree or BankWithoutHtree
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

//=============================================================================
// Subarray-Level Characteristics
//=============================================================================

double NVSimWrapper::getDecoderDelay() const {
    if (!valid_ || !nvsim_result_ || !nvsim_result_->bank) return 0.0;
    // Access subarray's row decoder timing
    // nvsim_result_->bank is a Bank* which contains mat which contains subarray
    return 0.5e-9;  // Placeholder until full integration
}

double NVSimWrapper::getWordlineDelay() const {
    if (!valid_ || !nvsim_result_) return 0.0;
    // Wordline delay is a significant portion of access time
    return getReadLatency() * 0.2;  // Approximately 20% of read latency
}

double NVSimWrapper::getBitlineDelay() const {
    if (!valid_ || !nvsim_result_) return 0.0;
    // Bitline delay is the dominant component in NVM
    return getReadLatency() * 0.5;  // Approximately 50% of read latency
}

double NVSimWrapper::getSenseAmpDelay() const {
    if (!valid_ || !nvsim_result_) return 0.0;
    // Sense amp delay
    return getReadLatency() * 0.15;  // Approximately 15% of read latency
}

double NVSimWrapper::getColumnDecoderDelay() const {
    if (!valid_ || !nvsim_result_) return 0.0;
    // Column mux/decoder delay
    return getReadLatency() * 0.1;  // Approximately 10% of read latency
}

double NVSimWrapper::getPrechargeDelay() const {
    if (!valid_ || !nvsim_result_) return 0.0;
    // Precharge delay (typically less critical for NVM)
    return getReadLatency() * 0.05;  // Approximately 5% of read latency
}

uint32_t NVSimWrapper::getSubarrayRows() const {
    return 512;  // Typical NVM subarray size
}

uint32_t NVSimWrapper::getSubarrayCols() const {
    return 512;  // Typical NVM subarray size
}

uint32_t NVSimWrapper::getSubarraysPerMat() const {
    return 4;  // Typically 2x2 subarray arrangement
}

uint32_t NVSimWrapper::getMatsPerBank() const {
    return 8;  // Typical value
}

uint32_t NVSimWrapper::getNumSenseAmps() const {
    // Number of sense amps per subarray (depends on mux ratio)
    return getSubarrayCols() / 8;  // Assuming 8:1 mux ratio
}

double NVSimWrapper::getWordlineLength() const {
    // Estimate from technology node and subarray columns
    double cell_pitch = config_.process_node_nm * 2e-9;  // 2F pitch
    return getSubarrayCols() * cell_pitch;  // in meters
}

double NVSimWrapper::getBitlineLength() const {
    // Estimate from technology node and subarray rows
    double cell_pitch = config_.process_node_nm * 2e-9;  // 2F pitch
    return getSubarrayRows() * cell_pitch;  // in meters
}

double NVSimWrapper::getWordlineCapacitance() const {
    // Estimate wordline capacitance
    // C_wl ~ length * capacitance_per_um (~0.2 fF/um for metal)
    return getWordlineLength() * 0.2e-9;  // F
}

double NVSimWrapper::getWordlineResistance() const {
    // Estimate wordline resistance
    // R_wl ~ length * resistance_per_um (~10 Ohm/um for metal)
    return getWordlineLength() * 10e6;  // Ohm
}

double NVSimWrapper::getBitlineCapacitance() const {
    // Estimate bitline capacitance
    return getBitlineLength() * 0.3e-9;  // F (bitlines typically higher C)
}

double NVSimWrapper::getBitlineResistance() const {
    // Estimate bitline resistance
    return getBitlineLength() * 15e6;  // Ohm
}

double NVSimWrapper::getDecoderEnergy() const {
    return getReadDynamicEnergy() * 0.1;  // ~10% of read energy
}

double NVSimWrapper::getWordlineEnergy() const {
    return getReadDynamicEnergy() * 0.2;  // ~20% of read energy
}

double NVSimWrapper::getBitlineEnergy() const {
    return getReadDynamicEnergy() * 0.4;  // ~40% of read energy (dominant)
}

double NVSimWrapper::getSenseAmpEnergy() const {
    return getReadDynamicEnergy() * 0.2;  // ~20% of read energy
}

double NVSimWrapper::getPrechargerEnergy() const {
    return getReadDynamicEnergy() * 0.1;  // ~10% of read energy
}

double NVSimWrapper::getDecoderLeakage() const {
    return getLeakagePower() * 0.15;  // ~15% of total leakage
}

double NVSimWrapper::getWordlineLeakage() const {
    return getLeakagePower() * 0.25;  // ~25% of total leakage
}

double NVSimWrapper::getSenseAmpLeakage() const {
    return getLeakagePower() * 0.3;  // ~30% of total leakage (dominant for analog circuits)
}

double NVSimWrapper::getSubarrayArea() const {
    // Estimate from cell area and organization
    double cell_area_um2 = getCellArea();
    double subarray_cells = getSubarrayRows() * getSubarrayCols();
    return cell_area_um2 * subarray_cells * 1e-6;  // Convert um^2 to mm^2
}

double NVSimWrapper::getMatArea() const {
    return getSubarrayArea() * getSubarraysPerMat() * 1.2;  // 20% overhead for decoders
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

    std::cout << "\nTiming Breakdown (subarray-level):" << std::endl;
    std::cout << "  Decoder Delay: " << (getDecoderDelay() * 1e9) << " ns" << std::endl;
    std::cout << "  Wordline Delay: " << (getWordlineDelay() * 1e9) << " ns" << std::endl;
    std::cout << "  Bitline Delay: " << (getBitlineDelay() * 1e9) << " ns" << std::endl;
    std::cout << "  Sense Amp Delay: " << (getSenseAmpDelay() * 1e9) << " ns" << std::endl;
    std::cout << "  Column Decoder Delay: " << (getColumnDecoderDelay() * 1e9) << " ns" << std::endl;

    std::cout << "\nSubarray Organization:" << std::endl;
    std::cout << "  Rows per Subarray: " << getSubarrayRows() << std::endl;
    std::cout << "  Cols per Subarray: " << getSubarrayCols() << std::endl;
    std::cout << "  Subarrays per Mat: " << getSubarraysPerMat() << std::endl;
    std::cout << "  Mats per Bank: " << getMatsPerBank() << std::endl;
    std::cout << "  Sense Amplifiers: " << getNumSenseAmps() << std::endl;

    std::cout << "\nElectrical Parameters:" << std::endl;
    std::cout << "  Wordline Length: " << (getWordlineLength() * 1e6) << " um" << std::endl;
    std::cout << "  Bitline Length: " << (getBitlineLength() * 1e6) << " um" << std::endl;
    std::cout << "  Wordline Capacitance: " << (getWordlineCapacitance() * 1e15) << " fF" << std::endl;
    std::cout << "  Wordline Resistance: " << getWordlineResistance() << " Ohm" << std::endl;
    std::cout << "  Bitline Capacitance: " << (getBitlineCapacitance() * 1e15) << " fF" << std::endl;
    std::cout << "  Bitline Resistance: " << getBitlineResistance() << " Ohm" << std::endl;

    std::cout << "\nArea:" << std::endl;
    std::cout << "  Total Area: " << getArea() << " mm^2" << std::endl;
    std::cout << "  Height: " << getHeight() << " mm" << std::endl;
    std::cout << "  Width: " << getWidth() << " mm" << std::endl;
    std::cout << "  Subarray Area: " << (getSubarrayArea() * 1e6) << " um^2" << std::endl;
    std::cout << "  Mat Area: " << (getMatArea() * 1e6) << " um^2" << std::endl;

    std::cout << "\nEnergy:" << std::endl;
    std::cout << "  Read Energy: " << getReadDynamicEnergy() << " nJ" << std::endl;
    std::cout << "  Write Energy: " << getWriteDynamicEnergy() << " nJ" << std::endl;
    std::cout << "  Read EDP: " << getReadEDP() << " nJ*ns" << std::endl;
    std::cout << "  Write EDP: " << getWriteEDP() << " nJ*ns" << std::endl;

    std::cout << "\nEnergy Breakdown (subarray-level):" << std::endl;
    std::cout << "  Decoder Energy: " << getDecoderEnergy() << " nJ" << std::endl;
    std::cout << "  Wordline Energy: " << getWordlineEnergy() << " nJ" << std::endl;
    std::cout << "  Bitline Energy: " << getBitlineEnergy() << " nJ" << std::endl;
    std::cout << "  Sense Amp Energy: " << getSenseAmpEnergy() << " nJ" << std::endl;

    std::cout << "\nPower:" << std::endl;
    std::cout << "  Leakage Power: " << getLeakagePower() << " mW" << std::endl;

    std::cout << "\nLeakage Breakdown (subarray-level):" << std::endl;
    std::cout << "  Decoder Leakage: " << getDecoderLeakage() << " mW" << std::endl;
    std::cout << "  Wordline Leakage: " << getWordlineLeakage() << " mW" << std::endl;
    std::cout << "  Sense Amp Leakage: " << getSenseAmpLeakage() << " mW" << std::endl;

    std::cout << "\nBandwidth:" << std::endl;
    std::cout << "  Read Bandwidth: " << getReadBandwidth() << " GB/s" << std::endl;
    std::cout << "  Write Bandwidth: " << getWriteBandwidth() << " GB/s" << std::endl;
    std::cout << "===================\n" << std::endl;
}

#else  // !HAVE_NVSIM

//=============================================================================
// NVSimWrapper Stub Implementation (WITHOUT NVSIM)
//=============================================================================

NVSimWrapper::NVSimWrapper(const NVMConfig& config)
    : config_(config)
    , nvsim_input_(nullptr)
    , nvsim_cell_(nullptr)
    , nvsim_result_(nullptr)
    , initialized_(false)
    , valid_(true)
    , error_message_("")
{
    std::cerr << "[NVSimWrapper] WARNING: NVSim not available, using placeholder values" << std::endl;
}

NVSimWrapper::~NVSimWrapper() {}

void NVSimWrapper::initialize() {
    initialized_ = true;
    std::cout << "[NVSimWrapper] Using placeholder values (NVSim not compiled)" << std::endl;
}

void NVSimWrapper::reconfigure(const NVMConfig& config) {
    config_ = config;
    initialized_ = false;
}

void NVSimWrapper::runNVSim() {}
void NVSimWrapper::validateConfiguration() { valid_ = true; }
void NVSimWrapper::createNVSimInput() {}
void NVSimWrapper::loadCellParameters() {}

bool NVSimWrapper::isValid() const { return true; }
std::string NVSimWrapper::getErrorMessage() const { return ""; }
std::string NVSimWrapper::getCellFileName() const { return ""; }

// Return placeholder values
double NVSimWrapper::getReadLatency() const { return 5.0e-9; }  // 5ns
double NVSimWrapper::getWriteLatency() const { return 15.0e-9; } // 15ns (asymmetric)
double NVSimWrapper::getReadDynamicEnergy() const { return 0.5; }
double NVSimWrapper::getWriteDynamicEnergy() const { return 1.0; }
double NVSimWrapper::getLeakagePower() const { return 10.0; }
double NVSimWrapper::getReadEDP() const { return 2.5; }
double NVSimWrapper::getWriteEDP() const { return 15.0; }
double NVSimWrapper::getArea() const { return 2.0; }
double NVSimWrapper::getHeight() const { return 1.0; }
double NVSimWrapper::getWidth() const { return 2.0; }
double NVSimWrapper::getCellReadLatency() const { return 5.0e-9; }
double NVSimWrapper::getCellWriteLatency() const { return 15.0e-9; }
double NVSimWrapper::getCellArea() const { return 0.01; }
double NVSimWrapper::getReadBandwidth() const { return 10.0; }
double NVSimWrapper::getWriteBandwidth() const { return 5.0; }

void NVSimWrapper::printDetailedResults() const {
    std::cout << "[NVSimWrapper] NVSim not available - using placeholder values" << std::endl;
}

// Stub implementations for subarray-level methods (without NVSim)
double NVSimWrapper::getDecoderDelay() const { return 0.5e-9; }        // 0.5 ns
double NVSimWrapper::getWordlineDelay() const { return 1.0e-9; }       // 1.0 ns
double NVSimWrapper::getBitlineDelay() const { return 2.5e-9; }        // 2.5 ns (dominant for NVM)
double NVSimWrapper::getSenseAmpDelay() const { return 0.75e-9; }      // 0.75 ns
double NVSimWrapper::getColumnDecoderDelay() const { return 0.5e-9; }  // 0.5 ns
double NVSimWrapper::getPrechargeDelay() const { return 0.25e-9; }     // 0.25 ns

uint32_t NVSimWrapper::getSubarrayRows() const { return 512; }
uint32_t NVSimWrapper::getSubarrayCols() const { return 512; }
uint32_t NVSimWrapper::getSubarraysPerMat() const { return 4; }
uint32_t NVSimWrapper::getMatsPerBank() const { return 8; }
uint32_t NVSimWrapper::getNumSenseAmps() const { return 64; }

double NVSimWrapper::getWordlineLength() const { return 100e-6; }      // 100 um
double NVSimWrapper::getBitlineLength() const { return 100e-6; }       // 100 um
double NVSimWrapper::getWordlineCapacitance() const { return 50e-15; } // 50 fF
double NVSimWrapper::getWordlineResistance() const { return 500.0; }   // 500 Ohm
double NVSimWrapper::getBitlineCapacitance() const { return 80e-15; }  // 80 fF
double NVSimWrapper::getBitlineResistance() const { return 800.0; }    // 800 Ohm

double NVSimWrapper::getDecoderEnergy() const { return 0.05; }         // 0.05 nJ
double NVSimWrapper::getWordlineEnergy() const { return 0.1; }         // 0.1 nJ
double NVSimWrapper::getBitlineEnergy() const { return 0.2; }          // 0.2 nJ
double NVSimWrapper::getSenseAmpEnergy() const { return 0.1; }         // 0.1 nJ
double NVSimWrapper::getPrechargerEnergy() const { return 0.05; }      // 0.05 nJ

double NVSimWrapper::getDecoderLeakage() const { return 1.5; }         // 1.5 mW
double NVSimWrapper::getWordlineLeakage() const { return 2.5; }        // 2.5 mW
double NVSimWrapper::getSenseAmpLeakage() const { return 3.0; }        // 3.0 mW

double NVSimWrapper::getSubarrayArea() const { return 0.0005; }        // 0.0005 mm^2
double NVSimWrapper::getMatArea() const { return 0.0024; }             // 0.0024 mm^2

#endif  // HAVE_NVSIM

} // namespace pimid

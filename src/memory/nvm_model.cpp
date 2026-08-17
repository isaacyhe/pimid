#include "memory/nvm_model.h"
#include "memory/nvsim_wrapper.h"
#include "memory/architecture_extractor.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <yaml-cpp/yaml.h>

namespace pimid {

//=============================================================================
// NVMModel Implementation (NVSim Integration)
//=============================================================================

NVMModel::~NVMModel() = default;

NVMModel::NVMModel(const std::string& config_path)
    : MemoryModel(MemoryTechnology::STT_MRAM, config_path)
    , nvsim_instance_(nullptr)
    , total_reads_(0)
    , total_writes_(0)
    , write_cycles_(0)
    , read_energy_(0.0)
    , write_energy_(0.0)
    , leakage_power_(0.0)
    , area_mm2_(0.0)
    , current_cycle_(0)
    , capacity_(0)
    , bandwidth_(0)
    , endurance_(0) {

    // Initialize default NVM configuration (STT-MRAM)
    nvm_config_.cell_type = "STT-MRAM";
    nvm_config_.capacity = 1ULL * 1024 * 1024 * 1024;  // 1GB
    nvm_config_.banks = 8;
    nvm_config_.read_write_ports = 1;
    nvm_config_.tech_node_nm = 22;      // 22nm technology
    nvm_config_.read_latency = 10;      // 10 cycles (faster than DRAM)
    nvm_config_.write_latency = 50;     // 50 cycles (slower write)
    nvm_config_.endurance = 1e15;       // 10^15 writes (high for STT-MRAM)
    nvm_config_.is_pim_enabled = true;  // Support in-memory compute
}

void NVMModel::initialize() {
    std::cout << "[NVMModel] Initializing NVM model..." << std::endl;
    loadConfig(config_path_);

    // Calculate derived parameters
    capacity_ = nvm_config_.capacity;
    bandwidth_ = capacity_ / 100; // Simplified bandwidth model
    endurance_ = nvm_config_.endurance;

    // Initialize NVSim for accurate modeling (if HAVE_NVSIM is defined)
    initializeNVSim();

    // Extract architecture specification based on cell type
    std::string cell_type_lower = nvm_config_.cell_type;
    std::transform(cell_type_lower.begin(), cell_type_lower.end(),
                   cell_type_lower.begin(), ::tolower);

    bool is_sttmram = (cell_type_lower.find("stt") != std::string::npos ||
                       cell_type_lower.find("mram") != std::string::npos);
    bool is_pcm = (cell_type_lower.find("pcm") != std::string::npos);
    bool is_reram = (cell_type_lower.find("reram") != std::string::npos ||
                     cell_type_lower.find("rram") != std::string::npos);

    // Try to extract architecture from NVSim wrapper
#ifdef HAVE_NVSIM
    if (nvsim_wrapper_ && nvsim_wrapper_->isValid()) {
        if (is_sttmram) {
            sttmram_arch_ = memory::extractSTTMRAMArchitecture(
                *nvsim_wrapper_, "STT-MRAM-NVSim-Extracted");
            if (sttmram_arch_) {
                std::cout << "[NVMModel] STT-MRAM architecture EXTRACTED from NVSim" << std::endl;
            }
        } else if (is_pcm) {
            pcm_arch_ = memory::extractPCMArchitecture(
                *nvsim_wrapper_, "PCM-NVSim-Extracted");
            if (pcm_arch_) {
                std::cout << "[NVMModel] PCM architecture EXTRACTED from NVSim" << std::endl;
            }
        } else if (is_reram) {
            // Determine if analog capable based on config
            bool analog_capable = nvm_config_.is_pim_enabled;
            reram_arch_ = memory::extractReRAMArchitecture(
                *nvsim_wrapper_, "ReRAM-NVSim-Extracted", analog_capable);
            if (reram_arch_) {
                std::cout << "[NVMModel] ReRAM architecture EXTRACTED from NVSim" << std::endl;
            }
        }
    }
#endif

    /* 1.11.24: factory fallbacks REMOVED. They filled the architecture with
     * hand-written specs when NVSim characterization failed, and nothing
     * downstream could tell the difference between those and a real tool
     * read -- the run simply reported numbers. 678 literal assignments across
     * 19 create*() factories, none derivable from anything. A technology
     * whose tool binding fails must REFUSE. */
    if ((is_sttmram && !sttmram_arch_) || (is_pcm && !pcm_arch_) ||
        (is_reram && !reram_arch_)) {
        throw std::runtime_error(
            "[NVMModel] NVSim characterization failed and there is no "
            "fallback. The hand-written default architectures were removed "
            "in 1.11.24 because they were unsourced and indistinguishable "
            "from tool output. Fix the NVSim configuration rather than "
            "pricing this run from invented numbers.");
    }

    // Set energy values from architecture if available, otherwise use defaults
    if (is_sttmram && sttmram_arch_) {
        read_energy_ = sttmram_arch_->energy.bank_read_energy_pJ / 1000.0;  // pJ to nJ
        write_energy_ = sttmram_arch_->energy.bank_write_energy_pJ / 1000.0;
        leakage_power_ = sttmram_arch_->energy.chip_leakage_mw / 1000.0;  // mW to W
        area_mm2_ = 15.0;  // Approximate
    } else if (is_pcm && pcm_arch_) {
        read_energy_ = pcm_arch_->energy.bank_read_energy_pJ / 1000.0;
        // PCM has separate SET and RESET energies; use SET as primary write energy
        write_energy_ = pcm_arch_->energy.bank_set_energy_pJ / 1000.0;
        leakage_power_ = pcm_arch_->energy.chip_leakage_mw / 1000.0;
        area_mm2_ = 12.0;
    } else if (is_reram && reram_arch_) {
        read_energy_ = reram_arch_->energy.bank_read_energy_pJ / 1000.0;
        write_energy_ = reram_arch_->energy.bank_write_energy_pJ / 1000.0;
        leakage_power_ = reram_arch_->energy.chip_leakage_mw / 1000.0;
        area_mm2_ = 10.0;
    } else {
        // Ultimate fallback: hard-coded defaults based on cell type
        if (is_sttmram) {
            read_energy_ = 0.3;      // nJ per read (lower than DRAM)
            write_energy_ = 5.0;     // nJ per write (higher due to spin torque)
            leakage_power_ = 0.01;   // W (very low leakage)
            area_mm2_ = 15.0;        // mm^2
        } else if (is_pcm) {
            read_energy_ = 1.0;      // nJ per read
            write_energy_ = 20.0;    // nJ per write (high write energy)
            leakage_power_ = 0.02;   // W
            area_mm2_ = 12.0;        // mm^2
        } else if (is_reram) {
            read_energy_ = 0.5;      // nJ per read
            write_energy_ = 8.0;     // nJ per write
            leakage_power_ = 0.015;  // W
            area_mm2_ = 10.0;        // mm^2
        }
    }

    std::cout << "[NVMModel] Configuration:" << std::endl;
    std::cout << "  Cell Type: " << nvm_config_.cell_type << std::endl;
    std::cout << "  Capacity: " << (nvm_config_.capacity / (1024.0 * 1024 * 1024)) << " GB" << std::endl;
    std::cout << "  Banks: " << nvm_config_.banks << std::endl;
    std::cout << "  Ports (RW): " << nvm_config_.read_write_ports << std::endl;
    std::cout << "  Technology: " << nvm_config_.tech_node_nm << " nm" << std::endl;
    std::cout << "  Read Latency: " << nvm_config_.read_latency << " cycles" << std::endl;
    std::cout << "  Write Latency: " << nvm_config_.write_latency << " cycles" << std::endl;
    std::cout << "  Endurance: " << nvm_config_.endurance << " writes" << std::endl;
    std::cout << "  PIM Enabled: " << (nvm_config_.is_pim_enabled ? "Yes" : "No") << std::endl;
    std::cout << "  Area: " << area_mm2_ << " mm^2" << std::endl;
    std::cout << "  Read Energy: " << read_energy_ << " nJ" << std::endl;
    std::cout << "  Write Energy: " << write_energy_ << " nJ" << std::endl;
    std::cout << "  Leakage Power: " << leakage_power_ << " W" << std::endl;
    std::cout << "[NVMModel] Initialization complete" << std::endl;
}

void NVMModel::loadConfig(const std::string& config_path) {
    std::cout << "[NVMModel] Loading configuration from: " << config_path << std::endl;

    try {
        YAML::Node config = YAML::LoadFile(config_path);

        if (!config["nvm"]) {
            std::cout << "[NVMModel] No 'nvm' section found, using default configuration" << std::endl;
            return;
        }

        YAML::Node nvm = config["nvm"];

        // Load NVM type
        if (nvm["type"]) {
            nvm_config_.cell_type = nvm["type"].as<std::string>();
        }

        // Load capacity
        if (nvm["capacity_mb"]) {
            nvm_config_.capacity = nvm["capacity_mb"].as<uint64_t>() * 1024ULL * 1024ULL;
        }

        // Load organization
        if (nvm["organization"]) {
            auto org = nvm["organization"];
            if (org["banks"]) {
                nvm_config_.banks = org["banks"].as<uint32_t>();
            }
            if (org["read_write_ports"]) {
                nvm_config_.read_write_ports = org["read_write_ports"].as<uint32_t>();
            }
        }

        // Load timing
        if (nvm["timing"]) {
            auto timing = nvm["timing"];
            if (timing["read_latency_cycles"]) {
                nvm_config_.read_latency = timing["read_latency_cycles"].as<uint32_t>();
            }
            if (timing["write_latency_cycles"]) {
                nvm_config_.write_latency = timing["write_latency_cycles"].as<uint32_t>();
            }
        }

        // Load reliability/endurance
        if (nvm["reliability"] && nvm["reliability"]["write_endurance"]) {
            nvm_config_.endurance = static_cast<uint64_t>(nvm["reliability"]["write_endurance"].as<double>());
        }

        // Load power parameters based on cell type
        std::string cell_type_lower = nvm_config_.cell_type;
        std::transform(cell_type_lower.begin(), cell_type_lower.end(), cell_type_lower.begin(), ::tolower);

        // Try to load technology-specific parameters
        std::string tech_key;
        if (cell_type_lower.find("stt") != std::string::npos || cell_type_lower.find("mram") != std::string::npos) {
            tech_key = "stt_mram";
        } else if (cell_type_lower.find("pcm") != std::string::npos) {
            tech_key = "pcm";
        } else if (cell_type_lower.find("reram") != std::string::npos || cell_type_lower.find("rram") != std::string::npos) {
            tech_key = "reram";
        }

        if (!tech_key.empty() && nvm[tech_key]) {
            auto tech = nvm[tech_key];
            if (tech["tech_node_nm"]) {
                nvm_config_.tech_node_nm = tech["tech_node_nm"].as<uint32_t>();
            }
            if (tech["read_energy_nj"]) {
                read_energy_ = tech["read_energy_nj"].as<double>();
            }
            if (tech["write_energy_nj"]) {
                write_energy_ = tech["write_energy_nj"].as<double>();
            }
            if (tech["leakage_power_mw"]) {
                leakage_power_ = tech["leakage_power_mw"].as<double>() / 1000.0;  // Convert mW to W
            }
            if (tech["area_mm2"]) {
                area_mm2_ = tech["area_mm2"].as<double>();
            }
        }

        std::cout << "[NVMModel] Successfully loaded configuration from YAML" << std::endl;
        std::cout << "[NVMModel] Cell type: " << nvm_config_.cell_type << std::endl;

    } catch (const YAML::Exception& e) {
        std::cerr << "[NVMModel] YAML parsing error: " << e.what() << std::endl;
        std::cerr << "[NVMModel] Using default configuration" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[NVMModel] Error loading config: " << e.what() << std::endl;
        std::cerr << "[NVMModel] Using default configuration" << std::endl;
    }
}

Cycle NVMModel::access(const MemoryRequest& req) {
    Cycle latency = nvm_config_.read_latency;  // safe default

    // NVM has asymmetric read/write latency
    switch (req.type) {
        case MemoryRequestType::READ:
            latency = nvm_config_.read_latency;
            total_reads_++;
            break;
        case MemoryRequestType::WRITE:
            latency = nvm_config_.write_latency;
            total_writes_++;
            write_cycles_++;
            updateEndurance(req.addr);
            if (write_cycles_ > endurance_) {
                std::cerr << "[NVMModel] WARNING: Endurance limit exceeded! ("
                          << write_cycles_ << " > " << endurance_ << ")" << std::endl;
            }
            break;
        case MemoryRequestType::ATOMIC:
            // Atomic operations: read + modify + write
            latency = nvm_config_.read_latency + nvm_config_.write_latency;
            total_reads_++;
            total_writes_++;
            write_cycles_++;
            break;
    }

    // Add queuing delay
    if (!pending_requests_.empty()) {
        latency += pending_requests_.size();
    }

    // Add to pending requests
    pending_requests_.push(req);

    return latency;
}

bool NVMModel::canAccept(const MemoryRequest& req) {
    // Check request queue capacity
    const size_t MAX_PENDING_REQUESTS = 32;
    if (pending_requests_.size() >= MAX_PENDING_REQUESTS) {
        return false;
    }

    // Check endurance limit for writes
    if (req.type == MemoryRequestType::WRITE ||
        req.type == MemoryRequestType::ATOMIC) {
        if (write_cycles_ >= endurance_) {
            std::cerr << "[NVMModel] Rejecting write: endurance limit reached" << std::endl;
            return false;
        }
    }

    return true;
}

void NVMModel::tick() {
    current_cycle_++;

    // Update energy models from NVSim periodically (every 10000 cycles)
#ifdef HAVE_NVSIM
    if (nvsim_wrapper_ && current_cycle_ % 10000 == 0) {
        // Re-query NVSim for updated energy based on activity patterns
        // This allows NVSim to model temperature-dependent leakage changes
    }
#endif

    // Process pending requests: latency is accounted for at access() time;
    // tick() just frees capacity by popping one request.
    if (!pending_requests_.empty()) {
        pending_requests_.pop();
    }

    // Leakage power is very low for NVM (key advantage)
}

Cycle NVMModel::getLatency(MemoryRequestType type) const {
    switch (type) {
        case MemoryRequestType::READ:
            return nvm_config_.read_latency;
        case MemoryRequestType::WRITE:
            return nvm_config_.write_latency;
        case MemoryRequestType::ATOMIC:
            return nvm_config_.read_latency + nvm_config_.write_latency;
        default:
            return nvm_config_.read_latency;
    }
}

double NVMModel::getTotalEnergy() const {
    // Total energy = dynamic energy + leakage energy
    double dynamic_energy = (total_reads_ * read_energy_) +
                           (total_writes_ * write_energy_);

    // Leakage energy = leakage_power * time
    double leakage_energy = leakage_power_ * (current_cycle_ / 1e9); // Assuming 1 GHz

    return dynamic_energy + leakage_energy;
}

void NVMModel::printStats() const {
    std::cout << "\n=== NVM Model Statistics ===" << std::endl;
    std::cout << "Cell Type: " << nvm_config_.cell_type << std::endl;
    std::cout << "Total Cycles: " << current_cycle_ << std::endl;
    std::cout << "Total Reads: " << total_reads_ << std::endl;
    std::cout << "Total Writes: " << total_writes_ << std::endl;
    std::cout << "Write Cycles (Endurance): " << write_cycles_ << " / " << endurance_ << std::endl;

    if (total_reads_ + total_writes_ > 0) {
        double read_ratio = static_cast<double>(total_reads_) /
                           (total_reads_ + total_writes_);
        double write_ratio = static_cast<double>(total_writes_) /
                            (total_reads_ + total_writes_);
        std::cout << "Read Ratio: " << (read_ratio * 100.0) << "%" << std::endl;
        std::cout << "Write Ratio: " << (write_ratio * 100.0) << "%" << std::endl;

        double endurance_used = (static_cast<double>(write_cycles_) / endurance_) * 100.0;
        std::cout << "Endurance Used: " << endurance_used << "%" << std::endl;
    }

    std::cout << "\nPhysical Characteristics:" << std::endl;
    std::cout << "  Area: " << area_mm2_ << " mm^2" << std::endl;
    std::cout << "  Technology: " << nvm_config_.tech_node_nm << " nm" << std::endl;
    std::cout << "  PIM Capable: " << (nvm_config_.is_pim_enabled ? "Yes" : "No") << std::endl;

    std::cout << "\nLatency:" << std::endl;
    std::cout << "  Read Latency: " << nvm_config_.read_latency << " cycles" << std::endl;
    std::cout << "  Write Latency: " << nvm_config_.write_latency << " cycles" << std::endl;
    std::cout << "  Write/Read Ratio: " <<
        (static_cast<double>(nvm_config_.write_latency) / nvm_config_.read_latency) << "x" << std::endl;

    std::cout << "\nEnergy Consumption:" << std::endl;
    std::cout << "  Read Energy (per access): " << read_energy_ << " nJ" << std::endl;
    std::cout << "  Write Energy (per access): " << write_energy_ << " nJ" << std::endl;
    std::cout << "  Total Read Energy: " << (total_reads_ * read_energy_) << " nJ" << std::endl;
    std::cout << "  Total Write Energy: " << (total_writes_ * write_energy_) << " nJ" << std::endl;
    std::cout << "  Leakage Power: " << leakage_power_ << " W" << std::endl;
    std::cout << "  Total Energy: " << getTotalEnergy() << " nJ" << std::endl;

    // Energy comparison
    if (total_reads_ + total_writes_ > 0) {
        double avg_energy = getTotalEnergy() / (total_reads_ + total_writes_);
        std::cout << "  Average Energy per Access: " << avg_energy << " nJ" << std::endl;
    }

    std::cout << "================================\n" << std::endl;
}

void NVMModel::resetStats() {
    total_reads_ = 0;
    total_writes_ = 0;
    write_cycles_ = 0;
    current_cycle_ = 0;
}

//=============================================================================
// Private Helper Functions
//=============================================================================

void NVMModel::updateEndurance(Address addr) {
    // Per-bank and per-page endurance tracking
    // Calculate bank and page from address
    uint32_t bank = static_cast<uint32_t>((addr >> 12) % nvm_config_.banks);
    uint64_t page = (addr >> 12) / nvm_config_.banks;

    // Track writes per bank
    bank_write_counts_[bank]++;

    // Track writes per page (sampled - every 1000th page tracked for memory efficiency)
    if (page % 1000 == 0) {
        page_write_counts_[page]++;

        // Check for hot pages (potential wear-out)
        // For NVM, hot pages may need wear-leveling
        uint64_t hot_threshold = endurance_ / 1000;  // Scaled by sampling factor
        if (page_write_counts_[page] > hot_threshold) {
            std::cerr << "[NVMModel] WARNING: Hot page detected at page "
                      << page << " with " << page_write_counts_[page]
                      << " writes (sampling 1:1000). Consider wear-leveling." << std::endl;
        }
    }

    // Report bank-level wear imbalance periodically
    if (write_cycles_ % 100000 == 0 && write_cycles_ > 0) {
        uint64_t max_bank_writes = 0;
        uint64_t min_bank_writes = UINT64_MAX;
        for (const auto& [bank_id, count] : bank_write_counts_) {
            max_bank_writes = std::max(max_bank_writes, count);
            min_bank_writes = std::min(min_bank_writes, count);
        }
        if (min_bank_writes > 0 && max_bank_writes / min_bank_writes > 2) {
            std::cerr << "[NVMModel] WARNING: Bank wear imbalance detected "
                      << "(max/min ratio: " << (max_bank_writes / min_bank_writes)
                      << "x). Consider bank-level wear-leveling." << std::endl;
        }
    }
}

void NVMModel::initializeNVSim() {
#ifdef HAVE_NVSIM
    try {
        // Determine NVSim type from cell_type string
        NVSimWrapper::NVMType nvsim_type = NVSimWrapper::NVMType::STTRAM;
        std::string cell_type_lower = nvm_config_.cell_type;
        std::transform(cell_type_lower.begin(), cell_type_lower.end(),
                       cell_type_lower.begin(), ::tolower);

        if (cell_type_lower.find("pcm") != std::string::npos) {
            nvsim_type = NVSimWrapper::NVMType::PCRAM;
        } else if (cell_type_lower.find("reram") != std::string::npos ||
                   cell_type_lower.find("rram") != std::string::npos) {
            nvsim_type = NVSimWrapper::NVMType::RERAM;
        }

        // Create NVSim configuration
        NVSimWrapper::NVMConfig nvsim_config;
        nvsim_config.capacity_bytes = nvm_config_.capacity;
        nvsim_config.word_width_bits = 64;
        nvsim_config.nvm_type = nvsim_type;
        nvsim_config.process_node_nm = nvm_config_.tech_node_nm;
        nvsim_config.temperature_k = 350;  // 77 degC typical operating temp
        nvsim_config.optimize_read_energy = true;
        nvsim_config.optimize_write_energy = true;
        nvsim_config.optimize_leakage = true;
        nvsim_config.is_cache = false;

        // Create and initialize NVSim wrapper
        nvsim_wrapper_ = std::make_unique<NVSimWrapper>(nvsim_config);
        nvsim_wrapper_->initialize();

        // Extract NVSim results if valid
        if (nvsim_wrapper_->isValid()) {
            read_energy_ = nvsim_wrapper_->getReadDynamicEnergy();
            write_energy_ = nvsim_wrapper_->getWriteDynamicEnergy();
            leakage_power_ = nvsim_wrapper_->getLeakagePower() / 1000.0;  // mW to W
            area_mm2_ = nvsim_wrapper_->getArea();

            // Update latencies based on NVSim
            double freq_hz = 1e9;  // Assume 1 GHz
            nvm_config_.read_latency = static_cast<Cycle>(
                nvsim_wrapper_->getReadLatency() * freq_hz);
            nvm_config_.write_latency = static_cast<Cycle>(
                nvsim_wrapper_->getWriteLatency() * freq_hz);

            std::cout << "[NVMModel] Using NVSim-generated parameters" << std::endl;
            std::cout << "[NVMModel]   Read Energy: " << read_energy_ << " nJ" << std::endl;
            std::cout << "[NVMModel]   Write Energy: " << write_energy_ << " nJ" << std::endl;
            std::cout << "[NVMModel]   Leakage Power: " << leakage_power_ << " W" << std::endl;
            std::cout << "[NVMModel]   Area: " << area_mm2_ << " mm^2" << std::endl;
        } else {
            std::cerr << "[NVMModel] NVSim failed: "
                      << nvsim_wrapper_->getErrorMessage() << std::endl;
            std::cerr << "[NVMModel] Using default cell-type-based values" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[NVMModel] NVSim exception: " << e.what() << std::endl;
        std::cerr << "[NVMModel] Using default cell-type-based values" << std::endl;
    }
#else
    std::cout << "[NVMModel] NVSim not available, using cell-type-based defaults" << std::endl;
#endif
}

//=============================================================================
// Inner-bank Timing Query Methods (NEW!)
//=============================================================================

double NVMModel::getSubarrayReadLatency() const {
    if (sttmram_arch_) {
        return sttmram_arch_->timing.subarray_read_ns;
    } else if (pcm_arch_) {
        return pcm_arch_->timing.subarray_read_ns;
    } else if (reram_arch_) {
        return reram_arch_->timing.subarray_read_ns;
    }
    return 0.0;
}

double NVMModel::getBankReadLatency() const {
    if (sttmram_arch_) {
        return sttmram_arch_->timing.bank_read_ns;
    } else if (pcm_arch_) {
        return pcm_arch_->timing.bank_read_ns;
    } else if (reram_arch_) {
        return reram_arch_->timing.bank_read_ns;
    }
    return 0.0;
}

double NVMModel::getChipReadLatency() const {
    if (sttmram_arch_) {
        return sttmram_arch_->timing.chip_read_ns;
    } else if (pcm_arch_) {
        return pcm_arch_->timing.chip_read_ns;
    } else if (reram_arch_) {
        return reram_arch_->timing.chip_read_ns;
    }
    return 0.0;
}

double NVMModel::getSubarrayWriteLatency() const {
    if (sttmram_arch_) {
        return sttmram_arch_->timing.subarray_write_ns;
    } else if (pcm_arch_) {
        // PCM has separate SET/RESET; use SET (slower) as primary write latency
        return pcm_arch_->timing.subarray_set_ns;
    } else if (reram_arch_) {
        return reram_arch_->timing.subarray_write_ns;
    }
    return 0.0;
}

double NVMModel::getBankWriteLatency() const {
    if (sttmram_arch_) {
        return sttmram_arch_->timing.bank_write_ns;
    } else if (pcm_arch_) {
        return pcm_arch_->timing.bank_set_ns;
    } else if (reram_arch_) {
        return reram_arch_->timing.bank_write_ns;
    }
    return 0.0;
}

double NVMModel::getChipWriteLatency() const {
    if (sttmram_arch_) {
        return sttmram_arch_->timing.chip_write_ns;
    } else if (pcm_arch_) {
        return pcm_arch_->timing.chip_set_ns;
    } else if (reram_arch_) {
        return reram_arch_->timing.chip_write_ns;
    }
    return 0.0;
}

double NVMModel::getInnerBankReadLatency() const {
    // Inner-bank = subarray + local routing overhead
    return getSubarrayReadLatency();
}

double NVMModel::getInnerBankWriteLatency() const {
    // Inner-bank = subarray + local routing overhead
    return getSubarrayWriteLatency();
}

bool NVMModel::supportsBankPIM() const {
    // Check if architecture is suitable for PIM based on characteristics
    if (sttmram_arch_) {
        // STT-MRAM suitable for read-heavy PIM if write ratio is acceptable
        return sttmram_arch_->isSuitableForPIM();
    } else if (pcm_arch_) {
        // PCM limited for PIM due to slow writes
        return pcm_arch_->isSuitableForPIM();
    } else if (reram_arch_) {
        // ReRAM excellent for PIM, especially analog
        return reram_arch_->isSuitableForPIM();
    }
    // Default: PIM enabled if configured
    return nvm_config_.is_pim_enabled;
}

bool NVMModel::supportsSubarrayPIM() const {
    // Check if architecture supports subarray-level PIM
    if (sttmram_arch_) {
        // STT-MRAM: good for read-heavy subarray PIM
        return sttmram_arch_->isSuitableForPIM();
    } else if (pcm_arch_) {
        // PCM: only for read-heavy subarray operations
        return false;  // PCM writes too slow for subarray PIM
    } else if (reram_arch_) {
        // ReRAM: excellent for subarray PIM, especially with analog compute
        return reram_arch_->hasAnalogCompute() || reram_arch_->isSuitableForPIM();
    }
    return false;
}

} // namespace pimid

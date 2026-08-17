#include "memory/sttmram_model.h"
#include "memory/nvsim_wrapper.h"
#include "memory/sttmram_architecture.h"
#include "memory/architecture_extractor.h"
#include "config/config_parser.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <algorithm>

namespace pimid {

//=============================================================================
// STTMRAMModel Implementation
//=============================================================================

STTMRAMModel::STTMRAMModel(const std::string& config_path)
    : MemoryModel(MemoryTechnology::STT_MRAM, config_path)
    , nvsim_wrapper_(nullptr)
    , mram_arch_(nullptr)
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

    // Initialize default STT-MRAM configuration
    mram_config_.capacity = 256ULL * 1024 * 1024;  // 256MB
    mram_config_.banks = 8;
    mram_config_.read_write_ports = 1;
    mram_config_.tech_node_nm = 22;
    mram_config_.read_latency = 5;    // 5 cycles (fast read)
    mram_config_.write_latency = 20;  // 20 cycles (slow MTJ switching)
    mram_config_.endurance = 1e15;    // 10^15 writes (very high)
    mram_config_.is_pim_enabled = true;
}

void STTMRAMModel::initialize() {
    std::cout << "[STTMRAMModel] Initializing STT-MRAM model..." << std::endl;
    loadConfig(config_path_);

    // Calculate derived parameters
    capacity_ = mram_config_.capacity;
    bandwidth_ = capacity_ / 100; // Simplified bandwidth model
    endurance_ = mram_config_.endurance;

    // Initialize NVSim FIRST (before architecture creation)
    // This allows us to extract architecture from NVSim results
    initializeNVSim();

    // Initialize STT-MRAM architecture with inner-bank timing
    // PRIORITY: Extract from NVSim if available, otherwise use factory defaults
#ifdef HAVE_NVSIM
    if (nvsim_wrapper_ && nvsim_wrapper_->isValid()) {
        // Extract architecture directly from NVSim results (PREFERRED!)
        mram_arch_ = memory::extractSTTMRAMArchitecture(*nvsim_wrapper_,
            "STTMRAM-NVSim-Extracted", 2.0);

        if (mram_arch_) {
            std::cout << "[STTMRAMModel] Architecture EXTRACTED from NVSim" << std::endl;
            /* 1.11.23: surface the tier ladder with its provenance. Nothing in
             * the log used to say where a latency came from, so a ladder built
             * from invented multipliers was indistinguishable from a tool-read
             * one -- which is how reset = write * 0.3 survived. The residual
             * between subarray and bank IS the intra-bank H-tree (NVSim runs
             * with routingMode = h_tree). */
            {
                const auto& t = mram_arch_->timing;
                std::cout << "  [tier] subarray " << t.subarray_read_ns
                          << " ns (NVSim components) | bank " << t.bank_read_ns
                          << " ns (NVSim bank->readLatency) | H-tree residual "
                          << (t.bank_read_ns - t.subarray_read_ns) << " ns"
                          << std::endl;
            }
        } else {
            std::cerr << "[STTMRAMModel] NVSim extraction failed, using factory defaults" << std::endl;
        }
    }
#endif

    /* 1.11.24: the hand-written factory defaults are GONE. They were a
     * fallback for "tool extraction failed", and that fallback is exactly
     * what let fabricated numbers reach a result while looking tool-sourced:
     * 678 literal assignments across 19 create*() factories, none of them
     * derivable from anything. A technology whose tool binding fails must
     * REFUSE, not quietly report invented specs.
     *
     * This is the vendorArrayFraction() discipline applied to a whole
     * model: absence is reported, never filled. */
    if (!mram_arch_) {
        throw std::runtime_error(
            "[STTMRAMModel] NVSim characterization failed and there is no fallback. "
            "The hand-written default specs were removed in 1.11.24 because "
            "they were unsourced and indistinguishable from tool output. "
            "Fix the NVSim configuration rather than pricing this run from "
            "invented numbers.");
    }

    std::cout << "[STTMRAMModel] Inner-bank read latency: "
              << mram_arch_->timing.inner_bank.getTotalReadLatency() << " ns" << std::endl;
    std::cout << "[STTMRAMModel] Inner-bank write latency: "
              << mram_arch_->timing.inner_bank.getTotalWriteLatency() << " ns" << std::endl;
    std::cout << "[STTMRAMModel] Architecture source: "
              << mram_arch_->timing.inner_bank.source << std::endl;

    // Use architecture energy values as defaults (may have been extracted from NVSim)
    read_energy_ = mram_arch_->energy.read_energy_per_byte;
    write_energy_ = mram_arch_->energy.write_energy_per_byte;
    leakage_power_ = mram_arch_->energy.chip_leakage_mw / 1000.0;  // mW to W

    std::cout << "[STTMRAMModel] Configuration:" << std::endl;
    std::cout << "  Capacity: " << (mram_config_.capacity / (1024.0 * 1024)) << " MB" << std::endl;
    std::cout << "  Banks: " << mram_config_.banks << std::endl;
    std::cout << "  Technology: " << mram_config_.tech_node_nm << " nm" << std::endl;
    std::cout << "  Read Latency: " << mram_config_.read_latency << " cycles" << std::endl;
    std::cout << "  Write Latency: " << mram_config_.write_latency << " cycles" << std::endl;
    std::cout << "  Endurance: " << mram_config_.endurance << " writes" << std::endl;
    std::cout << "  PIM Enabled: " << (mram_config_.is_pim_enabled ? "Yes" : "No") << std::endl;
    std::cout << "  Read Energy: " << read_energy_ << " pJ/byte" << std::endl;
    std::cout << "  Write Energy: " << write_energy_ << " pJ/byte" << std::endl;
    std::cout << "  Leakage Power: " << leakage_power_ << " W" << std::endl;
    std::cout << "[STTMRAMModel] Initialization complete" << std::endl;
}

void STTMRAMModel::loadConfig(const std::string& config_path) {
    std::cout << "[STTMRAMModel] Loading configuration from: " << config_path << std::endl;

    config::ConfigParser parser;
    std::map<std::string, std::string> config;

    if (!parser.parseFile(config_path, config)) {
        std::cerr << "[STTMRAMModel] Warning: Failed to parse config, using defaults" << std::endl;
        return;
    }

    // Extract STT-MRAM capacity
    if (config.find("stt_mram.capacity_mb") != config.end()) {
        try {
            uint64_t capacity_mb = std::stoull(config["stt_mram.capacity_mb"]);
            mram_config_.capacity = capacity_mb * 1024 * 1024;
        } catch (...) {}
    }

    // Extract organization
    if (config.find("stt_mram.organization.num_arrays") != config.end()) {
        try {
            mram_config_.banks = std::stoi(config["stt_mram.organization.num_arrays"]);
        } catch (...) {}
    }

    // Extract timing
    if (config.find("stt_mram.timing.read_latency_ns") != config.end()) {
        try {
            double read_ns = std::stod(config["stt_mram.timing.read_latency_ns"]);
            mram_config_.read_latency = static_cast<Cycle>(read_ns);
        } catch (...) {}
    }

    if (config.find("stt_mram.timing.write_latency_ns") != config.end()) {
        try {
            double write_ns = std::stod(config["stt_mram.timing.write_latency_ns"]);
            mram_config_.write_latency = static_cast<Cycle>(write_ns);
        } catch (...) {}
    }

    // Extract reliability
    if (config.find("stt_mram.reliability.write_endurance") != config.end()) {
        try {
            mram_config_.endurance = std::stoull(config["stt_mram.reliability.write_endurance"]);
        } catch (...) {}
    }

    // Extract technology
    if (config.find("stt_mram.power.tech_node_nm") != config.end()) {
        try {
            mram_config_.tech_node_nm = std::stoi(config["stt_mram.power.tech_node_nm"]);
        } catch (...) {}
    }

    std::cout << "[STTMRAMModel] Configuration loaded successfully" << std::endl;
}

Cycle STTMRAMModel::access(const MemoryRequest& req) {
    Cycle latency = mram_config_.read_latency;  // safe default

    // STT-MRAM has asymmetric read/write latency
    switch (req.type) {
        case MemoryRequestType::READ:
            latency = mram_config_.read_latency;
            total_reads_++;
            break;
        case MemoryRequestType::WRITE:
            latency = mram_config_.write_latency;  // MTJ switching dominates!
            total_writes_++;
            write_cycles_++;
            updateEndurance(req.addr);
            if (write_cycles_ > endurance_) {
                std::cerr << "[STTMRAMModel] WARNING: Endurance limit exceeded!" << std::endl;
            }
            break;
        case MemoryRequestType::ATOMIC:
            latency = mram_config_.read_latency + mram_config_.write_latency;
            total_reads_++;
            total_writes_++;
            write_cycles_++;
            break;
    }

    // Add queuing delay
    if (!pending_requests_.empty()) {
        latency += pending_requests_.size();
    }

    pending_requests_.push(req);

    return latency;
}

bool STTMRAMModel::canAccept(const MemoryRequest& req) {
    const size_t MAX_PENDING_REQUESTS = 32;
    if (pending_requests_.size() >= MAX_PENDING_REQUESTS) {
        return false;
    }

    // Check endurance for writes
    if ((req.type == MemoryRequestType::WRITE ||
         req.type == MemoryRequestType::ATOMIC) &&
        write_cycles_ >= endurance_) {
        return false;
    }

    return true;
}

void STTMRAMModel::tick() {
    current_cycle_++;

    // Process pending requests
    if (!pending_requests_.empty()) {
        pending_requests_.pop();
    }
}

Cycle STTMRAMModel::getLatency(MemoryRequestType type) const {
    switch (type) {
        case MemoryRequestType::READ:
            return mram_config_.read_latency;
        case MemoryRequestType::WRITE:
            return mram_config_.write_latency;
        case MemoryRequestType::ATOMIC:
            return mram_config_.read_latency + mram_config_.write_latency;
        default:
            return mram_config_.read_latency;
    }
}

double STTMRAMModel::getTotalEnergy() const {
    double dynamic_energy = (total_reads_ * read_energy_) +
                           (total_writes_ * write_energy_);
    double leakage_energy = leakage_power_ * (current_cycle_ / 1e9);
    return dynamic_energy + leakage_energy;
}

void STTMRAMModel::printStats() const {
    std::cout << "\n=== STT-MRAM Model Statistics ===" << std::endl;
    std::cout << "Total Cycles: " << current_cycle_ << std::endl;
    std::cout << "Total Reads: " << total_reads_ << std::endl;
    std::cout << "Total Writes: " << total_writes_ << std::endl;
    std::cout << "Write Cycles (Endurance): " << write_cycles_ << " / " << endurance_ << std::endl;

    if (total_reads_ + total_writes_ > 0) {
        double read_ratio = static_cast<double>(total_reads_) / (total_reads_ + total_writes_);
        double write_ratio = static_cast<double>(total_writes_) / (total_reads_ + total_writes_);
        std::cout << "Read Ratio: " << (read_ratio * 100.0) << "%" << std::endl;
        std::cout << "Write Ratio: " << (write_ratio * 100.0) << "%" << std::endl;

        double endurance_used = (static_cast<double>(write_cycles_) / endurance_) * 100.0;
        std::cout << "Endurance Used: " << endurance_used << "%" << std::endl;
    }

    std::cout << "\nLatency (Inner-Bank Timing):" << std::endl;
    std::cout << "  Subarray Read: " << getSubarrayReadLatency() << " ns" << std::endl;
    std::cout << "  Bank Read: " << getBankReadLatency() << " ns" << std::endl;
    std::cout << "  Chip Read: " << getChipReadLatency() << " ns" << std::endl;
    std::cout << "  Subarray Write: " << getSubarrayWriteLatency() << " ns" << std::endl;
    std::cout << "  Bank Write: " << getBankWriteLatency() << " ns" << std::endl;
    std::cout << "  Chip Write: " << getChipWriteLatency() << " ns" << std::endl;

    std::cout << "\nEnergy Consumption:" << std::endl;
    std::cout << "  Read Energy (per byte): " << read_energy_ << " pJ" << std::endl;
    std::cout << "  Write Energy (per byte): " << write_energy_ << " pJ" << std::endl;
    std::cout << "  Total Read Energy: " << (total_reads_ * read_energy_) << " pJ" << std::endl;
    std::cout << "  Total Write Energy: " << (total_writes_ * write_energy_) << " pJ" << std::endl;
    std::cout << "  Leakage Power: " << leakage_power_ << " W" << std::endl;
    std::cout << "  Total Energy: " << getTotalEnergy() << " pJ" << std::endl;
    std::cout << "================================\n" << std::endl;
}

void STTMRAMModel::resetStats() {
    total_reads_ = 0;
    total_writes_ = 0;
    write_cycles_ = 0;
    current_cycle_ = 0;
}

void STTMRAMModel::updateEndurance(Address addr) {
    // Per-cell endurance tracking
    // Calculate bank and page from address
    uint32_t bank = static_cast<uint32_t>((addr >> 12) % mram_config_.banks);
    uint64_t page = (addr >> 12) / mram_config_.banks;

    // Track writes per bank
    bank_write_counts_[bank]++;

    // Track writes per page (sampled - every 1000th page tracked)
    if (page % 1000 == 0) {
        page_write_counts_[page]++;

        // Check for hot pages (potential wear-out)
        if (page_write_counts_[page] > endurance_ / 1000) {
            std::cerr << "[STTMRAMModel] WARNING: Hot page detected at page "
                      << page << " with " << page_write_counts_[page]
                      << " writes (sampling 1:1000)" << std::endl;
        }
    }
}

void STTMRAMModel::initializeNVSim() {
#ifdef HAVE_NVSIM
    try {
        // Create NVSim configuration for STT-MRAM
        NVSimWrapper::NVMConfig nvsim_config;
        nvsim_config.capacity_bytes = mram_config_.capacity;
        /* 1.11.25: the run's access width when supplied, else the model

         * default. Mismatching this selects a different pregenerated

         * cache entry and characterizes a different access. */

        nvsim_config.word_width_bits = (access_width_bits_ > 0) ? access_width_bits_ : 64;
        nvsim_config.nvm_type = NVSimWrapper::NVMType::STTRAM;
        nvsim_config.process_node_nm = mram_config_.tech_node_nm;
        nvsim_config.temperature_k = 350;  // 77°C typical operating temp
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
            mram_config_.read_latency = static_cast<Cycle>(
                nvsim_wrapper_->getReadLatency() * freq_hz);
            mram_config_.write_latency = static_cast<Cycle>(
                nvsim_wrapper_->getWriteLatency() * freq_hz);

            std::cout << "[STTMRAMModel] Using NVSim-generated parameters" << std::endl;
        } else {
            std::cerr << "[STTMRAMModel] NVSim failed: "
                      << nvsim_wrapper_->getErrorMessage() << std::endl;
            std::cerr << "[STTMRAMModel] Using architecture-based defaults" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[STTMRAMModel] NVSim exception: " << e.what() << std::endl;
        std::cerr << "[STTMRAMModel] Using architecture-based defaults" << std::endl;
    }
#else
    std::cout << "[STTMRAMModel] NVSim not available, using architecture-based values" << std::endl;
#endif
}

//=============================================================================
// Inner-Bank Timing Queries (NEW!)
//=============================================================================

double STTMRAMModel::getSubarrayReadLatency() const {
    if (!mram_arch_) return 0.0;
    return mram_arch_->timing.subarray_read_ns;
}

double STTMRAMModel::getBankReadLatency() const {
    if (!mram_arch_) return 0.0;
    return mram_arch_->timing.bank_read_ns;
}

double STTMRAMModel::getChipReadLatency() const {
    if (!mram_arch_) return 0.0;
    return mram_arch_->timing.chip_read_ns;
}

double STTMRAMModel::getSubarrayWriteLatency() const {
    if (!mram_arch_) return 0.0;
    return mram_arch_->timing.subarray_write_ns;
}

double STTMRAMModel::getBankWriteLatency() const {
    if (!mram_arch_) return 0.0;
    return mram_arch_->timing.bank_write_ns;
}

double STTMRAMModel::getChipWriteLatency() const {
    if (!mram_arch_) return 0.0;
    return mram_arch_->timing.chip_write_ns;
}

double STTMRAMModel::getInnerBankReadLatency() const {
    if (!mram_arch_) return 0.0;
    return mram_arch_->timing.inner_bank.getTotalReadLatency();
}

double STTMRAMModel::getInnerBankWriteLatency() const {
    if (!mram_arch_) return 0.0;
    return mram_arch_->timing.inner_bank.getTotalWriteLatency();
}

bool STTMRAMModel::supportsBankPIM() const {
    if (!mram_arch_) return false;
    return mram_arch_->isSuitableForPIM();
}

bool STTMRAMModel::supportsSubarrayPIM() const {
    // STT-MRAM supports subarray-level PIM (fast reads, persistent state)
    return true;
}


/* 1.11.24: STTMRAMModel under the plugin contract. NVM is not DRAM-like: subarray,
 * bank and chip only. The subarray/bank separation is the intra-bank H-tree
 * NVSim builds (routingMode = h_tree), not a multiplier -- see 1.11.23. */
double STTMRAMModel::getTierLatencyNs(Tier tier, Op op) const {
    if (op == Op::READ) {
        switch (tier) {
            case Tier::SUBARRAY: return getSubarrayReadLatency();
            case Tier::BANK:     return getBankReadLatency();
            case Tier::CHIP:     return getChipReadLatency();
            default:             return -1.0;
        }
    }
    if (op == Op::WRITE) {
        switch (tier) {
            case Tier::SUBARRAY: return getSubarrayWriteLatency();
            case Tier::BANK:     return getBankWriteLatency();
            case Tier::CHIP:     return getChipWriteLatency();
            default:             return -1.0;
        }
    }
    return -1.0;   // SET/RESET are a PCM distinction
}
bool STTMRAMModel::hasTier(Tier tier) const {
    return tier == Tier::SUBARRAY || tier == Tier::BANK || tier == Tier::CHIP;
}
std::string STTMRAMModel::tierLatencySource(Tier tier, Op op) const {
    if (getTierLatencyNs(tier, op) < 0.0) return "";
    switch (tier) {
        case Tier::SUBARRAY: return "NVSim component delays";
        case Tier::BANK:     return "NVSim bank->readLatency (incl. H-tree)";
        case Tier::CHIP:     return "NVSim bank + configured net hop";
        default:             return "";
    }
}


/* 1.11.25: characterize the array the RUN configures, not the model's
 * compiled-in default. Must be called before initialize(). */
void STTMRAMModel::setArrayCapacityBytes(uint64_t bytes) {
    if (bytes > 0) mram_config_.capacity = bytes;
}


/* 1.11.25: characterize the access the RUN performs. Must precede
 * initialize(). 0 keeps the model default. */
/* 1.11.51 (L70): the run's node, replacing the compiled-in default.
 * Must precede initialize(). */
void STTMRAMModel::setTechNodeNm(int nm) {
    if (nm > 0) mram_config_.tech_node_nm = nm;
}

void STTMRAMModel::setAccessWidthBits(uint32_t bits) {
    if (bits >= 8 && bits <= 1024) access_width_bits_ = bits;
}

} // namespace pimid

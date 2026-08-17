#include "memory/reram_model.h"
#include "memory/nvsim_wrapper.h"
#include "memory/reram_architecture.h"
#include "memory/architecture_extractor.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <map>
#include <unordered_map>

namespace pimid {

//=============================================================================
// ReRAMModel Implementation
//=============================================================================

ReRAMModel::ReRAMModel(const std::string& config_path)
    : MemoryModel(MemoryTechnology::ReRAM, config_path)
    , nvsim_wrapper_(nullptr)
    , reram_arch_(nullptr)
    , total_reads_(0)
    , total_writes_(0)
    , total_analog_ops_(0)
    , write_cycles_(0)
    , read_energy_(0.0)
    , write_energy_(0.0)
    , analog_compute_energy_(0.0)
    , leakage_power_(0.0)
    , area_mm2_(0.0)
    , current_cycle_(0)
    , capacity_(0)
    , bandwidth_(0)
    , endurance_(0) {

    // Initialize default ReRAM configuration
    reram_config_.capacity = 256ULL * 1024 * 1024;  // 256MB
    reram_config_.banks = 16;
    reram_config_.read_write_ports = 1;
    reram_config_.tech_node_nm = 32;
    reram_config_.read_latency = 7;      // 7 cycles (moderate)
    reram_config_.write_latency = 15;    // 15 cycles (fast!)
    reram_config_.analog_compute_latency = 3;  // 3 cycles (VERY fast analog!)
    reram_config_.endurance = 1e11;      // 10^11 writes (good)
    reram_config_.analog_capable = true;
    reram_config_.is_pim_enabled = true;
}

void ReRAMModel::initialize() {
    std::cout << "[ReRAMModel] Initializing ReRAM model..." << std::endl;
    loadConfig(config_path_);

    // Calculate derived parameters
    capacity_ = reram_config_.capacity;
    bandwidth_ = capacity_ / 50;  // Good bandwidth
    endurance_ = reram_config_.endurance;

    // Initialize NVSim FIRST (before architecture creation)
    initializeNVSim();

    // Initialize ReRAM architecture with inner-bank timing
    // PRIORITY: Extract from NVSim if available, otherwise use factory defaults
#ifdef HAVE_NVSIM
    if (nvsim_wrapper_ && nvsim_wrapper_->isValid()) {
        // Extract architecture directly from NVSim results (PREFERRED!)
        reram_arch_ = memory::extractReRAMArchitecture(*nvsim_wrapper_,
            "ReRAM-NVSim-Extracted", 1.0, reram_config_.analog_capable);

        if (reram_arch_) {
            std::cout << "[ReRAMModel] Architecture EXTRACTED from NVSim" << std::endl;
            /* 1.11.23: surface the tier ladder with its provenance. Nothing in
             * the log used to say where a latency came from, so a ladder built
             * from invented multipliers was indistinguishable from a tool-read
             * one -- which is how reset = write * 0.3 survived. The residual
             * between subarray and bank IS the intra-bank H-tree (NVSim runs
             * with routingMode = h_tree). */
            {
                const auto& t = reram_arch_->timing;
                std::cout << "  [tier] subarray " << t.subarray_read_ns
                          << " ns (NVSim components) | bank " << t.bank_read_ns
                          << " ns (NVSim bank->readLatency) | H-tree residual "
                          << (t.bank_read_ns - t.subarray_read_ns) << " ns"
                          << std::endl;
            }
            if (reram_config_.analog_capable) {
                std::cout << "[ReRAMModel] ANALOG COMPUTE ENABLED!" << std::endl;
            }
        } else {
            std::cerr << "[ReRAMModel] NVSim extraction failed, using factory defaults" << std::endl;
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
    if (!reram_arch_) {
        throw std::runtime_error(
            "[ReRAMModel] NVSim characterization failed and there is no fallback. "
            "The hand-written default specs were removed in 1.11.24 because "
            "they were unsourced and indistinguishable from tool output. "
            "Fix the NVSim configuration rather than pricing this run from "
            "invented numbers.");
    }

    std::cout << "[ReRAMModel] Inner-bank read latency: "
              << reram_arch_->timing.inner_bank.getTotalReadLatency() << " ns" << std::endl;
    std::cout << "[ReRAMModel] Inner-bank write latency: "
              << reram_arch_->timing.inner_bank.getTotalWriteLatency() << " ns" << std::endl;
    std::cout << "[ReRAMModel] Architecture source: "
              << reram_arch_->timing.inner_bank.source << std::endl;

    if (reram_arch_->hasAnalogCompute()) {
        std::cout << "[ReRAMModel] Analog compute latency: "
                  << reram_arch_->timing.inner_bank.getAnalogComputeLatency() << " ns (FAST!)" << std::endl;
    }

    // Use architecture energy values (may have been extracted from NVSim)
    read_energy_ = reram_arch_->energy.read_energy_per_byte;
    write_energy_ = reram_arch_->energy.write_energy_per_byte;
    analog_compute_energy_ = reram_arch_->energy.analog_compute_energy_pJ;
    leakage_power_ = reram_arch_->energy.chip_leakage_mw / 1000.0;

    std::cout << "[ReRAMModel] Configuration:" << std::endl;
    std::cout << "  Capacity: " << (reram_config_.capacity / (1024.0 * 1024)) << " MB" << std::endl;
    std::cout << "  Banks: " << reram_config_.banks << std::endl;
    std::cout << "  Technology: " << reram_config_.tech_node_nm << " nm" << std::endl;
    std::cout << "  Read Latency: " << reram_config_.read_latency << " cycles" << std::endl;
    std::cout << "  Write Latency: " << reram_config_.write_latency << " cycles (fast!)" << std::endl;
    std::cout << "  Analog Compute Latency: " << reram_config_.analog_compute_latency
              << " cycles (VERY fast!)" << std::endl;
    std::cout << "  Endurance: " << reram_config_.endurance << " writes" << std::endl;
    std::cout << "  Analog Capable: " << (reram_config_.analog_capable ? "Yes" : "No") << std::endl;
    std::cout << "  Read Energy: " << read_energy_ << " pJ/byte" << std::endl;
    std::cout << "  Write Energy: " << write_energy_ << " pJ/byte" << std::endl;
    std::cout << "  Analog Compute Energy: " << analog_compute_energy_ << " pJ (very low!)" << std::endl;
    std::cout << "  Leakage Power: " << leakage_power_ << " W" << std::endl;
    std::cout << "[ReRAMModel] Initialization complete" << std::endl;
}

void ReRAMModel::loadConfig(const std::string& config_path) {
    std::cout << "[ReRAMModel] Loading configuration from: " << config_path << std::endl;
    std::cout << "[ReRAMModel] Using default 256MB ReRAM configuration" << std::endl;
}

Cycle ReRAMModel::access(const MemoryRequest& req) {
    Cycle latency = reram_config_.read_latency;  // safe default

    // ReRAM has moderate read/write latency (better than PCM!)
    switch (req.type) {
        case MemoryRequestType::READ:
            latency = reram_config_.read_latency;
            total_reads_++;
            break;
        case MemoryRequestType::WRITE:
            latency = reram_config_.write_latency;  // Fast writes!
            total_writes_++;
            write_cycles_++;
            updateEndurance(req.addr);
            if (write_cycles_ > endurance_) {
                std::cerr << "[ReRAMModel] WARNING: Endurance limit exceeded!" << std::endl;
            }
            break;
        case MemoryRequestType::ATOMIC: {
            // Detect analog compute via request flags or payload:
            //   - req.flags bit 0x80 = analog compute
            //   - req.size == 0      = no data movement (pure compute)
            bool is_analog_compute = reram_config_.analog_capable &&
                (req.size == 0 || (req.flags & 0x80) != 0);
            if (is_analog_compute) {
                // Analog compute is VERY fast — in-situ matrix-vector multiplication
                latency = reram_config_.analog_compute_latency;
                total_analog_ops_++;
            } else {
                // Standard atomic: read-modify-write
                latency = reram_config_.read_latency + reram_config_.write_latency;
                total_reads_++;
                total_writes_++;
                write_cycles_++;
                updateEndurance(req.addr);
            }
            break;
        }
    }

    // Add queuing delay
    if (!pending_requests_.empty()) {
        latency += pending_requests_.size();
    }

    pending_requests_.push(req);

    return latency;
}

bool ReRAMModel::canAccept(const MemoryRequest& req) {
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

void ReRAMModel::tick() {
    current_cycle_++;

    if (!pending_requests_.empty()) {
        pending_requests_.pop();
    }
}

Cycle ReRAMModel::getLatency(MemoryRequestType type) const {
    switch (type) {
        case MemoryRequestType::READ:
            return reram_config_.read_latency;
        case MemoryRequestType::WRITE:
            return reram_config_.write_latency;
        case MemoryRequestType::ATOMIC:
            if (reram_config_.analog_capable) {
                return reram_config_.analog_compute_latency;
            } else {
                return reram_config_.read_latency + reram_config_.write_latency;
            }
        default:
            return reram_config_.read_latency;
    }
}

double ReRAMModel::getTotalEnergy() const {
    double dynamic_energy = (total_reads_ * read_energy_) +
                           (total_writes_ * write_energy_) +
                           (total_analog_ops_ * analog_compute_energy_);
    double leakage_energy = leakage_power_ * (current_cycle_ / 1e9);
    return dynamic_energy + leakage_energy;
}

void ReRAMModel::printStats() const {
    std::cout << "\n=== ReRAM Model Statistics ===" << std::endl;
    std::cout << "Total Cycles: " << current_cycle_ << std::endl;
    std::cout << "Total Reads: " << total_reads_ << std::endl;
    std::cout << "Total Writes: " << total_writes_ << std::endl;
    std::cout << "Total Analog Compute Ops: " << total_analog_ops_ << std::endl;
    std::cout << "Write Cycles (Endurance): " << write_cycles_ << " / " << endurance_ << std::endl;

    uint64_t total_ops = total_reads_ + total_writes_ + total_analog_ops_;
    if (total_ops > 0) {
        double read_ratio = static_cast<double>(total_reads_) / total_ops;
        double write_ratio = static_cast<double>(total_writes_) / total_ops;
        double analog_ratio = static_cast<double>(total_analog_ops_) / total_ops;
        std::cout << "Read Ratio: " << (read_ratio * 100.0) << "%" << std::endl;
        std::cout << "Write Ratio: " << (write_ratio * 100.0) << "%" << std::endl;
        std::cout << "Analog Compute Ratio: " << (analog_ratio * 100.0) << "%" << std::endl;

        double endurance_used = (static_cast<double>(write_cycles_) / endurance_) * 100.0;
        std::cout << "Endurance Used: " << endurance_used << "%" << std::endl;
    }

    std::cout << "\nLatency (Inner-Bank Timing):" << std::endl;
    std::cout << "  Subarray Read: " << getSubarrayReadLatency() << " ns" << std::endl;
    std::cout << "  Bank Read: " << getBankReadLatency() << " ns" << std::endl;
    std::cout << "  Chip Read: " << getChipReadLatency() << " ns" << std::endl;
    std::cout << "  Subarray Write: " << getSubarrayWriteLatency() << " ns (fast!)" << std::endl;
    std::cout << "  Bank Write: " << getBankWriteLatency() << " ns" << std::endl;
    std::cout << "  Chip Write: " << getChipWriteLatency() << " ns" << std::endl;

    if (reram_config_.analog_capable) {
        std::cout << "  Analog Compute: " << getAnalogComputeLatency() << " ns (VERY fast!)" << std::endl;
    }

    std::cout << "\nEnergy Consumption:" << std::endl;
    std::cout << "  Read Energy (per byte): " << read_energy_ << " pJ" << std::endl;
    std::cout << "  Write Energy (per byte): " << write_energy_ << " pJ" << std::endl;
    std::cout << "  Analog Compute Energy (per op): " << analog_compute_energy_ << " pJ (very low!)" << std::endl;
    std::cout << "  Total Read Energy: " << (total_reads_ * read_energy_) << " pJ" << std::endl;
    std::cout << "  Total Write Energy: " << (total_writes_ * write_energy_) << " pJ" << std::endl;
    std::cout << "  Total Analog Compute Energy: " << (total_analog_ops_ * analog_compute_energy_) << " pJ" << std::endl;
    std::cout << "  Leakage Power: " << leakage_power_ << " W" << std::endl;
    std::cout << "  Total Energy: " << getTotalEnergy() << " pJ" << std::endl;
    std::cout << "================================\n" << std::endl;
}

void ReRAMModel::resetStats() {
    total_reads_ = 0;
    total_writes_ = 0;
    total_analog_ops_ = 0;
    write_cycles_ = 0;
    current_cycle_ = 0;
}

void ReRAMModel::updateEndurance(Address addr) {
    // Per-cell endurance tracking for ReRAM
    // ReRAM has moderate endurance (10^10 - 10^12 writes)
    // which is better than PCM but worse than STT-MRAM

    // Calculate bank and page from address
    uint32_t bank = static_cast<uint32_t>((addr >> 12) % reram_config_.banks);
    uint64_t page = (addr >> 12) / reram_config_.banks;
    uint64_t cell = addr >> 6;  // Cell granularity (64-byte)

    // Track writes per bank
    bank_write_counts_[bank]++;

    // Track writes per page (sampled - every 100th page for memory efficiency)
    if (page % 100 == 0) {
        page_write_counts_[page]++;

        // Check for hot pages approaching wear-out
        // ReRAM endurance is ~10^11, so threshold scales accordingly
        uint64_t hot_threshold = endurance_ / 100;  // Scaled by sampling factor
        if (page_write_counts_[page] > hot_threshold) {
            std::cerr << "[ReRAMModel] WARNING: Hot page at " << page
                      << " with " << page_write_counts_[page]
                      << " writes (sampling 1:100). Consider wear-leveling." << std::endl;
        }
    }

    // Track high-write cells for wear-leveling hints (sample every 1000th cell)
    if (cell % 1000 == 0) {
        cell_write_counts_[cell]++;

        // Warn about potential hot cells
        if (cell_write_counts_[cell] > endurance_ / 10000) {
            std::cerr << "[ReRAMModel] WARNING: Hot cell at address 0x"
                      << std::hex << addr << std::dec
                      << " with " << cell_write_counts_[cell] << " writes."
                      << " (sampling 1:1000)" << std::endl;
        }
    }

    // Report bank-level wear imbalance periodically (every 100K writes)
    if (write_cycles_ % 100000 == 0 && write_cycles_ > 0) {
        reportWearImbalance();
    }
}

void ReRAMModel::reportWearImbalance() const {
    if (bank_write_counts_.empty()) return;

    uint64_t max_bank_writes = 0;
    uint64_t min_bank_writes = UINT64_MAX;
    uint32_t max_bank_id = 0;
    uint32_t min_bank_id = 0;

    for (const auto& [bank_id, count] : bank_write_counts_) {
        if (count > max_bank_writes) {
            max_bank_writes = count;
            max_bank_id = bank_id;
        }
        if (count < min_bank_writes) {
            min_bank_writes = count;
            min_bank_id = bank_id;
        }
    }

    if (min_bank_writes > 0 && max_bank_writes / min_bank_writes > 2) {
        std::cerr << "[ReRAMModel] WARNING: Bank wear imbalance detected!" << std::endl;
        std::cerr << "  Max writes: bank " << max_bank_id << " (" << max_bank_writes << " writes)" << std::endl;
        std::cerr << "  Min writes: bank " << min_bank_id << " (" << min_bank_writes << " writes)" << std::endl;
        std::cerr << "  Imbalance ratio: " << (max_bank_writes / min_bank_writes) << "x" << std::endl;
        std::cerr << "  Consider enabling bank-level wear-leveling." << std::endl;
    }
}

//=============================================================================
// Inner-Bank Timing Queries (NEW!)
//=============================================================================

double ReRAMModel::getSubarrayReadLatency() const {
    if (!reram_arch_) return 0.0;
    return reram_arch_->timing.subarray_read_ns;
}

double ReRAMModel::getBankReadLatency() const {
    if (!reram_arch_) return 0.0;
    return reram_arch_->timing.bank_read_ns;
}

double ReRAMModel::getChipReadLatency() const {
    if (!reram_arch_) return 0.0;
    return reram_arch_->timing.chip_read_ns;
}

double ReRAMModel::getSubarrayWriteLatency() const {
    if (!reram_arch_) return 0.0;
    return reram_arch_->timing.subarray_write_ns;
}

double ReRAMModel::getBankWriteLatency() const {
    if (!reram_arch_) return 0.0;
    return reram_arch_->timing.bank_write_ns;
}

double ReRAMModel::getChipWriteLatency() const {
    if (!reram_arch_) return 0.0;
    return reram_arch_->timing.chip_write_ns;
}

double ReRAMModel::getInnerBankReadLatency() const {
    if (!reram_arch_) return 0.0;
    return reram_arch_->timing.inner_bank.getTotalReadLatency();
}

double ReRAMModel::getInnerBankWriteLatency() const {
    if (!reram_arch_) return 0.0;
    return reram_arch_->timing.inner_bank.getTotalWriteLatency();
}

double ReRAMModel::getAnalogComputeLatency() const {
    if (!reram_arch_) return 0.0;
    return reram_arch_->timing.inner_bank.getAnalogComputeLatency();
}

double ReRAMModel::getAnalogComputeEnergy() const {
    return analog_compute_energy_;
}

bool ReRAMModel::supportsAnalogCompute() const {
    if (!reram_arch_) return false;
    return reram_arch_->hasAnalogCompute();
}

bool ReRAMModel::supportsBankPIM() const {
    if (!reram_arch_) return false;
    return reram_arch_->isSuitableForPIM();
}

bool ReRAMModel::supportsSubarrayPIM() const {
    // ReRAM supports subarray PIM, especially for analog compute!
    return true;
}

//=============================================================================
// NVSim Integration (NEW!)
//=============================================================================

void ReRAMModel::initializeNVSim() {
#ifdef HAVE_NVSIM
    try {
        // Create NVSim configuration for ReRAM
        NVSimWrapper::NVMConfig nvsim_config;
        nvsim_config.capacity_bytes = reram_config_.capacity;
        /* 1.11.25: the run's access width when supplied, else the model

         * default. Mismatching this selects a different pregenerated

         * cache entry and characterizes a different access. */

        nvsim_config.word_width_bits = (access_width_bits_ > 0) ? access_width_bits_ : 64;
        nvsim_config.nvm_type = NVSimWrapper::NVMType::RERAM;  // ReRAM type
        nvsim_config.process_node_nm = reram_config_.tech_node_nm;
        nvsim_config.temperature_k = temperature_k_;   // 1.11.52 (D055)  // ~77°C typical operating temp
        nvsim_config.optimize_read_energy = true;
        nvsim_config.optimize_write_energy = true;  // ReRAM has fast writes
        nvsim_config.optimize_leakage = true;
        nvsim_config.is_cache = false;

        // Create and initialize NVSim wrapper
        nvsim_wrapper_ = std::make_unique<NVSimWrapper>(nvsim_config);
        nvsim_wrapper_->initialize();

        // Extract NVSim results if valid
        if (nvsim_wrapper_->isValid()) {
            // Update config latencies from NVSim
            double read_ns = nvsim_wrapper_->getReadLatency() * 1e9;
            double write_ns = nvsim_wrapper_->getWriteLatency() * 1e9;

            if (read_ns > 0) {
                reram_config_.read_latency = static_cast<Cycle>(read_ns);
            }
            if (write_ns > 0) {
                reram_config_.write_latency = static_cast<Cycle>(write_ns);
            }

            // Update energy values
            read_energy_ = nvsim_wrapper_->getReadDynamicEnergy();
            write_energy_ = nvsim_wrapper_->getWriteDynamicEnergy();
            leakage_power_ = nvsim_wrapper_->getLeakagePower() / 1000.0;  // mW to W
            area_mm2_ = nvsim_wrapper_->getArea();

            std::cout << "[ReRAMModel] Using NVSim-generated parameters" << std::endl;
            std::cout << "[ReRAMModel] NVSim Read Latency: " << read_ns << " ns" << std::endl;
            std::cout << "[ReRAMModel] NVSim Write Latency: " << write_ns << " ns (fast!)" << std::endl;
        } else {
            std::cerr << "[ReRAMModel] NVSim failed: "
                      << nvsim_wrapper_->getErrorMessage() << std::endl;
            std::cerr << "[ReRAMModel] Using architecture-based defaults" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[ReRAMModel] NVSim exception: " << e.what() << std::endl;
        std::cerr << "[ReRAMModel] Using architecture-based defaults" << std::endl;
    }
#else
    std::cout << "[ReRAMModel] NVSim not available, using architecture-based values" << std::endl;
#endif
}


/* 1.11.24: ReRAMModel under the plugin contract. NVM is not DRAM-like: subarray,
 * bank and chip only. The subarray/bank separation is the intra-bank H-tree
 * NVSim builds (routingMode = h_tree), not a multiplier -- see 1.11.23. */
double ReRAMModel::getTierLatencyNs(Tier tier, Op op) const {
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
bool ReRAMModel::hasTier(Tier tier) const {
    return tier == Tier::SUBARRAY || tier == Tier::BANK || tier == Tier::CHIP;
}
std::string ReRAMModel::tierLatencySource(Tier tier, Op op) const {
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
void ReRAMModel::setArrayCapacityBytes(uint64_t bytes) {
    if (bytes > 0) reram_config_.capacity = bytes;
}


/* 1.11.25: characterize the access the RUN performs. Must precede
 * initialize(). 0 keeps the model default. */
/* 1.11.51 (L70): the run's node, replacing the compiled-in default.
 * Must precede initialize(). */
/* 1.11.52 (D055): the run's temperature, replacing the hardcoded 350 K in
 * the tool query below. Must precede initialize(). */
void ReRAMModel::setTemperatureK(int k) {
    if (k > 0) temperature_k_ = k;
}

void ReRAMModel::setTechNodeNm(int nm) {
    if (nm > 0) reram_config_.tech_node_nm = nm;
}

void ReRAMModel::setAccessWidthBits(uint32_t bits) {
    if (bits >= 8 && bits <= 1024) access_width_bits_ = bits;
}

} // namespace pimid

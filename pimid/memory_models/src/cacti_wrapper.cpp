#include "cacti_wrapper.h"
#include <iostream>
#include <cmath>
#include <stdexcept>

// Include CACTI headers
#include "cacti_interface.h"

namespace pimid {

//=============================================================================
// CACTIWrapper Implementation
//=============================================================================

CACTIWrapper::CACTIWrapper(const SRAMConfig& config)
    : config_(config)
    , cacti_result_(nullptr)
    , cacti_input_(nullptr)
    , initialized_(false)
    , valid_(false)
    , error_message_("")
{
}

CACTIWrapper::~CACTIWrapper() {
    if (cacti_result_) {
        delete cacti_result_;
        cacti_result_ = nullptr;
    }
    if (cacti_input_) {
        delete cacti_input_;
        cacti_input_ = nullptr;
    }
}

void CACTIWrapper::initialize() {
    if (initialized_) {
        std::cerr << "[CACTIWrapper] Warning: Already initialized, reinitializing..." << std::endl;
        if (cacti_result_) {
            delete cacti_result_;
            cacti_result_ = nullptr;
        }
        if (cacti_input_) {
            delete cacti_input_;
            cacti_input_ = nullptr;
        }
    }

    validateConfiguration();
    if (!valid_) {
        throw std::runtime_error("[CACTIWrapper] Invalid configuration: " + error_message_);
    }

    runCACTI();
    initialized_ = true;

    std::cout << "[CACTIWrapper] Initialized with:" << std::endl;
    std::cout << "  Capacity: " << (config_.capacity_bytes / 1024) << " KB" << std::endl;
    std::cout << "  Line Size: " << config_.line_size << " bytes" << std::endl;
    std::cout << "  Associativity: " << config_.associativity << "-way" << std::endl;
    std::cout << "  Banks: " << config_.banks << std::endl;
    std::cout << "  Technology: " << config_.tech_node_nm << " nm" << std::endl;
    if (valid_ && cacti_result_) {
        std::cout << "  Access Time: " << (getAccessTime() * 1e9) << " ns" << std::endl;
        std::cout << "  Cycle Time: " << (getCycleTime() * 1e9) << " ns" << std::endl;
        std::cout << "  Area: " << getArea() << " mm^2" << std::endl;
        std::cout << "  Read Energy: " << getDynamicReadEnergy() << " nJ" << std::endl;
        std::cout << "  Write Energy: " << getDynamicWriteEnergy() << " nJ" << std::endl;
        std::cout << "  Leakage Power: " << getLeakagePower() << " mW" << std::endl;
    }
}

void CACTIWrapper::reconfigure(const SRAMConfig& config) {
    config_ = config;
    initialized_ = false;
    initialize();
}

void CACTIWrapper::validateConfiguration() {
    valid_ = true;
    error_message_ = "";

    // Validate capacity
    if (config_.capacity_bytes < 64 || config_.capacity_bytes > (16ULL * 1024 * 1024 * 1024)) {
        valid_ = false;
        error_message_ = "Capacity out of range (64B - 16GB)";
        return;
    }

    // Validate line size (must be power of 2)
    if (config_.line_size < 8 || config_.line_size > 1024 ||
        (config_.line_size & (config_.line_size - 1)) != 0) {
        valid_ = false;
        error_message_ = "Line size must be power of 2 between 8 and 1024";
        return;
    }

    // Validate associativity
    if (config_.associativity < 1 || config_.associativity > 64) {
        valid_ = false;
        error_message_ = "Associativity out of range (1-64)";
        return;
    }

    // Validate banks
    if (config_.banks < 1 || config_.banks > 32) {
        valid_ = false;
        error_message_ = "Number of banks out of range (1-32)";
        return;
    }

    // Validate technology node
    if (config_.tech_node_nm < 7 || config_.tech_node_nm > 90) {
        valid_ = false;
        error_message_ = "Technology node out of range (7nm - 90nm)";
        return;
    }
}

InputParameter* CACTIWrapper::createCACTIInput(const SRAMConfig& config) {
    InputParameter* input = new InputParameter();

    // Basic cache parameters
    input->cache_sz = config.capacity_bytes;
    input->line_sz = config.line_size;
    input->assoc = config.associativity;
    input->nbanks = config.banks;

    // Port configuration
    input->num_rw_ports = config.read_write_ports;
    input->num_rd_ports = config.read_ports;
    input->num_wr_ports = config.write_ports;
    input->num_se_rd_ports = config.single_ended_read_ports;
    input->num_search_ports = 0;  // Not a CAM

    // Technology
    input->F_sz_nm = config.tech_node_nm;
    input->F_sz_um = config.tech_node_nm / 1000.0;
    input->temp = config.temperature;

    // Cache or scratchpad
    input->is_cache = config.is_cache;
    input->is_main_mem = config.is_main_memory;
    input->pure_ram = !config.is_cache;
    input->pure_cam = false;

    // Access mode
    input->access_mode = config.access_mode;

    // Output width
    input->out_w = config.output_width_bits;

    // Tag configuration
    input->specific_tag = config.specific_tag;
    input->tag_w = config.tag_width_bits;

    // Technology flavor (using ITRS-HP for high performance)
    input->ram_cell_tech_type = 0;  // ITRS-HP
    input->peri_global_tech_type = 0;  // ITRS-HP
    input->data_arr_ram_cell_tech_type = 0;
    input->data_arr_peri_global_tech_type = 0;
    input->tag_arr_ram_cell_tech_type = 0;
    input->tag_arr_peri_global_tech_type = 0;

    // Interconnect (0 = aggressive)
    input->ic_proj_type = 0;
    input->wire_is_mat_type = 2;  // Semi-global
    input->wire_os_mat_type = 2;  // Semi-global

    // Optimization objectives
    input->obj_func_dyn_energy = config.obj_func_dynamic_power;
    input->obj_func_dyn_power = config.obj_func_dynamic_power;
    input->obj_func_leak_power = config.obj_func_leakage_power;
    input->obj_func_cycle_t = config.obj_func_cycle_time;

    // Delay/power/area weights (default: balanced)
    input->delay_wt = config.obj_func_delay;
    input->dynamic_power_wt = config.obj_func_dynamic_power;
    input->leakage_power_wt = config.obj_func_leakage_power;
    input->cycle_time_wt = config.obj_func_cycle_time;
    input->area_wt = config.obj_func_area;

    // Deviation weights
    input->delay_dev = 100000;
    input->dynamic_power_dev = 100000;
    input->leakage_power_dev = 100000;
    input->cycle_time_dev = 100000;
    input->area_dev = 100000;

    // ED optimization (2 = use weight and deviate)
    input->ed = 2;

    // NUCA parameters (not used for simple cache)
    input->nuca = 0;
    input->nuca_bank_count = 0;

    // 3D parameters (not used)
    input->is_3d_mem = false;
    input->print_detail_debug = false;

    // Main memory parameters (set defaults even for cache)
    input->page_sz_bits = 8192;
    input->burst_len = 8;
    input->int_prefetch_w = 8;

    // Repeater parameters
    input->rpters_in_htree = true;
    input->ver_htree_wires_over_array = 0;
    input->broadcast_addr_din_over_ver_htrees = 0;

    // Other flags
    input->force_cache_config = false;
    input->print_input_args = false;
    input->nsets = 0;  // Let CACTI calculate
    input->block_sz = config.line_size;
    input->tag_assoc = 1;
    input->data_assoc = 1;
    input->is_seq_acc = false;
    input->fully_assoc = false;

    // Additional parameters
    input->add_ecc_b_ = false;

    return input;
}

void CACTIWrapper::runCACTI() {
    try {
        // Create CACTI input parameters
        cacti_input_ = createCACTIInput(config_);

        // Run CACTI through its interface
        std::cout << "[CACTIWrapper] Running CACTI analysis..." << std::endl;

        // Call CACTI interface
        uca_org_t result = cacti_interface(cacti_input_);

        // Allocate and copy result
        cacti_result_ = new uca_org_t();
        *cacti_result_ = result;

        // Check validity
        if (!cacti_result_->valid) {
            valid_ = false;
            error_message_ = "CACTI returned invalid result - configuration may be infeasible";
            std::cerr << "[CACTIWrapper] " << error_message_ << std::endl;
        } else {
            valid_ = true;
            std::cout << "[CACTIWrapper] CACTI analysis complete" << std::endl;
        }

    } catch (const std::exception& e) {
        valid_ = false;
        error_message_ = std::string("CACTI execution failed: ") + e.what();
        std::cerr << "[CACTIWrapper] " << error_message_ << std::endl;
    }
}

//=============================================================================
// Query functions
//=============================================================================

double CACTIWrapper::getAccessTime() const {
    if (!valid_ || !cacti_result_) return 0.0;
    return cacti_result_->access_time;  // in seconds
}

double CACTIWrapper::getCycleTime() const {
    if (!valid_ || !cacti_result_) return 0.0;
    return cacti_result_->cycle_time;  // in seconds
}

double CACTIWrapper::getArea() const {
    if (!valid_ || !cacti_result_) return 0.0;
    return cacti_result_->area;  // in mm^2
}

double CACTIWrapper::getDynamicReadEnergy() const {
    if (!valid_ || !cacti_result_) return 0.0;
    // CACTI returns energy in nJ
    return cacti_result_->power.readOp.dynamic * 1e9;  // Convert J to nJ
}

double CACTIWrapper::getDynamicWriteEnergy() const {
    if (!valid_ || !cacti_result_) return 0.0;
    // CACTI returns energy in nJ
    return cacti_result_->power.writeOp.dynamic * 1e9;  // Convert J to nJ
}

double CACTIWrapper::getLeakagePower() const {
    if (!valid_ || !cacti_result_) return 0.0;
    // CACTI returns power in W, convert to mW
    return cacti_result_->power.readOp.leakage * 1000.0;  // Convert W to mW
}

double CACTIWrapper::getReadDynamicPower() const {
    if (!valid_ || !cacti_result_) return 0.0;
    // Power = Energy / Time, convert to mW
    double energy_j = cacti_result_->power.readOp.dynamic;
    double time_s = cacti_result_->access_time;
    if (time_s > 0) {
        return (energy_j / time_s) * 1000.0;  // Convert W to mW
    }
    return 0.0;
}

double CACTIWrapper::getWriteDynamicPower() const {
    if (!valid_ || !cacti_result_) return 0.0;
    // Power = Energy / Time, convert to mW
    double energy_j = cacti_result_->power.writeOp.dynamic;
    double time_s = cacti_result_->access_time;
    if (time_s > 0) {
        return (energy_j / time_s) * 1000.0;  // Convert W to mW
    }
    return 0.0;
}

double CACTIWrapper::getGateLeakagePower() const {
    if (!valid_ || !cacti_result_) return 0.0;
    return cacti_result_->power.readOp.gate_leakage * 1000.0;  // Convert W to mW
}

double CACTIWrapper::getCacheHeight() const {
    if (!valid_ || !cacti_result_) return 0.0;
    return cacti_result_->cache_ht;  // in mm
}

double CACTIWrapper::getCacheWidth() const {
    if (!valid_ || !cacti_result_) return 0.0;
    return cacti_result_->cache_len;  // in mm
}

double CACTIWrapper::getAreaEfficiency() const {
    if (!valid_ || !cacti_result_) return 0.0;
    return cacti_result_->area_efficiency;
}

Cycle CACTIWrapper::getAccessLatencyCycles(double freq_hz) const {
    if (!valid_ || !cacti_result_) return 0;
    double time_s = getAccessTime();
    return static_cast<Cycle>(std::ceil(time_s * freq_hz));
}

Cycle CACTIWrapper::getCycleTimeCycles(double freq_hz) const {
    if (!valid_ || !cacti_result_) return 0;
    double time_s = getCycleTime();
    return static_cast<Cycle>(std::ceil(time_s * freq_hz));
}

bool CACTIWrapper::isValid() const {
    return valid_;
}

std::string CACTIWrapper::getErrorMessage() const {
    return error_message_;
}

void CACTIWrapper::printDetailedResults() const {
    if (!valid_ || !cacti_result_) {
        std::cout << "[CACTIWrapper] No valid results available" << std::endl;
        if (!error_message_.empty()) {
            std::cout << "  Error: " << error_message_ << std::endl;
        }
        return;
    }

    std::cout << "\n=== CACTI Results ===" << std::endl;
    std::cout << "Configuration:" << std::endl;
    std::cout << "  Capacity: " << (config_.capacity_bytes / 1024) << " KB" << std::endl;
    std::cout << "  Line Size: " << config_.line_size << " bytes" << std::endl;
    std::cout << "  Associativity: " << config_.associativity << "-way" << std::endl;
    std::cout << "  Banks: " << config_.banks << std::endl;
    std::cout << "  Technology: " << config_.tech_node_nm << " nm" << std::endl;

    std::cout << "\nTiming:" << std::endl;
    std::cout << "  Access Time: " << (getAccessTime() * 1e9) << " ns" << std::endl;
    std::cout << "  Cycle Time: " << (getCycleTime() * 1e9) << " ns" << std::endl;

    std::cout << "\nArea:" << std::endl;
    std::cout << "  Total Area: " << getArea() << " mm^2" << std::endl;
    std::cout << "  Height: " << getCacheHeight() << " mm" << std::endl;
    std::cout << "  Width: " << getCacheWidth() << " mm" << std::endl;
    std::cout << "  Efficiency: " << (getAreaEfficiency() * 100.0) << "%" << std::endl;

    std::cout << "\nEnergy:" << std::endl;
    std::cout << "  Read Energy: " << getDynamicReadEnergy() << " nJ" << std::endl;
    std::cout << "  Write Energy: " << getDynamicWriteEnergy() << " nJ" << std::endl;

    std::cout << "\nPower:" << std::endl;
    std::cout << "  Read Dynamic Power: " << getReadDynamicPower() << " mW" << std::endl;
    std::cout << "  Write Dynamic Power: " << getWriteDynamicPower() << " mW" << std::endl;
    std::cout << "  Leakage Power: " << getLeakagePower() << " mW" << std::endl;
    std::cout << "  Gate Leakage Power: " << getGateLeakagePower() << " mW" << std::endl;
    std::cout << "=====================\n" << std::endl;
}

} // namespace pimid

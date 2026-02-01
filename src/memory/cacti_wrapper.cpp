#include "memory/cacti_wrapper.h"
#include <iostream>
#include <cmath>
#include <cstring>
#include <stdexcept>

// Include CACTI headers if available
#ifdef HAVE_CACTI
#include "cacti_interface.h"
#endif

namespace pimid {

#ifdef HAVE_CACTI

//=============================================================================
// CACTIWrapper Implementation (WITH CACTI)
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

    // Zero-initialize all members to avoid undefined behavior
    // InputParameter's constructor only initializes a few boolean fields,
    // leaving most fields uninitialized which can cause segfaults
    std::memset(input, 0, sizeof(InputParameter));

    // Re-apply the defaults that the constructor would have set
    input->array_power_gated = false;
    input->bitline_floating = false;
    input->wl_power_gated = false;
    input->cl_power_gated = false;
    input->interconect_power_gated = false;
    input->power_gating = false;
    input->cl_vertical = true;

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
    input->is_3d_mem = false;  // Not 3D memory
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

    // Interconnect (1 = conservative, matching CACTI's default)
    input->ic_proj_type = 1;      // Conservative (required for reliable results)
    input->wire_is_mat_type = 2;  // Semi-global
    input->wire_os_mat_type = 2;  // Semi-global
    input->wt = (Wire_type)0;     // Global wires with repeaters
    input->force_wiretype = 0;    // Don't force wire type

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

    // Print detail level (0 = minimal output)
    input->print_detail = 0;

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

//=============================================================================
// CACTI 7.0 Subarray-Level Characteristics
//=============================================================================

double CACTIWrapper::getDecoderDelay() const {
    if (!valid_ || !cacti_result_) return 0.0;
    // Row predecoder + row decoder delay
    return cacti_result_->data_array.delay_row_predecode_driver_and_block +
           cacti_result_->data_array.delay_row_decoder;
}

double CACTIWrapper::getWordlineDelay() const {
    if (!valid_ || !cacti_result_) return 0.0;
    // Bitline delay includes wordline activation in CACTI's model
    // We approximate wordline delay as a portion of the total path
    // This is captured in the delay_bitlines field
    return cacti_result_->data_array.delay_bitlines * 0.3;  // ~30% for wordline
}

double CACTIWrapper::getBitlineDelay() const {
    if (!valid_ || !cacti_result_) return 0.0;
    return cacti_result_->data_array.delay_bitlines;
}

double CACTIWrapper::getSenseAmpDelay() const {
    if (!valid_ || !cacti_result_) return 0.0;
    return cacti_result_->data_array.delay_sense_amp;
}

double CACTIWrapper::getSubarrayOutputDelay() const {
    if (!valid_ || !cacti_result_) return 0.0;
    return cacti_result_->data_array.delay_subarray_output_driver;
}

double CACTIWrapper::getHtreeDelay() const {
    if (!valid_ || !cacti_result_) return 0.0;
    return cacti_result_->data_array.delay_input_htree +
           cacti_result_->data_array.delay_output_htree;
}

uint32_t CACTIWrapper::getSubarrayRows() const {
    if (!valid_ || !cacti_result_ || !cacti_result_->data_array2) return 0;
    return static_cast<uint32_t>(cacti_result_->data_array2->num_row_subarray);
}

uint32_t CACTIWrapper::getSubarrayCols() const {
    if (!valid_ || !cacti_result_ || !cacti_result_->data_array2) return 0;
    return static_cast<uint32_t>(cacti_result_->data_array2->num_col_subarray);
}

uint32_t CACTIWrapper::getSubarraysPerMat() const {
    if (!valid_ || !cacti_result_ || !cacti_result_->data_array2) return 0;
    // In CACTI, Ndbl * Ndwl gives the number of subarrays
    // Per mat is typically 2x2 = 4 subarrays
    return static_cast<uint32_t>(cacti_result_->data_array2->Ndbl *
                                  cacti_result_->data_array2->Ndwl);
}

uint32_t CACTIWrapper::getMatsPerBank() const {
    if (!valid_ || !cacti_result_ || !cacti_result_->data_array2) return 0;
    // Number of active mats gives us mats per bank
    return static_cast<uint32_t>(cacti_result_->data_array2->num_active_mats);
}

double CACTIWrapper::getWordlineCapacitance() const {
    if (!valid_ || !cacti_result_ || !cacti_result_->data_array2) return 0.0;
    // CACTI stores this in the subarray dimensions
    // We estimate from subarray width and technology
    double subarray_width = cacti_result_->data_array2->subarray_length;  // in um
    double tech_um = config_.tech_node_nm / 1000.0;
    // C_wl ~ width * capacitance_per_um (typical ~0.2fF/um for interconnect)
    return subarray_width * 0.2e-15;  // in Farads
}

double CACTIWrapper::getWordlineResistance() const {
    if (!valid_ || !cacti_result_ || !cacti_result_->data_array2) return 0.0;
    // Estimate from subarray width and technology
    double subarray_width = cacti_result_->data_array2->subarray_length;  // in um
    // R_wl ~ width * resistance_per_um (typical ~10 Ohm/um for poly/metal)
    return subarray_width * 10.0;  // in Ohms
}

double CACTIWrapper::getBitlineCapacitance() const {
    if (!valid_ || !cacti_result_ || !cacti_result_->data_array2) return 0.0;
    // Estimate from subarray height
    double subarray_height = cacti_result_->data_array2->subarray_height;  // in um
    // C_bl ~ height * capacitance_per_um (typical ~0.3fF/um for metal)
    return subarray_height * 0.3e-15;  // in Farads
}

double CACTIWrapper::getDecoderEnergy() const {
    if (!valid_ || !cacti_result_) return 0.0;
    // Row predecoder + row decoder energy (convert J to nJ)
    return (cacti_result_->data_array.power_row_predecoder_drivers.readOp.dynamic +
            cacti_result_->data_array.power_row_predecoder_blocks.readOp.dynamic +
            cacti_result_->data_array.power_row_decoders.readOp.dynamic) * 1e9;
}

double CACTIWrapper::getWordlineEnergy() const {
    if (!valid_ || !cacti_result_ || !cacti_result_->data_array2) return 0.0;
    // Use the energy breakdown from mem_array if available
    return cacti_result_->data_array2->energy_local_wordline * 1e9;  // Convert J to nJ
}

double CACTIWrapper::getBitlineEnergy() const {
    if (!valid_ || !cacti_result_) return 0.0;
    // Bitline energy (convert J to nJ)
    return cacti_result_->data_array.power_bitlines.readOp.dynamic * 1e9;
}

double CACTIWrapper::getSenseAmpEnergy() const {
    if (!valid_ || !cacti_result_) return 0.0;
    // Sense amp energy (convert J to nJ)
    return cacti_result_->data_array.power_sense_amps.readOp.dynamic * 1e9;
}

double CACTIWrapper::getArrayLeakage() const {
    if (!valid_ || !cacti_result_ || !cacti_result_->data_array2) return 0.0;
    // Array leakage in mW
    return cacti_result_->data_array2->array_leakage * 1000.0;  // Convert W to mW
}

double CACTIWrapper::getWordlineLeakage() const {
    if (!valid_ || !cacti_result_ || !cacti_result_->data_array2) return 0.0;
    // Wordline leakage in mW
    return cacti_result_->data_array2->wl_leakage * 1000.0;  // Convert W to mW
}

double CACTIWrapper::getColumnLeakage() const {
    if (!valid_ || !cacti_result_ || !cacti_result_->data_array2) return 0.0;
    // Column leakage in mW
    return cacti_result_->data_array2->cl_leakage * 1000.0;  // Convert W to mW
}

double CACTIWrapper::getSubarrayArea() const {
    if (!valid_ || !cacti_result_ || !cacti_result_->data_array2) return 0.0;
    // Subarray area in mm^2
    return cacti_result_->data_array2->area_subarray;
}

double CACTIWrapper::getCellArea() const {
    if (!valid_ || !cacti_result_) return 0.0;
    // Cell area from subarray dimensions
    double subarray_area = cacti_result_->data_array.subarray_memory_cell_area_height *
                           cacti_result_->data_array.subarray_memory_cell_area_width;
    uint32_t rows = getSubarrayRows();
    uint32_t cols = getSubarrayCols();
    if (rows > 0 && cols > 0) {
        return (subarray_area / (rows * cols)) * 1e6;  // Convert mm^2 to um^2
    }
    return 0.0;
}

void CACTIWrapper::printDetailedResults() const {
    if (!valid_ || !cacti_result_) {
        std::cout << "[CACTIWrapper] No valid results available" << std::endl;
        if (!error_message_.empty()) {
            std::cout << "  Error: " << error_message_ << std::endl;
        }
        return;
    }

    std::cout << "\n=== CACTI 7.0 Results ===" << std::endl;
    std::cout << "Configuration:" << std::endl;
    std::cout << "  Capacity: " << (config_.capacity_bytes / 1024) << " KB" << std::endl;
    std::cout << "  Line Size: " << config_.line_size << " bytes" << std::endl;
    std::cout << "  Associativity: " << config_.associativity << "-way" << std::endl;
    std::cout << "  Banks: " << config_.banks << std::endl;
    std::cout << "  Technology: " << config_.tech_node_nm << " nm" << std::endl;

    std::cout << "\nTiming:" << std::endl;
    std::cout << "  Access Time: " << (getAccessTime() * 1e9) << " ns" << std::endl;
    std::cout << "  Cycle Time: " << (getCycleTime() * 1e9) << " ns" << std::endl;

    std::cout << "\nTiming Breakdown (subarray-level):" << std::endl;
    std::cout << "  Decoder Delay: " << (getDecoderDelay() * 1e9) << " ns" << std::endl;
    std::cout << "  Wordline Delay: " << (getWordlineDelay() * 1e9) << " ns" << std::endl;
    std::cout << "  Bitline Delay: " << (getBitlineDelay() * 1e9) << " ns" << std::endl;
    std::cout << "  Sense Amp Delay: " << (getSenseAmpDelay() * 1e9) << " ns" << std::endl;
    std::cout << "  Subarray Output Delay: " << (getSubarrayOutputDelay() * 1e9) << " ns" << std::endl;
    std::cout << "  H-tree Delay: " << (getHtreeDelay() * 1e9) << " ns" << std::endl;

    std::cout << "\nSubarray Organization:" << std::endl;
    std::cout << "  Rows per Subarray: " << getSubarrayRows() << std::endl;
    std::cout << "  Cols per Subarray: " << getSubarrayCols() << std::endl;
    std::cout << "  Subarrays per Mat: " << getSubarraysPerMat() << std::endl;
    std::cout << "  Mats per Bank: " << getMatsPerBank() << std::endl;

    std::cout << "\nElectrical Parameters:" << std::endl;
    std::cout << "  Wordline Capacitance: " << (getWordlineCapacitance() * 1e15) << " fF" << std::endl;
    std::cout << "  Wordline Resistance: " << getWordlineResistance() << " Ohm" << std::endl;
    std::cout << "  Bitline Capacitance: " << (getBitlineCapacitance() * 1e15) << " fF" << std::endl;

    std::cout << "\nArea:" << std::endl;
    std::cout << "  Total Area: " << getArea() << " mm^2" << std::endl;
    std::cout << "  Height: " << getCacheHeight() << " mm" << std::endl;
    std::cout << "  Width: " << getCacheWidth() << " mm" << std::endl;
    std::cout << "  Efficiency: " << (getAreaEfficiency() * 100.0) << "%" << std::endl;
    std::cout << "  Subarray Area: " << (getSubarrayArea() * 1e6) << " um^2" << std::endl;
    std::cout << "  Cell Area: " << getCellArea() << " um^2" << std::endl;

    std::cout << "\nEnergy (per access):" << std::endl;
    std::cout << "  Read Energy: " << getDynamicReadEnergy() << " nJ" << std::endl;
    std::cout << "  Write Energy: " << getDynamicWriteEnergy() << " nJ" << std::endl;

    std::cout << "\nEnergy Breakdown (subarray-level):" << std::endl;
    std::cout << "  Decoder Energy: " << getDecoderEnergy() << " nJ" << std::endl;
    std::cout << "  Wordline Energy: " << getWordlineEnergy() << " nJ" << std::endl;
    std::cout << "  Bitline Energy: " << getBitlineEnergy() << " nJ" << std::endl;
    std::cout << "  Sense Amp Energy: " << getSenseAmpEnergy() << " nJ" << std::endl;

    std::cout << "\nPower:" << std::endl;
    std::cout << "  Read Dynamic Power: " << getReadDynamicPower() << " mW" << std::endl;
    std::cout << "  Write Dynamic Power: " << getWriteDynamicPower() << " mW" << std::endl;
    std::cout << "  Leakage Power: " << getLeakagePower() << " mW" << std::endl;
    std::cout << "  Gate Leakage Power: " << getGateLeakagePower() << " mW" << std::endl;

    std::cout << "\nLeakage Breakdown (subarray-level):" << std::endl;
    std::cout << "  Array Leakage: " << getArrayLeakage() << " mW" << std::endl;
    std::cout << "  Wordline Leakage: " << getWordlineLeakage() << " mW" << std::endl;
    std::cout << "  Column Leakage: " << getColumnLeakage() << " mW" << std::endl;
    std::cout << "=============================\n" << std::endl;
}

#else  // !HAVE_CACTI

//=============================================================================
// CACTIWrapper Stub Implementation (WITHOUT CACTI)
//=============================================================================

CACTIWrapper::CACTIWrapper(const SRAMConfig& config)
    : config_(config)
    , cacti_result_(nullptr)
    , cacti_input_(nullptr)
    , initialized_(false)
    , valid_(true)
    , error_message_("")
{
    std::cerr << "[CACTIWrapper] WARNING: CACTI not available, using placeholder values" << std::endl;
}

CACTIWrapper::~CACTIWrapper() {}

void CACTIWrapper::initialize() {
    initialized_ = true;
    std::cout << "[CACTIWrapper] Using placeholder values (CACTI not compiled)" << std::endl;
}

void CACTIWrapper::runCACTI() {}
void CACTIWrapper::validateConfiguration() { valid_ = true; }
bool CACTIWrapper::isValid() const { return true; }
std::string CACTIWrapper::getErrorMessage() const { return ""; }

void CACTIWrapper::reconfigure(const SRAMConfig& config) {
    config_ = config;
    initialized_ = false;
}

// Return placeholder values
double CACTIWrapper::getAccessTime() const { return 2.0e-9; }  // 2ns
double CACTIWrapper::getCycleTime() const { return 2.0e-9; }
double CACTIWrapper::getArea() const { return 1.0; }
double CACTIWrapper::getCacheHeight() const { return 1.0; }
double CACTIWrapper::getCacheWidth() const { return 1.0; }
double CACTIWrapper::getAreaEfficiency() const { return 0.5; }
double CACTIWrapper::getDynamicReadEnergy() const { return 0.1; }
double CACTIWrapper::getDynamicWriteEnergy() const { return 0.1; }
double CACTIWrapper::getReadDynamicPower() const { return 10.0; }
double CACTIWrapper::getWriteDynamicPower() const { return 10.0; }
double CACTIWrapper::getLeakagePower() const { return 5.0; }
double CACTIWrapper::getGateLeakagePower() const { return 2.0; }

Cycle CACTIWrapper::getAccessLatencyCycles(double freq_hz) const {
    return static_cast<Cycle>(getAccessTime() * freq_hz);
}

Cycle CACTIWrapper::getCycleTimeCycles(double freq_hz) const {
    return static_cast<Cycle>(getCycleTime() * freq_hz);
}

void CACTIWrapper::printDetailedResults() const {
    std::cout << "[CACTIWrapper] CACTI not available - using placeholder values" << std::endl;
}

InputParameter* CACTIWrapper::createCACTIInput(const SRAMConfig& config) {
    return nullptr;
}

// Stub implementations for subarray-level methods (without CACTI)
double CACTIWrapper::getDecoderDelay() const { return 0.5e-9; }         // 0.5 ns
double CACTIWrapper::getWordlineDelay() const { return 0.3e-9; }        // 0.3 ns
double CACTIWrapper::getBitlineDelay() const { return 0.8e-9; }         // 0.8 ns
double CACTIWrapper::getSenseAmpDelay() const { return 0.2e-9; }        // 0.2 ns
double CACTIWrapper::getSubarrayOutputDelay() const { return 0.1e-9; }  // 0.1 ns
double CACTIWrapper::getHtreeDelay() const { return 0.1e-9; }           // 0.1 ns

uint32_t CACTIWrapper::getSubarrayRows() const { return 512; }
uint32_t CACTIWrapper::getSubarrayCols() const { return 512; }
uint32_t CACTIWrapper::getSubarraysPerMat() const { return 4; }
uint32_t CACTIWrapper::getMatsPerBank() const { return 8; }

double CACTIWrapper::getWordlineCapacitance() const { return 100e-15; }  // 100 fF
double CACTIWrapper::getWordlineResistance() const { return 1000.0; }    // 1 kOhm
double CACTIWrapper::getBitlineCapacitance() const { return 150e-15; }   // 150 fF

double CACTIWrapper::getDecoderEnergy() const { return 0.01; }    // 0.01 nJ
double CACTIWrapper::getWordlineEnergy() const { return 0.02; }   // 0.02 nJ
double CACTIWrapper::getBitlineEnergy() const { return 0.03; }    // 0.03 nJ
double CACTIWrapper::getSenseAmpEnergy() const { return 0.01; }   // 0.01 nJ

double CACTIWrapper::getArrayLeakage() const { return 1.0; }      // 1 mW
double CACTIWrapper::getWordlineLeakage() const { return 0.5; }   // 0.5 mW
double CACTIWrapper::getColumnLeakage() const { return 0.5; }     // 0.5 mW

double CACTIWrapper::getSubarrayArea() const { return 0.001; }    // 0.001 mm^2
double CACTIWrapper::getCellArea() const { return 0.1; }          // 0.1 um^2

#endif  // HAVE_CACTI

}  // namespace pimid

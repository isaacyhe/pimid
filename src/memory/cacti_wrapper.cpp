#include "memory/cacti_wrapper.h"
#include <iostream>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <unistd.h>
#include <climits>

// Include CACTI headers if available
#ifdef HAVE_CACTI
#include "cacti_interface.h"
#include "parameter.h"   // 1.11.45 (E30): g_ip, to clear the global at our boundary
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
        if (g_ip == cacti_input_) g_ip = nullptr;   // 1.11.45 (E30): never dangle
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

    if (config_.quiet) return;
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

    // Cell technology flavor
    input->ram_cell_tech_type = static_cast<unsigned int>(config.cell_type);
    input->peri_global_tech_type = 0;  // Peripheral always ITRS-HP
    input->data_arr_ram_cell_tech_type = static_cast<unsigned int>(config.cell_type);
    input->data_arr_peri_global_tech_type = 0;
    input->tag_arr_ram_cell_tech_type = 0;  // Tags always SRAM
    input->tag_arr_peri_global_tech_type = 0;

    /* 1.11.30 (user ruling E5): from the CALLER, so CACTI and McPAT model one
     * metal stack. This was pinned to 1 here while McPAT defaulted to 0, so the
     * same die had lossier wires in its arrays than in its cores. The original
     * note read "Conservative (required for reliable results)" -- conservative
     * remains the DEFAULT, now for a stated physical reason: the aggressive
     * column sets barrier_thickness = 0 at every node, and a copper wire with
     * no diffusion barrier cannot be built. */
    input->ic_proj_type = (config.ic_proj_type == 0) ? 0 : 1;
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

    // Main memory / DRAM parameters
    input->page_sz_bits = config.page_sz_bits;
    input->burst_len = config.burst_len;
    input->int_prefetch_w = config.int_prefetch_w;

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

        // CACTI uses relative paths to load tech_params/*.dat files.
        // We must chdir to the CACTI source directory before calling it.
        char saved_cwd[PATH_MAX];
        bool cwd_saved = (getcwd(saved_cwd, sizeof(saved_cwd)) != nullptr);

#ifdef CACTI_DATA_DIR
        if (chdir(CACTI_DATA_DIR) != 0) {
            std::cerr << "[CACTIWrapper] Warning: Could not chdir to CACTI data directory: "
                      << CACTI_DATA_DIR << std::endl;
        }
#endif

        // Call CACTI interface
        uca_org_t result = cacti_interface(cacti_input_);
        /* 1.11.45 (audit E30): the GLOBAL never outlives the call.
         * cacti_interface() points g_ip at our input and leaves it there;
         * when this wrapper is destroyed, delete cacti_input_ would turn the
         * global into a dangling pointer that every later libcacti7 consumer
         * (the McPAT fork reads g_ip->F_sz_nm in its area/energy formulas;
         * CactiIOWrapper used to save/restore THROUGH it) can silently read
         * -- or write -- as freed memory. The ecosystem worked only on the
         * unstated invariant that every reader re-inits g_ip immediately
         * before computing; this makes the rule real at our boundary. */
        g_ip = nullptr;

        // Restore original working directory
        if (cwd_saved) {
            if (chdir(saved_cwd) != 0) {
                std::cerr << "[CACTIWrapper] Warning: Could not restore working directory" << std::endl;
            }
        }

        // Allocate and copy result
        cacti_result_ = new uca_org_t();
        *cacti_result_ = result;

        // Check validity: CACTI 7.0's uca_org_t::valid is never set to true
        // (vestigial field). Instead check that solve() produced usable results.
        if (cacti_result_->access_time > 0 && cacti_result_->data_array2 != nullptr) {
            valid_ = true;
            std::cout << "[CACTIWrapper] CACTI analysis complete" << std::endl;
        } else {
            valid_ = false;
            error_message_ = "CACTI returned invalid result - configuration may be infeasible";
            std::cerr << "[CACTIWrapper] " << error_message_ << std::endl;
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
    return cacti_result_->area / 1e6;  // CACTI native um^2 -> mm^2
}

/* 1.11.14 (#122): the JEDEC calibration, moved INSIDE the tool that owns the
 * array model. It used to be computed by the caller in main.cpp, at two sites,
 * with the density and generation tables living there too -- model logic in
 * the orchestrator, which the borders rule forbids.
 *
 * The scope gate is the whole point. McPAT links this same library; a DRAM
 * vendor-density factor applied to its cache and register-file queries would
 * be nonsense. So calibration requires BOTH a commodity-DRAM main-memory
 * query AND a named technology, and anything else returns raw CACTI
 * unchanged. */
/* 1.11.19 (user decision D11): FULL-DIE density, one published measurement
 * per row. "mm^2/die" now means what a reviewer assumes and can check
 * against a die photo.
 *
 * The previous table was ARRAY-REGION and wrong in two independent ways,
 * both confirmed against published measurements (2026-08-15):
 *   MAGNITUDE  every row was 2.4x-22x denser than silicon.
 *   ORDERING   it ranked HBM the DENSEST technology; in silicon HBM is the
 *              LEAST dense -- TSVs and a very wide interface cost area, and
 *              SK Hynix's own D1z DDR4 is ~85% denser than their HBM3.
 *              Since reported area = CACTI x k with k calibrated here, that
 *              inversion systematically flattered HBM.
 *     measured: LPDDR5 > DDR5 > DDR4 > GDDR6 > HBM3
 *     old:      HBM3   > GDDR6 > LPDDR5 > DDR5 > DDR4
 *
 * Values are MB/mm^2 = (Gb/mm^2) x 128. Each row states the part, the
 * capacity, the die area and the source. Rows we could NOT source are
 * marked and reported at runtime rather than quietly invented. */
double CACTIWrapper::vendorDieDensity(const std::string& tech) {
    // DDR4: SK Hynix D1z, 0.296 Gb/mm^2 (SemiAnalysis/TechInsights)
    if (tech == "DDR4")   return 0.296 * 128.0;   // 37.9
    // DDR5: Micron D1a, 8 Gb / 25.41 mm^2 = 0.315 Gb/mm^2 (TechInsights)
    if (tech == "DDR5")   return 0.315 * 128.0;   // 40.3
    // LPDDR5: Samsung D1z, 16 Gb / 43.98 mm^2 (TechInsights)
    if (tech == "LPDDR5") return (16.0 / 43.98) * 128.0;   // 46.6
    // GDDR6: Samsung K4Z80165BC D1z, 8 Gb / 37.03 mm^2 whole die
    // (TechInsights floorplan analysis; NOTE the part is 8 Gb -- one
    // secondary article labels it 16 Gb, which would double the density)
    if (tech == "GDDR6")  return (8.0 / 37.03) * 128.0;    // 27.7
    // HBM3: SK Hynix, 0.16 Gb/mm^2 (SemiAnalysis)
    if (tech == "HBM3")   return 0.160 * 128.0;   // 20.5
    // DDR3: SK Hynix 23nm 4 Gb DDR3 SDRAM, 30.9 mm^2 -- ISSCC 2012 Paper 2.3
    // ("Hynix demonstrates the smallest 23nm 30.9mm2 4Gb DDR3 SDRAM by using
    //  an open bitline architecture with 6F2 cell", ISSCC 2012 press kit).
    // This is the densest DDR3 generation shipped; our class map puts DDR3 at
    // 3x/2x nm, and 23 nm is that 2x end.
    if (tech == "DDR3")   return (4.0 / 30.9) * 128.0;   // 16.6
    /* HBM2: Samsung 20nm HBM Gen2 core die. Two statements from the SAME
     * paper, in both its versions:
     *   die area  "The HBM chip is fabricated using a 20nm DRAM process and
     *              the chip size is 12x8mm2"       -> 96 mm^2
     *   capacity  "each core die has 8 Gb DRAM cell array with additional
     *              1 Gb [for ECC]"                 -> 8 Gb user capacity
     * K. Sohn et al., ISSCC 2016, paper 18.2; and the journal version,
     * IEEE JSSC vol.52 no.1 pp.250-260, Jan 2017 (misc/sohn2016.pdf,
     * misc/sohn2017.pdf).
     *
     * 8 Gb / 96 mm^2 = 0.0833 Gb/mm^2. USER capacity is the numerator, to
     * match every other row here (vendors advertise HBM2 stack capacity
     * excluding the ECC bits); counting the full 9 Gb cell array instead
     * would give 12.0 MB/mm^2.
     *
     * The 12x8 is rounded in the source -- it sits within a few percent of
     * the JEDEC HBM package outline, which for HBM is nearly the die
     * footprint. Treat as +/-5%; it does not move any conclusion.
     *
     * This replaces the 1.11.19 placeholder (HBM3 x 0.70 = 14.3), which was
     * 34% too dense. HBM2 is now the LEAST dense row in the table by a wide
     * margin -- about 4x less dense than LPDDR5. */
    if (tech == "HBM2")   return (8.0 / 96.0) * 128.0;   // 10.7
    return 0.296 * 128.0;                 // default: DDR4-class
}

/* 1.11.19 (D11): does this row rest on a published measurement? Rows that
 * do not are printed as DERIVED wherever the die area is reported, so a
 * number can never be cited as sourced when it is not. */
bool CACTIWrapper::vendorDieDensitySourced(const std::string& tech) {
    /* 2026-08-15: HBM2 joined this list -- Sohn et al. ISSCC 2016 18.2 /
     * JSSC Jan 2017. Every row in vendorDieDensity() now rests on a
     * published measurement; there are no derived rows left. */
    return tech == "DDR3" || tech == "DDR4" || tech == "DDR5" ||
           tech == "LPDDR5" || tech == "GDDR6" ||
           tech == "HBM2" || tech == "HBM3";
}

/* 1.11.19 (D11): the ARRAY fraction of a full die -- what the derived
 * "of which array ~Y mm^2" line reports. Returns <0 when unknown, and the
 * line is then omitted rather than guessed: cell-array efficiency is a
 * per-design figure we have not sourced per technology, and the whole point
 * of D11 is that the array and the die are different quantities. */
double CACTIWrapper::vendorArrayFraction(const std::string& tech) {
    (void)tech;
    return -1.0;   // not sourced yet; see docs/power.md
}

int CACTIWrapper::generationTableNm(const std::string& tech) {
    return (tech == "DDR3") ? 32 : 22;
}

const char* CACTIWrapper::generationClass(const std::string& tech) {
    if (tech == "DDR3")   return "3x/2x";
    if (tech == "DDR4")   return "1x";
    if (tech == "DDR5")   return "1a";
    if (tech == "LPDDR5") return "1a";
    if (tech == "GDDR6")  return "1y/1z";
    if (tech == "HBM2")   return "1y";
    if (tech == "HBM3")   return "1a/1b";
    return "1x";
}

CACTIWrapper::CalibratedArea CACTIWrapper::getCalibratedDieArea() const {
    CalibratedArea out;
    out.raw_mm2 = getArea();
    out.area_mm2 = out.raw_mm2;
    if (!valid_ || out.raw_mm2 <= 0.0) return out;
    /* THE SCOPE GATE: commodity-DRAM main memory, with a technology named.
     * Every SRAM/cache/RF/TLB query -- i.e. everything McPAT asks -- fails
     * this and leaves with raw CACTI. */
    if (config_.cell_type != COMM_DRAM || !config_.is_main_memory ||
        config_.memory_tech.empty()) {
        return out;
    }
    /* 1.11.19 (gate 1129 P1): UNITS. vendorDieDensity() returns MEGABYTES
     * per mm^2 (its rows are Gb/mm^2 x 128). The capacity fed to it was in
     * MEGABITS -- the old local was even named chip_mbit -- so every die
     * area came out 8x too large. It went unnoticed because the pre-1.11.19
     * density table was itself ~4-5x too dense, and the two errors partly
     * cancelled; correcting only the density (D11) exposed the factor of 8
     * as a 22x jump that breached the reticle limit on HBM3. Both halves are
     * fixed now: MB divided by MB/mm^2. */
    double chip_mbyte = static_cast<double>(config_.capacity_bytes)
                      / (1024.0 * 1024.0);
    /* 1.11.45 (audit E29, user ruling): the x1.12 is GONE. Its provenance was
     * lost in the 1.11.14 migration, and it double-counted periphery: the
     * vendorDieDensity() rows are measured from REAL DIES (die photos and
     * vendor disclosures), so capacity/density is already a full-die area,
     * periphery, spare arrays and all. Multiplying a full-die anchor by a
     * periphery allowance charges it twice. Every DRAM die area shrinks by
     * 1/1.12 = 10.7% relative to 1.11.44. */
    double jedec_ref = chip_mbyte / vendorDieDensity(config_.memory_tech);
    if (!(jedec_ref > 0.0)) return out;
    out.k = jedec_ref / out.raw_mm2;
    out.area_mm2 = out.raw_mm2 * out.k;
    out.calibrated = true;
    return out;
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
    return cacti_result_->cache_ht / 1e3;  // CACTI native um -> mm
}

double CACTIWrapper::getCacheWidth() const {
    if (!valid_ || !cacti_result_) return 0.0;
    return cacti_result_->cache_len / 1e3;  // CACTI native um -> mm
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
    // CACTI native um^2 -> mm^2
    return cacti_result_->data_array2->area_subarray / 1e6;
}

double CACTIWrapper::getCellArea() const {
    if (!valid_ || !cacti_result_) return 0.0;
    // Cell area from subarray dimensions
    double subarray_area = cacti_result_->data_array.subarray_memory_cell_area_height *
                           cacti_result_->data_array.subarray_memory_cell_area_width;
    uint32_t rows = getSubarrayRows();
    uint32_t cols = getSubarrayCols();
    if (rows > 0 && cols > 0) {
        return subarray_area / (rows * cols);  // CACTI cell dims are um -> already um^2
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

#else
#error "CACTI 7.0 is mandatory for PIMID. Check external/cacti/ and CMakeLists.txt."
#endif  // HAVE_CACTI

}  // namespace pimid

#include "mcpat_wrapper.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <stdexcept>

// Note: Full McPAT integration requires XML parsing
// This is a simplified wrapper with placeholder implementations

namespace pimid {

//=============================================================================
// McPATWrapper Implementation
//=============================================================================

McPATWrapper::McPATWrapper(const SystemConfig& config)
    : config_(config)
    , mcpat_parser_(nullptr)
    , mcpat_processor_(nullptr)
    , total_cycles_(0)
    , busy_cycles_(0)
    , total_instructions_(0)
    , l1_reads_(0)
    , l1_writes_(0)
    , l2_reads_(0)
    , l2_writes_(0)
    , l3_reads_(0)
    , l3_writes_(0)
    , initialized_(false)
    , valid_(false)
    , power_computed_(false)
    , error_message_("")
{
}

McPATWrapper::~McPATWrapper() {
    // Clean up McPAT objects if allocated
    if (mcpat_processor_) {
        // Note: McPAT cleanup would go here
        mcpat_processor_ = nullptr;
    }
    if (mcpat_parser_) {
        mcpat_parser_ = nullptr;
    }
}

void McPATWrapper::initialize() {
    if (initialized_) {
        std::cerr << "[McPATWrapper] Warning: Already initialized" << std::endl;
        return;
    }

    validateConfiguration();
    if (!valid_) {
        throw std::runtime_error("[McPATWrapper] Invalid configuration: " + error_message_);
    }

    try {
        createMcPATInput();
        initialized_ = true;

        std::cout << "[McPATWrapper] Initialized with:" << std::endl;
        std::cout << "  Cores: " << config_.num_cores << std::endl;
        std::cout << "  Core Clock: " << config_.core_clock_mhz << " MHz" << std::endl;
        std::cout << "  L1I/L1D: " << (config_.l1i_size_bytes/1024) << "/"
                  << (config_.l1d_size_bytes/1024) << " KB" << std::endl;
        std::cout << "  L2: " << (config_.l2_size_bytes/1024) << " KB" << std::endl;
        std::cout << "  L3: " << (config_.l3_size_bytes/(1024*1024)) << " MB" << std::endl;
        std::cout << "  Technology: " << config_.tech_node_nm << " nm" << std::endl;

    } catch (const std::exception& e) {
        valid_ = false;
        error_message_ = std::string("Initialization failed: ") + e.what();
        throw;
    }
}

void McPATWrapper::reconfigure(const SystemConfig& config) {
    config_ = config;
    initialized_ = false;
    power_computed_ = false;
    initialize();
}

void McPATWrapper::validateConfiguration() {
    valid_ = true;
    error_message_ = "";

    if (config_.num_cores < 1 || config_.num_cores > 1024) {
        valid_ = false;
        error_message_ = "Number of cores out of range (1-1024)";
        return;
    }

    if (config_.core_clock_mhz < 100 || config_.core_clock_mhz > 10000) {
        valid_ = false;
        error_message_ = "Core clock out of range (100-10000 MHz)";
        return;
    }

    if (config_.tech_node_nm < 7 || config_.tech_node_nm > 90) {
        valid_ = false;
        error_message_ = "Technology node out of range (7nm - 90nm)";
        return;
    }
}

void McPATWrapper::setTotalCycles(uint64_t cycles) {
    total_cycles_ = cycles;
    power_computed_ = false;
}

void McPATWrapper::setBusyCycles(uint64_t cycles) {
    busy_cycles_ = cycles;
    power_computed_ = false;
}

void McPATWrapper::setTotalInstructions(uint64_t instructions) {
    total_instructions_ = instructions;
    power_computed_ = false;
}

void McPATWrapper::setL1Accesses(uint64_t reads, uint64_t writes) {
    l1_reads_ = reads;
    l1_writes_ = writes;
    power_computed_ = false;
}

void McPATWrapper::setL2Accesses(uint64_t reads, uint64_t writes) {
    l2_reads_ = reads;
    l2_writes_ = writes;
    power_computed_ = false;
}

void McPATWrapper::setL3Accesses(uint64_t reads, uint64_t writes) {
    l3_reads_ = reads;
    l3_writes_ = writes;
    power_computed_ = false;
}

void McPATWrapper::createMcPATInput() {
    std::cout << "[McPATWrapper] Creating McPAT configuration" << std::endl;

    if (!config_.xml_file.empty()) {
        // Use provided XML file
        std::cout << "  Using XML file: " << config_.xml_file << std::endl;
        valid_ = true;
        return;
    }

    // Generate XML configuration from parameters
    std::string xml_content = generateXMLConfig();

    // Write to temporary file for McPAT
    std::string temp_xml = "/tmp/mcpat_input.xml";
    std::ofstream xml_file(temp_xml);
    if (!xml_file) {
        throw std::runtime_error("Failed to create McPAT XML file");
    }

    xml_file << xml_content;
    xml_file.close();

    config_.xml_file = temp_xml;
    valid_ = true;

    std::cout << "  Generated XML configuration: " << temp_xml << std::endl;
}

void McPATWrapper::runMcPAT() {
    std::cout << "[McPATWrapper] Running McPAT power analysis..." << std::endl;

    // NOTE: Full McPAT integration would require:
    // 1. Linking against McPAT library (or running as subprocess)
    // 2. Parsing XML with ParseXML class
    // 3. Creating Processor object
    // 4. Calling computeEnergy() and displayEnergy()
    //
    // For now, we use analytical models that approximate McPAT results
    // The full integration is straightforward once McPAT is built:
    //
    //   #include "XML_Parse.h"
    //   #include "processor.h"
    //
    //   mcpat_parser_ = new ParseXML();
    //   mcpat_parser_->parse(config_.xml_file.c_str());
    //   mcpat_processor_ = new Processor(mcpat_parser_);
    //   mcpat_processor_->computeEnergy();
    //   mcpat_processor_->displayEnergy();
    //
    // The XML file format is already generated correctly below.

    valid_ = true;
}

void McPATWrapper::computePower() {
    if (!initialized_) {
        throw std::runtime_error("[McPATWrapper] Not initialized");
    }

    runMcPAT();
    extractResults();
    power_computed_ = true;

    std::cout << "[McPATWrapper] Power analysis complete" << std::endl;
    std::cout << "  Total Power: " << system_power_.total_power << " W" << std::endl;
}

void McPATWrapper::extractResults() {
    // Extract results from McPAT analysis
    // This is a simplified placeholder implementation

    // Calculate approximate power based on technology and frequency
    double tech_factor = 90.0 / config_.tech_node_nm;  // Power scales with tech node
    double freq_factor = config_.core_clock_mhz / 1000.0;  // GHz

    // Core power (dynamic + leakage)
    PowerMetrics core_power;
    double core_dynamic_per_core = 2.0 * freq_factor * tech_factor;  // ~2W per core at 22nm, 1GHz
    core_power.runtime_dynamic = core_dynamic_per_core * config_.num_cores;
    core_power.subthreshold_leakage = 0.5 * config_.num_cores * tech_factor;
    core_power.gate_leakage = 0.1 * config_.num_cores * tech_factor;
    core_power.total_leakage = core_power.subthreshold_leakage + core_power.gate_leakage;
    core_power.total_dynamic = core_power.runtime_dynamic;
    core_power.total_power = core_power.total_dynamic + core_power.total_leakage;
    component_power_[ComponentType::CORE] = core_power;

    // L1 Cache power
    PowerMetrics l1_power;
    l1_power.runtime_dynamic = 0.3 * config_.num_cores;
    l1_power.total_leakage = 0.1 * config_.num_cores;
    l1_power.total_power = l1_power.runtime_dynamic + l1_power.total_leakage;
    component_power_[ComponentType::L1_CACHE] = l1_power;

    // L2 Cache power
    PowerMetrics l2_power;
    l2_power.runtime_dynamic = 0.5 * config_.num_cores;
    l2_power.total_leakage = 0.2 * config_.num_cores;
    l2_power.total_power = l2_power.runtime_dynamic + l2_power.total_leakage;
    component_power_[ComponentType::L2_CACHE] = l2_power;

    // L3 Cache power (shared)
    PowerMetrics l3_power;
    double l3_size_mb = config_.l3_size_bytes / (1024.0 * 1024.0);
    l3_power.runtime_dynamic = 0.1 * l3_size_mb;
    l3_power.total_leakage = 0.05 * l3_size_mb;
    l3_power.total_power = l3_power.runtime_dynamic + l3_power.total_leakage;
    component_power_[ComponentType::L3_CACHE] = l3_power;

    // Memory Controller
    PowerMetrics mc_power;
    mc_power.runtime_dynamic = 1.0 * config_.num_memory_controllers;
    mc_power.total_leakage = 0.2 * config_.num_memory_controllers;
    mc_power.total_power = mc_power.runtime_dynamic + mc_power.total_leakage;
    component_power_[ComponentType::MEMORY_CONTROLLER] = mc_power;

    // NoC power
    PowerMetrics noc_power;
    if (config_.has_noc) {
        noc_power.runtime_dynamic = 0.5 * config_.num_cores;
        noc_power.total_leakage = 0.1 * config_.num_cores;
        noc_power.total_power = noc_power.runtime_dynamic + noc_power.total_leakage;
    }
    component_power_[ComponentType::NOC] = noc_power;

    // System total
    system_power_.runtime_dynamic = 0;
    system_power_.total_leakage = 0;
    for (const auto& pair : component_power_) {
        system_power_.runtime_dynamic += pair.second.runtime_dynamic;
        system_power_.total_leakage += pair.second.total_leakage;
    }
    system_power_.total_dynamic = system_power_.runtime_dynamic;
    system_power_.total_power = system_power_.total_dynamic + system_power_.total_leakage;
}

//=============================================================================
// Query functions
//=============================================================================

McPATWrapper::PowerMetrics McPATWrapper::getComponentPower(ComponentType component) const {
    auto it = component_power_.find(component);
    if (it != component_power_.end()) {
        return it->second;
    }
    return PowerMetrics();
}

McPATWrapper::PowerMetrics McPATWrapper::getSystemPower() const {
    return system_power_;
}

double McPATWrapper::getCorePower() const {
    return getComponentPower(ComponentType::CORE).total_power;
}

double McPATWrapper::getCachePower() const {
    double total = 0.0;
    total += getComponentPower(ComponentType::L1_CACHE).total_power;
    total += getComponentPower(ComponentType::L2_CACHE).total_power;
    total += getComponentPower(ComponentType::L3_CACHE).total_power;
    return total;
}

double McPATWrapper::getMemoryControllerPower() const {
    return getComponentPower(ComponentType::MEMORY_CONTROLLER).total_power;
}

double McPATWrapper::getNoCPower() const {
    return getComponentPower(ComponentType::NOC).total_power;
}

double McPATWrapper::getComponentArea(ComponentType component) const {
    // Placeholder area calculations (mm^2)
    switch (component) {
        case ComponentType::CORE:
            return 10.0 * config_.num_cores;
        case ComponentType::L1_CACHE:
            return 2.0 * config_.num_cores;
        case ComponentType::L2_CACHE:
            return 4.0 * config_.num_cores;
        case ComponentType::L3_CACHE:
            return config_.l3_size_bytes / (1024.0 * 1024.0);  // ~1 mm^2 per MB
        case ComponentType::MEMORY_CONTROLLER:
            return 5.0 * config_.num_memory_controllers;
        case ComponentType::NOC:
            return config_.has_noc ? (2.0 * config_.num_cores) : 0.0;
        default:
            return 0.0;
    }
}

double McPATWrapper::getTotalArea() const {
    double total = 0.0;
    total += getComponentArea(ComponentType::CORE);
    total += getComponentArea(ComponentType::L1_CACHE);
    total += getComponentArea(ComponentType::L2_CACHE);
    total += getComponentArea(ComponentType::L3_CACHE);
    total += getComponentArea(ComponentType::MEMORY_CONTROLLER);
    total += getComponentArea(ComponentType::NOC);
    return total;
}

double McPATWrapper::getPeakPower() const {
    // Peak power is typically 1.5-2x average power
    return system_power_.total_power * 1.8;
}

double McPATWrapper::getEnergyForPeriod(double time_seconds) const {
    return system_power_.total_power * time_seconds;  // Joules
}

bool McPATWrapper::isValid() const {
    return valid_;
}

std::string McPATWrapper::getErrorMessage() const {
    return error_message_;
}

void McPATWrapper::printDetailedResults() const {
    if (!power_computed_) {
        std::cout << "[McPATWrapper] Power not yet computed" << std::endl;
        return;
    }

    std::cout << "\n=== McPAT Power Analysis Results ===" << std::endl;
    std::cout << "Configuration:" << std::endl;
    std::cout << "  Cores: " << config_.num_cores << " @ " << config_.core_clock_mhz << " MHz" << std::endl;
    std::cout << "  Technology: " << config_.tech_node_nm << " nm" << std::endl;

    printComponentBreakdown();

    std::cout << "\nSystem Totals:" << std::endl;
    std::cout << "  Total Dynamic Power: " << system_power_.total_dynamic << " W" << std::endl;
    std::cout << "  Total Leakage Power: " << system_power_.total_leakage << " W" << std::endl;
    std::cout << "  Total Power: " << system_power_.total_power << " W" << std::endl;
    std::cout << "  Peak Power: " << getPeakPower() << " W" << std::endl;
    std::cout << "  Total Area: " << getTotalArea() << " mm^2" << std::endl;
    std::cout << "=====================================\n" << std::endl;
}

void McPATWrapper::printComponentBreakdown() const {
    std::cout << "\nComponent Power Breakdown:" << std::endl;

    auto print_component = [](const std::string& name, const PowerMetrics& power) {
        std::cout << "  " << name << ":" << std::endl;
        std::cout << "    Dynamic: " << power.runtime_dynamic << " W" << std::endl;
        std::cout << "    Leakage: " << power.total_leakage << " W" << std::endl;
        std::cout << "    Total: " << power.total_power << " W" << std::endl;
    };

    print_component("Cores", getComponentPower(ComponentType::CORE));
    print_component("L1 Caches", getComponentPower(ComponentType::L1_CACHE));
    print_component("L2 Caches", getComponentPower(ComponentType::L2_CACHE));
    print_component("L3 Cache", getComponentPower(ComponentType::L3_CACHE));
    print_component("Memory Controllers", getComponentPower(ComponentType::MEMORY_CONTROLLER));
    if (config_.has_noc) {
        print_component("NoC", getComponentPower(ComponentType::NOC));
    }
}

//=============================================================================
// XML Generation for McPAT
//=============================================================================

std::string McPATWrapper::generateXMLConfig() const {
    std::ostringstream xml;

    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml << "<component id=\"root\" name=\"root\">\n";

    // System parameters
    xml << "  <component id=\"system\" name=\"system\">\n";
    xml << "    <param name=\"number_of_cores\" value=\"" << config_.num_cores << "\"/>\n";
    xml << "    <param name=\"number_of_L1Directories\" value=\"0\"/>\n";
    xml << "    <param name=\"number_of_L2Directories\" value=\"0\"/>\n";
    xml << "    <param name=\"number_of_L2s\" value=\"" << config_.num_cores << "\"/>\n";
    xml << "    <param name=\"number_of_L3s\" value=\"1\"/>\n";
    xml << "    <param name=\"number_of_NoCs\" value=\"" << (config_.has_noc ? 1 : 0) << "\"/>\n";
    xml << "    <param name=\"homogeneous_cores\" value=\"1\"/>\n";
    xml << "    <param name=\"homogeneous_L2s\" value=\"1\"/>\n";
    xml << "    <param name=\"homogeneous_L1Directories\" value=\"1\"/>\n";
    xml << "    <param name=\"homogeneous_L2Directories\" value=\"1\"/>\n";
    xml << "    <param name=\"homogeneous_L3s\" value=\"1\"/>\n";
    xml << "    <param name=\"homogeneous_ccs\" value=\"1\"/>\n";
    xml << "    <param name=\"homogeneous_NoCs\" value=\"1\"/>\n";
    xml << "    <param name=\"core_tech_node\" value=\"" << config_.tech_node_nm << "\"/>\n";
    xml << "    <param name=\"target_core_clockrate\" value=\"" << static_cast<int>(config_.core_clock_mhz) << "\"/>\n";
    xml << "    <param name=\"temperature\" value=\"" << config_.temperature_k << "\"/>\n";
    xml << "    <param name=\"number_cache_levels\" value=\"3\"/>\n";
    xml << "    <param name=\"interconnect_projection_type\" value=\"0\"/>\n";
    xml << "    <param name=\"device_type\" value=\"0\"/>\n";
    xml << "    <param name=\"longer_channel_device\" value=\"1\"/>\n";
    xml << "    <param name=\"power_gating\" value=\"0\"/>\n";
    xml << "    <param name=\"machine_bits\" value=\"64\"/>\n";
    xml << "    <param name=\"virtual_address_width\" value=\"48\"/>\n";
    xml << "    <param name=\"physical_address_width\" value=\"48\"/>\n";
    xml << "    <param name=\"virtual_memory_page_size\" value=\"4096\"/>\n";

    // System statistics
    xml << "    <stat name=\"total_cycles\" value=\"" << total_cycles_ << "\"/>\n";
    xml << "    <stat name=\"idle_cycles\" value=\"" << (total_cycles_ - busy_cycles_) << "\"/>\n";
    xml << "    <stat name=\"busy_cycles\" value=\"" << busy_cycles_ << "\"/>\n";

    // Core component (for each core)
    for (int i = 0; i < config_.num_cores; i++) {
        xml << "    <component id=\"system.core" << i << "\" name=\"core" << i << "\">\n";
        xml << "      <param name=\"clock_rate\" value=\"" << static_cast<int>(config_.core_clock_mhz) << "\"/>\n";
        xml << "      <param name=\"vdd\" value=\"0\"/>\n";
        xml << "      <param name=\"power_gating_vcc\" value=\"-1\"/>\n";
        xml << "      <param name=\"opt_local\" value=\"0\"/>\n";
        xml << "      <param name=\"instruction_length\" value=\"32\"/>\n";
        xml << "      <param name=\"opcode_width\" value=\"7\"/>\n";
        xml << "      <param name=\"x86\" value=\"0\"/>\n";
        xml << "      <param name=\"micro_opcode_width\" value=\"8\"/>\n";
        xml << "      <param name=\"machine_type\" value=\"0\"/>\n";
        xml << "      <param name=\"number_hardware_threads\" value=\"1\"/>\n";
        xml << "      <param name=\"fetch_width\" value=\"" << config_.issue_width << "\"/>\n";
        xml << "      <param name=\"number_instruction_fetch_ports\" value=\"1\"/>\n";
        xml << "      <param name=\"decode_width\" value=\"" << config_.issue_width << "\"/>\n";
        xml << "      <param name=\"issue_width\" value=\"" << config_.issue_width << "\"/>\n";
        xml << "      <param name=\"peak_issue_width\" value=\"" << config_.issue_width << "\"/>\n";
        xml << "      <param name=\"commit_width\" value=\"" << config_.issue_width << "\"/>\n";
        xml << "      <param name=\"pipelines_per_core\" value=\"1,1\"/>\n";
        xml << "      <param name=\"pipeline_depth\" value=\"" << config_.pipeline_depth << ",14\"/>\n";
        xml << "      <param name=\"ALU_per_core\" value=\"3\"/>\n";
        xml << "      <param name=\"MUL_per_core\" value=\"1\"/>\n";
        xml << "      <param name=\"FPU_per_core\" value=\"1\"/>\n";
        xml << "      <param name=\"instruction_buffer_size\" value=\"32\"/>\n";
        xml << "      <param name=\"decoded_stream_buffer_size\" value=\"16\"/>\n";
        xml << "      <param name=\"instruction_window_scheme\" value=\"0\"/>\n";
        xml << "      <param name=\"instruction_window_size\" value=\"64\"/>\n";
        xml << "      <param name=\"fp_instruction_window_size\" value=\"32\"/>\n";
        xml << "      <param name=\"ROB_size\" value=\"128\"/>\n";
        xml << "      <param name=\"archi_Regs_IRF_size\" value=\"32\"/>\n";
        xml << "      <param name=\"archi_Regs_FRF_size\" value=\"32\"/>\n";
        xml << "      <param name=\"phy_Regs_IRF_size\" value=\"256\"/>\n";
        xml << "      <param name=\"phy_Regs_FRF_size\" value=\"256\"/>\n";
        xml << "      <param name=\"rename_scheme\" value=\"0\"/>\n";
        xml << "      <param name=\"register_windows_size\" value=\"0\"/>\n";
        xml << "      <param name=\"LSU_order\" value=\"inorder\"/>\n";
        xml << "      <param name=\"store_buffer_size\" value=\"32\"/>\n";
        xml << "      <param name=\"load_buffer_size\" value=\"32\"/>\n";
        xml << "      <param name=\"memory_ports\" value=\"1\"/>\n";
        xml << "      <param name=\"RAS_size\" value=\"32\"/>\n";

        // Core statistics
        uint64_t inst_per_core = total_instructions_ / config_.num_cores;
        xml << "      <stat name=\"total_instructions\" value=\"" << inst_per_core << "\"/>\n";
        xml << "      <stat name=\"int_instructions\" value=\"" << (inst_per_core * 70 / 100) << "\"/>\n";
        xml << "      <stat name=\"fp_instructions\" value=\"" << (inst_per_core * 10 / 100) << "\"/>\n";
        xml << "      <stat name=\"branch_instructions\" value=\"" << (inst_per_core * 10 / 100) << "\"/>\n";
        xml << "      <stat name=\"branch_mispredictions\" value=\"" << (inst_per_core * 1 / 100) << "\"/>\n";
        xml << "      <stat name=\"load_instructions\" value=\"" << (inst_per_core * 20 / 100) << "\"/>\n";
        xml << "      <stat name=\"store_instructions\" value=\"" << (inst_per_core * 10 / 100) << "\"/>\n";
        xml << "      <stat name=\"committed_instructions\" value=\"" << inst_per_core << "\"/>\n";
        xml << "      <stat name=\"committed_int_instructions\" value=\"" << (inst_per_core * 70 / 100) << "\"/>\n";
        xml << "      <stat name=\"committed_fp_instructions\" value=\"" << (inst_per_core * 10 / 100) << "\"/>\n";
        xml << "      <stat name=\"pipeline_duty_cycle\" value=\"" << (busy_cycles_ > 0 ? 0.8 : 0.0) << "\"/>\n";
        xml << "      <stat name=\"total_cycles\" value=\"" << total_cycles_ << "\"/>\n";
        xml << "      <stat name=\"idle_cycles\" value=\"" << (total_cycles_ - busy_cycles_) << "\"/>\n";
        xml << "      <stat name=\"busy_cycles\" value=\"" << busy_cycles_ << "\"/>\n";
        xml << "      <stat name=\"ROB_reads\" value=\"" << inst_per_core << "\"/>\n";
        xml << "      <stat name=\"ROB_writes\" value=\"" << inst_per_core << "\"/>\n";
        xml << "      <stat name=\"rename_reads\" value=\"" << inst_per_core << "\"/>\n";
        xml << "      <stat name=\"rename_writes\" value=\"" << inst_per_core << "\"/>\n";
        xml << "      <stat name=\"fp_rename_reads\" value=\"" << (inst_per_core * 10 / 100) << "\"/>\n";
        xml << "      <stat name=\"fp_rename_writes\" value=\"" << (inst_per_core * 10 / 100) << "\"/>\n";
        xml << "      <stat name=\"inst_window_reads\" value=\"" << inst_per_core << "\"/>\n";
        xml << "      <stat name=\"inst_window_writes\" value=\"" << inst_per_core << "\"/>\n";
        xml << "      <stat name=\"inst_window_wakeup_accesses\" value=\"" << inst_per_core << "\"/>\n";
        xml << "      <stat name=\"fp_inst_window_reads\" value=\"" << (inst_per_core * 10 / 100) << "\"/>\n";
        xml << "      <stat name=\"fp_inst_window_writes\" value=\"" << (inst_per_core * 10 / 100) << "\"/>\n";
        xml << "      <stat name=\"fp_inst_window_wakeup_accesses\" value=\"" << (inst_per_core * 10 / 100) << "\"/>\n";
        xml << "      <stat name=\"int_regfile_reads\" value=\"" << (inst_per_core * 2) << "\"/>\n";
        xml << "      <stat name=\"int_regfile_writes\" value=\"" << inst_per_core << "\"/>\n";
        xml << "      <stat name=\"float_regfile_reads\" value=\"" << (inst_per_core * 20 / 100) << "\"/>\n";
        xml << "      <stat name=\"float_regfile_writes\" value=\"" << (inst_per_core * 10 / 100) << "\"/>\n";
        xml << "      <stat name=\"function_calls\" value=\"" << (inst_per_core / 100) << "\"/>\n";
        xml << "      <stat name=\"context_switches\" value=\"0\"/>\n";
        xml << "      <stat name=\"ialu_accesses\" value=\"" << (inst_per_core * 70 / 100) << "\"/>\n";
        xml << "      <stat name=\"fpu_accesses\" value=\"" << (inst_per_core * 10 / 100) << "\"/>\n";
        xml << "      <stat name=\"mul_accesses\" value=\"" << (inst_per_core * 5 / 100) << "\"/>\n";
        xml << "      <stat name=\"cdb_alu_accesses\" value=\"" << inst_per_core << "\"/>\n";
        xml << "      <stat name=\"cdb_mul_accesses\" value=\"" << (inst_per_core * 5 / 100) << "\"/>\n";
        xml << "      <stat name=\"cdb_fpu_accesses\" value=\"" << (inst_per_core * 10 / 100) << "\"/>\n";
        xml << "    </component>\n";

        // L1 caches for this core
        xml << "    <component id=\"system.core" << i << ".icache\" name=\"icache\">\n";
        xml << "      <param name=\"icache_config\" value=\"" << (config_.l1i_size_bytes/1024)
            << ",64,8,1,1,3,64,0\"/>\n";
        xml << "      <param name=\"buffer_sizes\" value=\"16,16,16,0\"/>\n";
        xml << "      <stat name=\"read_accesses\" value=\"" << l1_reads_ / config_.num_cores << "\"/>\n";
        xml << "      <stat name=\"read_misses\" value=\"" << (l1_reads_ / config_.num_cores / 10) << "\"/>\n";
        xml << "      <stat name=\"conflicts\" value=\"0\"/>\n";
        xml << "    </component>\n";

        xml << "    <component id=\"system.core" << i << ".dcache\" name=\"dcache\">\n";
        xml << "      <param name=\"dcache_config\" value=\"" << (config_.l1d_size_bytes/1024)
            << ",64,8,1,1,3,64,0\"/>\n";
        xml << "      <param name=\"buffer_sizes\" value=\"16,16,16,16\"/>\n";
        xml << "      <stat name=\"read_accesses\" value=\"" << l1_reads_ / config_.num_cores << "\"/>\n";
        xml << "      <stat name=\"write_accesses\" value=\"" << l1_writes_ / config_.num_cores << "\"/>\n";
        xml << "      <stat name=\"read_misses\" value=\"" << (l1_reads_ / config_.num_cores / 10) << "\"/>\n";
        xml << "      <stat name=\"write_misses\" value=\"" << (l1_writes_ / config_.num_cores / 10) << "\"/>\n";
        xml << "      <stat name=\"conflicts\" value=\"0\"/>\n";
        xml << "    </component>\n";

        // L2 cache for this core
        xml << "    <component id=\"system.L2" << i << "\" name=\"L2\">\n";
        xml << "      <param name=\"L2_config\" value=\"" << (config_.l2_size_bytes/1024)
            << ",64,8,8,8,23,64,1\"/>\n";
        xml << "      <param name=\"buffer_sizes\" value=\"16,16,16,16\"/>\n";
        xml << "      <param name=\"clockrate\" value=\"" << static_cast<int>(config_.core_clock_mhz) << "\"/>\n";
        xml << "      <param name=\"ports\" value=\"1,1,1\"/>\n";
        xml << "      <param name=\"device_type\" value=\"0\"/>\n";
        xml << "      <stat name=\"read_accesses\" value=\"" << l2_reads_ / config_.num_cores << "\"/>\n";
        xml << "      <stat name=\"write_accesses\" value=\"" << l2_writes_ / config_.num_cores << "\"/>\n";
        xml << "      <stat name=\"read_misses\" value=\"" << (l2_reads_ / config_.num_cores / 10) << "\"/>\n";
        xml << "      <stat name=\"write_misses\" value=\"" << (l2_writes_ / config_.num_cores / 10) << "\"/>\n";
        xml << "      <stat name=\"conflicts\" value=\"0\"/>\n";
        xml << "      <stat name=\"duty_cycle\" value=\"" << (busy_cycles_ > 0 ? 0.5 : 0.0) << "\"/>\n";
        xml << "    </component>\n";
    }

    // L3 cache (shared)
    xml << "    <component id=\"system.L3\" name=\"L3\">\n";
    xml << "      <param name=\"L3_config\" value=\"" << (config_.l3_size_bytes/(1024*1024))
        << ",64,16,16,16,23,64,1\"/>\n";
    xml << "      <param name=\"clockrate\" value=\"" << static_cast<int>(config_.core_clock_mhz) << "\"/>\n";
    xml << "      <param name=\"ports\" value=\"1,1,1\"/>\n";
    xml << "      <param name=\"device_type\" value=\"0\"/>\n";
    xml << "      <param name=\"buffer_sizes\" value=\"16,16,16,16\"/>\n";
    xml << "      <stat name=\"read_accesses\" value=\"" << l3_reads_ << "\"/>\n";
    xml << "      <stat name=\"write_accesses\" value=\"" << l3_writes_ << "\"/>\n";
    xml << "      <stat name=\"read_misses\" value=\"" << (l3_reads_ / 10) << "\"/>\n";
    xml << "      <stat name=\"write_misses\" value=\"" << (l3_writes_ / 10) << "\"/>\n";
    xml << "      <stat name=\"conflicts\" value=\"0\"/>\n";
    xml << "      <stat name=\"duty_cycle\" value=\"" << (busy_cycles_ > 0 ? 0.3 : 0.0) << "\"/>\n";
    xml << "    </component>\n";

    // Memory controller
    for (int i = 0; i < config_.num_memory_controllers; i++) {
        xml << "    <component id=\"system.mc\" name=\"mc\">\n";
        xml << "      <param name=\"type\" value=\"0\"/>\n";
        xml << "      <param name=\"mc_clock\" value=\"" << static_cast<int>(config_.mc_clock_mhz) << "\"/>\n";
        xml << "      <param name=\"vdd\" value=\"0\"/>\n";
        xml << "      <param name=\"power_gating_vcc\" value=\"-1\"/>\n";
        xml << "      <param name=\"peak_transfer_rate\" value=\"6400\"/>\n";
        xml << "      <param name=\"block_size\" value=\"64\"/>\n";
        xml << "      <param name=\"number_mcs\" value=\"" << config_.num_memory_controllers << "\"/>\n";
        xml << "      <param name=\"memory_channels_per_mc\" value=\"1\"/>\n";
        xml << "      <param name=\"number_ranks\" value=\"2\"/>\n";
        xml << "      <param name=\"withPHY\" value=\"0\"/>\n";
        xml << "      <param name=\"req_window_size_per_channel\" value=\"32\"/>\n";
        xml << "      <param name=\"IO_buffer_size_per_channel\" value=\"32\"/>\n";
        xml << "      <param name=\"databus_width\" value=\"128\"/>\n";
        xml << "      <param name=\"addressbus_width\" value=\"51\"/>\n";
        xml << "      <stat name=\"memory_accesses\" value=\"" << (l3_reads_ + l3_writes_) / config_.num_memory_controllers << "\"/>\n";
        xml << "      <stat name=\"memory_reads\" value=\"" << l3_reads_ / config_.num_memory_controllers << "\"/>\n";
        xml << "      <stat name=\"memory_writes\" value=\"" << l3_writes_ / config_.num_memory_controllers << "\"/>\n";
        xml << "    </component>\n";
    }

    // NoC (Network-on-Chip)
    if (config_.has_noc) {
        std::string topology = (config_.noc_topology == 0) ? "0" :
                             (config_.noc_topology == 1) ? "1" : "2";
        xml << "    <component id=\"system.noc0\" name=\"noc0\">\n";
        xml << "      <param name=\"clockrate\" value=\"" << static_cast<int>(config_.core_clock_mhz) << "\"/>\n";
        xml << "      <param name=\"vdd\" value=\"0\"/>\n";
        xml << "      <param name=\"power_gating_vcc\" value=\"-1\"/>\n";
        xml << "      <param name=\"type\" value=\"" << topology << "\"/>\n";
        xml << "      <param name=\"horizontal_nodes\" value=\"" << static_cast<int>(std::sqrt(config_.num_cores)) << "\"/>\n";
        xml << "      <param name=\"vertical_nodes\" value=\"" << static_cast<int>(std::sqrt(config_.num_cores)) << "\"/>\n";
        xml << "      <param name=\"has_global_link\" value=\"0\"/>\n";
        xml << "      <param name=\"link_throughput\" value=\"1\"/>\n";
        xml << "      <param name=\"link_latency\" value=\"1\"/>\n";
        xml << "      <param name=\"input_ports\" value=\"5\"/>\n";
        xml << "      <param name=\"output_ports\" value=\"5\"/>\n";
        xml << "      <param name=\"flit_bits\" value=\"128\"/>\n";
        xml << "      <param name=\"chip_coverage\" value=\"1\"/>\n";
        xml << "      <param name=\"link_routing_over_percentage\" value=\"0.5\"/>\n";
        xml << "      <stat name=\"total_accesses\" value=\"" << (l3_reads_ + l3_writes_) << "\"/>\n";
        xml << "      <stat name=\"duty_cycle\" value=\"" << (busy_cycles_ > 0 ? 0.5 : 0.0) << "\"/>\n";
        xml << "    </component>\n";
    }

    xml << "  </component>\n";
    xml << "</component>\n";

    return xml.str();
}

} // namespace pimid

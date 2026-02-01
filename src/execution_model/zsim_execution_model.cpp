#include "execution_model/zsim_execution_model.h"
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <limits.h>
#include <libgen.h>

namespace pimid {

// Helper to get PIMID root directory (from executable path or environment)
static std::string getPimidRoot() {
    // Check environment variable first
    const char* env_root = getenv("PIMID_ROOT");
    if (env_root && access(env_root, F_OK) == 0) {
        return std::string(env_root);
    }

    // Get executable path and derive root
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len != -1) {
        exe_path[len] = '\0';
        std::string path(dirname(exe_path));
        size_t build_pos = path.rfind("/build");
        if (build_pos != std::string::npos) {
            return path.substr(0, build_pos);
        }
        return path;
    }
    return ".";
}

// -------------------------------------------------------------------------
// ZSim Process Launcher - Handles launching ZSim as a subprocess
// -------------------------------------------------------------------------

class ZSimLauncher {
public:
    ZSimLauncher() : zsim_pid_(-1), running_(false) {}

    ~ZSimLauncher() {
        if (running_) {
            terminate();
        }
    }

    bool launch(const std::string& config_file, const std::string& output_dir) {
        // Find ZSim components
        std::string zsim_path = findZSimPath();
        std::string pin_path = findPinPath();

        if (zsim_path.empty()) {
            std::cerr << "ZSim not found. Build it first with: cd pimid/external/zsim && scons -j$(nproc)" << std::endl;
            return false;
        }

        if (pin_path.empty()) {
            std::cerr << "PIN not found. Set PIN_HOME environment variable." << std::endl;
            return false;
        }

        // Create output directory
        mkdir(output_dir.c_str(), 0755);

        // Fork and exec ZSim
        zsim_pid_ = fork();

        if (zsim_pid_ < 0) {
            std::cerr << "Failed to fork ZSim process" << std::endl;
            return false;
        }

        if (zsim_pid_ == 0) {
            // Child process - exec ZSim harness
            std::string harness_path = zsim_path + "/build/opt/zsim";

            // Check if harness exists
            struct stat st;
            if (stat(harness_path.c_str(), &st) != 0) {
                // Try release build
                harness_path = zsim_path + "/build/release/zsim";
                if (stat(harness_path.c_str(), &st) != 0) {
                    std::cerr << "ZSim harness not found at " << harness_path << std::endl;
                    std::cerr << "Build ZSim first: cd " << zsim_path << " && scons -j$(nproc)" << std::endl;
                    _exit(1);
                }
            }

            // Set up environment
            setenv("ZSIMPATH", zsim_path.c_str(), 1);
            setenv("PIN_HOME", pin_path.c_str(), 1);

            // Redirect stdout/stderr to files
            std::string stdout_file = output_dir + "/zsim.stdout";
            std::string stderr_file = output_dir + "/zsim.stderr";

            int stdout_fd = open(stdout_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            int stderr_fd = open(stderr_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);

            if (stdout_fd >= 0) dup2(stdout_fd, STDOUT_FILENO);
            if (stderr_fd >= 0) dup2(stderr_fd, STDERR_FILENO);

            // Execute ZSim harness
            execl(harness_path.c_str(), "zsim", config_file.c_str(), nullptr);

            // If exec fails
            std::cerr << "Failed to exec ZSim harness: " << strerror(errno) << std::endl;
            _exit(1);
        }

        // Parent process
        running_ = true;
        std::cout << "ZSim launched with PID " << zsim_pid_ << std::endl;
        return true;
    }

    bool isRunning() const {
        if (!running_ || zsim_pid_ < 0) return false;

        int status;
        pid_t result = waitpid(zsim_pid_, &status, WNOHANG);

        if (result == 0) {
            return true;  // Still running
        } else if (result == zsim_pid_) {
            return false;  // Terminated
        }
        return false;
    }

    int waitForCompletion() {
        if (!running_ || zsim_pid_ < 0) return -1;

        int status;
        waitpid(zsim_pid_, &status, 0);
        running_ = false;

        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
        return -1;
    }

    void terminate() {
        if (running_ && zsim_pid_ > 0) {
            kill(zsim_pid_, SIGTERM);
            usleep(100000);  // 100ms

            if (isRunning()) {
                kill(zsim_pid_, SIGKILL);
            }

            int status;
            waitpid(zsim_pid_, &status, 0);
            running_ = false;
        }
    }

    pid_t getPid() const { return zsim_pid_; }

private:
    std::string findZSimPath() {
        // Check environment variable first
        const char* zsim_env = getenv("ZSIM_PATH");
        if (zsim_env) {
            struct stat st;
            std::string scons = std::string(zsim_env) + "/SConstruct";
            if (stat(scons.c_str(), &st) == 0) {
                return zsim_env;
            }
        }

        // Check PIMID's external/zsim
        std::string pimid_root = getPimidRoot();
        std::string zsim_path = pimid_root + "/external/zsim";
        struct stat st;
        std::string scons = zsim_path + "/SConstruct";
        if (stat(scons.c_str(), &st) == 0) {
            return zsim_path;
        }

        return "";
    }

    std::string findPinPath() {
        // Check environment first (PIN_HOME or PINPATH)
        const char* pin_home = getenv("PIN_HOME");
        if (pin_home) return pin_home;
        const char* pin_path = getenv("PINPATH");
        if (pin_path) return pin_path;

        // Check PIMID's external/pin (symlink or directory)
        std::string pimid_root = getPimidRoot();
        std::string pimid_pin = pimid_root + "/external/pin";
        struct stat st;
        if (stat((pimid_pin + "/pin").c_str(), &st) == 0) {
            return pimid_pin;
        }

        // Check common system locations
        std::vector<std::string> paths = {
            "/opt/pin",
            "/usr/local/pin"
        };

        for (const auto& path : paths) {
            std::string pin_exe = path + "/pin";
            if (stat(pin_exe.c_str(), &st) == 0) {
                return path;
            }
        }
        return "";
    }

    pid_t zsim_pid_;
    bool running_;
};

// -------------------------------------------------------------------------
// ZSimExecutionModel Implementation
// -------------------------------------------------------------------------

ZSimExecutionModel::ZSimExecutionModel()
    : zsim_glob_info_(nullptr)
    , cores_(nullptr)
    , num_cores_(0)
    , domain_(SimulationDomain::HOST)
    , pim_mode_(false)
    , current_cycle_(0)
    , initialized_(false)
    , idle_(true)
    , zsim_launcher_(std::make_unique<ZSimLauncher>())
{
}

ZSimExecutionModel::~ZSimExecutionModel() {
    if (initialized_) {
        finalize();
    }
}

bool ZSimExecutionModel::initialize(const std::string& config_file,
                                    SimulationDomain domain) {
    if (initialized_) {
        std::cerr << "ZSimExecutionModel already initialized" << std::endl;
        return false;
    }

    config_file_ = config_file;
    domain_ = domain;

    try {
        std::cout << "Initializing ZSim for "
                  << (domain == SimulationDomain::HOST ? "HOST" : "DEVICE")
                  << " domain..." << std::endl;

        // Validate config file exists
        struct stat st;
        if (stat(config_file.c_str(), &st) != 0) {
            std::cerr << "ZSim config file not found: " << config_file << std::endl;
            return false;
        }

        // Parse config to get core count and other parameters
        if (!parseZSimConfig(config_file)) {
            std::cerr << "Failed to parse ZSim config" << std::endl;
            // Continue with defaults
        }

        // Set up output directory
        output_dir_ = "./zsim_output_" + std::to_string(getpid());

        // Register memory model integration
        setupMemoryInterception();

        initialized_ = true;
        std::cout << "ZSim execution model initialized with " << num_cores_ << " cores" << std::endl;
        std::cout << "Output directory: " << output_dir_ << std::endl;

        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to initialize ZSim: " << e.what() << std::endl;
        return false;
    }
}

void ZSimExecutionModel::finalize() {
    if (!initialized_) {
        return;
    }

    std::cout << "Finalizing ZSim execution model..." << std::endl;

    // Terminate ZSim if running
    if (zsim_launcher_) {
        zsim_launcher_->terminate();
    }

    // Parse final statistics from output
    parseZSimOutput();

    zsim_glob_info_ = nullptr;
    cores_ = nullptr;
    num_cores_ = 0;
    initialized_ = false;
}

bool ZSimExecutionModel::launchSimulation(const std::string& binary_path,
                                          const std::vector<std::string>& args) {
    if (!initialized_) {
        std::cerr << "ZSim not initialized" << std::endl;
        return false;
    }

    // Generate ZSim config with the binary
    std::string zsim_config = generateZSimConfig(binary_path, args);

    // Write config to file
    std::string config_path = output_dir_ + "/zsim.cfg";
    std::ofstream config_out(config_path);
    if (!config_out) {
        std::cerr << "Failed to write ZSim config to " << config_path << std::endl;
        return false;
    }
    config_out << zsim_config;
    config_out.close();

    // Launch ZSim
    if (!zsim_launcher_->launch(config_path, output_dir_)) {
        std::cerr << "Failed to launch ZSim" << std::endl;
        return false;
    }

    std::cout << "ZSim simulation started for: " << binary_path << std::endl;
    return true;
}

void ZSimExecutionModel::advanceCycles(Cycle num_cycles) {
    if (!initialized_) {
        throw std::runtime_error("ZSim not initialized");
    }

    Cycle target_cycle = current_cycle_ + num_cycles;

    // If ZSim is running as subprocess, we track cycles via output parsing
    // For now, advance analytically
    current_cycle_ = target_cycle;
    stats_.total_cycles = current_cycle_;

    idle_ = isIdle();
}

Cycle ZSimExecutionModel::executeTask(const Task& task) {
    if (!initialized_) {
        throw std::runtime_error("ZSim not initialized");
    }

    // For subprocess mode, tasks are executed by ZSim internally
    // We estimate cycles based on task parameters
    Cycle estimated_cycles = task.estimated_cycles;
    if (estimated_cycles == 0) {
        // Estimate based on operations and memory accesses
        // Assume 1 cycle per op + memory latency
        estimated_cycles = task.num_ops;

        // Add memory latency estimate
        if (memory_model_) {
            MemoryRequest req{task.input_addresses.empty() ? 0 : task.input_addresses[0],
                             64, MemoryRequestType::READ};
            estimated_cycles += memory_model_->getLatency(MemoryRequestType::READ);
        }
    }

    Cycle completion_cycle = current_cycle_ + estimated_cycles;

    // Update statistics
    stats_.total_tasks++;
    stats_.total_instructions += task.num_ops;

    if (task_complete_callback_) {
        task_complete_callback_(task, completion_cycle);
    }

    return completion_cycle;
}

bool ZSimExecutionModel::isIdle() const {
    if (!initialized_) return true;

    // Check if ZSim subprocess is still running
    if (zsim_launcher_ && zsim_launcher_->isRunning()) {
        return false;
    }

    return idle_.load();
}

void ZSimExecutionModel::registerMemoryModel(std::shared_ptr<MemoryModel> memory_model) {
    memory_model_ = memory_model;
    std::cout << "Registered memory model with ZSim execution model" << std::endl;
}

std::vector<MemoryAccess> ZSimExecutionModel::getMemoryAccessPattern(const Task& task) const {
    std::vector<MemoryAccess> accesses;

    // Generate memory accesses for inputs
    for (size_t i = 0; i < task.input_addresses.size(); i++) {
        uint64_t addr = task.input_addresses[i];
        uint64_t size = task.input_size / std::max(task.input_addresses.size(), size_t(1));

        accesses.push_back({
            addr, size, true, current_cycle_, task.pe_id,
            MemoryAccess::Pattern::SEQUENTIAL, 0
        });
    }

    // Generate memory accesses for outputs
    for (size_t i = 0; i < task.output_addresses.size(); i++) {
        uint64_t addr = task.output_addresses[i];
        uint64_t size = task.output_size / std::max(task.output_addresses.size(), size_t(1));

        accesses.push_back({
            addr, size, false, current_cycle_, task.pe_id,
            MemoryAccess::Pattern::SEQUENTIAL, 0
        });
    }

    return accesses;
}

ExecutionStats ZSimExecutionModel::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    ExecutionStats stats = stats_;
    stats.total_cycles = current_cycle_;

    if (stats.total_cycles > 0) {
        stats.ipc = static_cast<double>(stats.total_instructions) / stats.total_cycles;
    }

    return stats;
}

void ZSimExecutionModel::resetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = ExecutionStats{};
}

Cycle ZSimExecutionModel::getCurrentCycle() const {
    return current_cycle_.load();
}

void ZSimExecutionModel::registerTaskCompleteCallback(
    std::function<void(const Task&, Cycle)> callback) {
    task_complete_callback_ = callback;
}

void ZSimExecutionModel::registerMemoryCallback(
    std::function<void(const MemoryAccess&, Cycle)> callback) {
    memory_callback_ = callback;
}

Core* ZSimExecutionModel::getCore(uint32_t core_id) const {
    // In subprocess mode, we don't have direct access to ZSim cores
    return nullptr;
}

void ZSimExecutionModel::injectMemoryResponse(uint64_t address, Cycle latency) {
    if (memory_callback_) {
        MemoryAccess access{
            address, 64, true, current_cycle_, 0,
            MemoryAccess::Pattern::SEQUENTIAL, 0
        };
        memory_callback_(access, latency);
    }
}

void ZSimExecutionModel::configurePIMMode(bool enable_pim, uint32_t num_pim_cores) {
    pim_mode_ = enable_pim;
    if (enable_pim) {
        std::cout << "Configured ZSim for PIM mode with "
                  << num_pim_cores << " PIM cores" << std::endl;
    }
}

int ZSimExecutionModel::waitForCompletion() {
    if (!zsim_launcher_) {
        return -1;
    }

    int exit_code = zsim_launcher_->waitForCompletion();

    // Parse output statistics after completion
    parseZSimOutput();

    return exit_code;
}

bool ZSimExecutionModel::isSimulationRunning() const {
    return zsim_launcher_ && zsim_launcher_->isRunning();
}

// -------------------------------------------------------------------------
// Private Methods
// -------------------------------------------------------------------------

bool ZSimExecutionModel::parseZSimConfig(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file) return false;

    std::string line;
    while (std::getline(file, line)) {
        // Simple parsing for key = value format
        // Look for core count
        if (line.find("sys.numCores") != std::string::npos ||
            line.find("numCores") != std::string::npos) {
            size_t eq = line.find('=');
            if (eq != std::string::npos) {
                std::string value = line.substr(eq + 1);
                // Remove whitespace and semicolons
                value.erase(std::remove_if(value.begin(), value.end(),
                    [](char c) { return std::isspace(c) || c == ';'; }), value.end());
                num_cores_ = std::stoul(value);
            }
        }
    }

    if (num_cores_ == 0) {
        num_cores_ = (domain_ == SimulationDomain::HOST) ? 4 : 256;
    }

    return true;
}

std::string ZSimExecutionModel::generateZSimConfig(const std::string& binary_path,
                                                    const std::vector<std::string>& args) {
    std::ostringstream cfg;

    cfg << "// Auto-generated ZSim configuration\n";
    cfg << "sim = {\n";
    cfg << "    phaseLength = 10000;\n";
    cfg << "    maxTotalInstrs = 100000000;\n";
    cfg << "};\n\n";

    cfg << "sys = {\n";
    cfg << "    numCores = " << num_cores_ << ";\n";
    cfg << "    lineSize = 64;\n";
    cfg << "    frequency = 2000;\n";
    cfg << "};\n\n";

    // Core configuration
    cfg << "sys.coreType = \"" << (pim_mode_ ? "Simple" : "OOO") << "\";\n\n";

    // Process configuration
    cfg << "process0 = {\n";
    cfg << "    command = \"" << binary_path;
    for (const auto& arg : args) {
        cfg << " " << arg;
    }
    cfg << "\";\n";
    cfg << "};\n";

    return cfg.str();
}

void ZSimExecutionModel::setupMemoryInterception() {
    std::cout << "Memory interception configured for Ramulator integration" << std::endl;
    // In subprocess mode, memory interception happens within ZSim
    // Results are parsed from output files
}

void ZSimExecutionModel::parseZSimOutput() {
    // Parse zsim.out for statistics
    std::string stats_file = output_dir_ + "/zsim.out";
    std::ifstream file(stats_file);

    if (!file) {
        std::cout << "ZSim output not found at " << stats_file << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Parse relevant statistics
        if (line.find("cycles") != std::string::npos) {
            // Extract cycle count
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string value = line.substr(colon + 1);
                try {
                    current_cycle_ = std::stoull(value);
                    stats_.total_cycles = current_cycle_;
                } catch (...) {}
            }
        }
        if (line.find("instrs") != std::string::npos) {
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string value = line.substr(colon + 1);
                try {
                    stats_.total_instructions = std::stoull(value);
                } catch (...) {}
            }
        }
    }

    std::cout << "Parsed ZSim output: " << stats_.total_cycles << " cycles, "
              << stats_.total_instructions << " instructions" << std::endl;
}

// Static callbacks (used if ZSim is linked directly - future enhancement)
void ZSimExecutionModel::zsimMemoryRequestCallback(void* ctx, uint64_t address,
                                                   uint64_t size, bool is_write) {
    auto* model = static_cast<ZSimExecutionModel*>(ctx);

    if (model->memory_model_) {
        MemoryRequest req{
            address, size,
            is_write ? MemoryRequestType::WRITE : MemoryRequestType::READ,
            model->current_cycle_, 0
        };
        // Async memory access
        // Cycle latency = model->memory_model_->access(req);
        // model->injectMemoryResponse(address, latency);
    }

    model->stats_.memory_accesses++;
}

void ZSimExecutionModel::zsimCycleCallback(void* ctx, uint64_t cycle) {
    auto* model = static_cast<ZSimExecutionModel*>(ctx);
    model->current_cycle_ = cycle;
}

} // namespace pimid

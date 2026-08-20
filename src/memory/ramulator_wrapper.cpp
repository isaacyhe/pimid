#include "power/cacti_io_wrapper.h"   // 1.11.40 (N8): harnessed IO model
#include <iostream>
#include "memory/ramulator_wrapper.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <yaml-cpp/yaml.h>

// Include Ramulator headers
#include "base/base.h"
#include "base/request.h"
#include "memory_system/memory_system.h"
#include "base/factory.h"
#include "dram/pimid_energy.h"   // 1.9.10: Ramulator2-resident intensive energy layer

namespace pimid {

RamulatorWrapper::RamulatorWrapper(const std::string& config_path, const std::string& dram_type)
    : config_path_(config_path),
      capacity_(0),
      bandwidth_(0),
      channels_(1),
      ranks_per_channel_(1),
      banks_per_rank_(8),
      total_reads_(0),
      total_writes_(0),
      cached_read_energy_(0.0),
      cached_write_energy_(0.0),
      cached_leakage_power_(0.0),
      last_energy_update_(0),
      current_cycle_(0),
      pim_enabled_(false),
      dram_type_(dram_type),
      dram_arch_(nullptr),
      bandwidth_tracker_(nullptr),
      internal_network_(nullptr),
      pim_plugin_(nullptr) {
}

RamulatorWrapper::~RamulatorWrapper() {
    if (ramulator_memory_system_) {
        ramulator_memory_system_->finalize();
    }
}

void RamulatorWrapper::initialize() {
    parseConfiguration();

    // Only create a full Ramulator2 instance when a config file is provided
    // (for cycle-accurate simulation). Default configs (empty path) are used
    // as parameter oracles -- timing/power from DRAMArchitectureV2 suffices.
    if (!config_path_.empty()) {
        createRamulatorInstance();
    }

    // Auto-populate DRAM architecture for timing/power queries.
    // This ensures getTRCD(), getTCAS(), energy methods etc. return
    // calibrated values even without calling enablePIMSupport().
    /* 1.11.57 (audit C004): the SUBSTITUTION SAYS SO.
     *
     * Only DDR4, DDR5, HBM2 and HBM3 have an architecture object. DDR3,
     * LPDDR5 and GDDR6 fall into the else and are handed DDR4-2400's -- a
     * different part, a different speed bin, a different channel width -- and
     * the else said nothing. Every caller then read timings, widths and
     * bandwidths off an object describing DDR4 while believing it described
     * the technology it asked for; main.cpp's ladder printed exactly that, as
     * "the ladder from the GDDR6 architecture object".
     *
     * The adoption site now gates on the reconciliation check, so the false
     * ladder no longer reaches the model. This is the other half: the wrapper
     * records WHICH part the object it holds actually describes, exposes it
     * (getArchitectureTechnology()), and announces the substitution once.
     * Fabricating architecture objects for the three missing technologies
     * would be the dishonest repair; saying which part is being read is not. */
    if (!dram_arch_) {
        // 1.11.59 (audit C018): a fresh object carries its factory
        // organization, so no device width is stamped on it yet.
        arch_device_width_bits_ = 0;
        std::string dt = dram_type_;
        std::transform(dt.begin(), dt.end(), dt.begin(), ::toupper);
        if (dt == "DDR5") {
            dram_arch_ = pimid::memory::createDDR5_4800_Verified();
        } else if (dt == "HBM2") {
            dram_arch_ = pimid::memory::createHBM2_Verified();
        } else if (dt == "HBM3") {
            dram_arch_ = pimid::memory::createHBM3_Verified();
        } else {
            dram_arch_ = pimid::memory::createDDR4_2400_Verified();
            if (!dt.empty() && dt != "DDR4") {
                static bool announced_substitution = false;
                if (!announced_substitution) {
                    announced_substitution = true;
                    std::cerr << "[mem] NOTE: there is no " << dt
                              << " architecture object in this tree, so the "
                                 "DDR4-2400 one is being read in its place. "
                                 "Every width, internal bandwidth and derived "
                                 "hierarchy figure this wrapper reports for "
                              << dt << " describes DDR4-2400, not " << dt
                              << ". getArchitectureTechnology() reports the "
                                 "part actually being read." << std::endl;
                }
            }
        }
    }

    /* 1.11.59 (audit C018): stamp the configured JEDEC device width onto the
     * object BEFORE anything reads a width, a bandwidth or the reconciliation
     * check below off it. No-op when no width is configured. */
    applyDeviceWidthToArchitecture();

    /* 1.11.57 (audit round 3, C002): this check RUNS now.
     *
     * It was written in 1.11.56 to catch exactly the drift that release
     * fixed, and it sat inside parseConfiguration() behind `if (dram_arch_)`
     * -- but initialize() calls parseConfiguration() first and only
     * populates dram_arch_ afterwards, so the pointer was always null and
     * the guard always false. It never executed once. Gate 1166D's K11 arm
     * reported "mismatch warnings=0" and that was a vacuous pass: zero
     * warnings because nothing ran, not because nothing was wrong. Moved
     * below the population, where it would have caught C001 (the CACTI-IO
     * rate table still at DDR5-4800 and HBM2-2000) on its first run. */
    /* 1.11.56: ONE PART, ONE SPEED BIN -- checked, not trusted.
     *
     * The Ramulator preset above decides the cycles this simulator
     * counts. The DRAM architecture object beside it decides every
     * bandwidth the simulator REPORTS -- chip I/O, rank, channel, and
     * since 1.11.56 the whole per-level hierarchy link ladder. Nothing
     * held the two together, and three of the four had drifted: HBM2
     * 2000 vs the 2400 preset, DDR5 4800 vs the 3200 preset, HBM3 4000
     * vs the 6400 preset. That is D002's defect (DDR4 array at 2400,
     * termination at 3200) repeated at three more technologies, and it
     * is how a device's reported memory bandwidth came to describe a
     * part 1.6x faster than the one whose cycles were being counted.
     *
     * The aggregate is the arithmetic the preset implies, so it is the
     * cross-check: channel_databus_bits x data_rate x channels / 8 must
     * reproduce bandwidth_. A future preset change that forgets the
     * architecture object now fails here instead of quietly re-describing the
     * part. */
    /* 1.11.57 (audit C007): the aggregate is now channel width x rate x
     * CHANNELS. It used to be channel width x rate alone, which was the same
     * arithmetic only because the HBM objects stored the whole stack in a
     * field named for one channel. With that field holding one channel for
     * every family, the channel count has to appear explicitly -- and it comes
     * from the preset above, which is the authority for how many channels this
     * run counts cycles for. DDR keeps channels_ = 1, so nothing moves there. */
    if (dram_arch_) {
        std::string dt = dram_type_;
        std::transform(dt.begin(), dt.end(), dt.begin(), ::toupper);
        double implied_mbs =
            (dram_arch_->datapath.channel_databus_bits.value_bits / 8.0) *
            dram_arch_->timing.data_rate_mtps *
            static_cast<double>(channels_ > 0 ? channels_ : 1);
        if (bandwidth_ > 0 && implied_mbs > 0.0 &&
            std::fabs(implied_mbs - static_cast<double>(bandwidth_)) >
                0.02 * static_cast<double>(bandwidth_)) {
            /* 1.11.60 (audit round 4, C008): NAME THE SOURCE THE NUMBER
             * ACTUALLY CAME FROM. This message said "the simulated preset
             * implies <bandwidth_>", and bandwidth_ is not the preset's: for
             * DDR3/DDR5/LPDDR5/GDDR6 it is derivedBwMBs(), i.e.
             * CactiIOWrapper::dramRateMTs() x dramChannelWidthBits() x
             * channels -- PIMID's own rate table, the one the ENERGY path
             * prices from -- and for DDR4/HBM2/HBM3 it is a literal in the
             * branch above. The preset is the one authority of the three that
             * this comparison never consults. It was invisible on DDR3 and
             * LPDDR5, where the rate table and the preset happen to agree
             * (1600 and 6400); GDDR6 is where they differ, and there the line
             * quoted 112000 MB/s as the preset's implication twelve lines
             * after the wrapper had announced that same preset at a different
             * rate entirely. The comparison itself is unchanged and still
             * correct -- it is the rate table against the architecture object
             * -- so only the attribution moves. */
            std::cerr << "[ramulator] WARNING: " << dt
                      << " speed-bin mismatch. This run's configured aggregate "
                         "bandwidth is " << bandwidth_
                      << " MB/s (from PIMID's rate table via "
                         "CactiIOWrapper::dramRateMTs x channel width x "
                         "channels, or a literal where that table has no row "
                         "-- NOT from the Ramulator timing preset), but the "
                         "architecture object's channel databus x data rate x "
                         "channels gives " << implied_mbs
                      << " MB/s (" << dram_arch_->timing.data_rate_mtps
                      << " MT/s). Cycles come from the preset, reported "
                         "bandwidths come from the architecture object, and "
                         "energy comes from the rate table, so this run counts "
                         "one part and prices another."
                      << std::endl;
        }
    }

    current_cycle_ = 0;
    resetStats();
}

/* 1.11.59 (audit C018): THE DEVICE WIDTH REACHES THE WIDTHS, not only the
 * energy.
 *
 * What the audit found: setDeviceWidth() wrote device_width_ and nothing else.
 * That string was read at exactly two places, both array-energy calls, where it
 * sets how many devices a rank access lights up (4/8/16). Every WIDTH the
 * wrapper reports came from the architecture object instead, and the DDR
 * objects hold a fixed 8-bit chip DQ -- so with memory.dram.device_width: x16
 * set, one run priced the rank as 4 devices for energy and modelled it as 8
 * devices of 8 pins for bandwidth, and the hierarchy link ladder's L3 rung (the
 * level literally named "chip DQ pins") was 8 bits for an x4 part and an x16
 * part alike. Worse than the number: architecture_extractor.h stamped that 8
 * VERIFIED with the source "Extracted from Ramulator device configuration
 * (x4/x8/x16)" -- a provenance claim on a value that ignored the width.
 *
 * The repair is the one the finding asks for first: make the object honour the
 * width. Four coupled quantities move together, and each is JEDEC organization
 * rather than a new constant:
 *
 *   chip_io_bits          = the configured width. That IS the x4/x8/x16 datum.
 *   chips_per_rank        = rank_databus_bits / width. A DDR-class rank
 *                           presents 64 data bits, so it takes 64/width devices
 *                           to build one -- the same arithmetic, from the same
 *                           reasoning, as chipsPerRankForDeviceWidth() in
 *                           main.cpp (1.11.57, latent A013) and the die-count
 *                           helper beside it. It also makes the extractor's
 *                           "chips_per_rank x chip_io_bits" description of the
 *                           rank bus TRUE for DDR-class parts.
 *   prefetch_datapath_bits = prefetch length x width. The field's own factory
 *                           comment defines it that way ("8n prefetch x 8-bit
 *                           I/O" for DDR4, "16n prefetch x 8-bit I/O" for
 *                           DDR5); the prefetch LENGTH is what JEDEC fixes, and
 *                           it is recovered from the object rather than
 *                           re-tabulated here.
 *   bank_groups_per_chip  = halved at x16 on DDR4 (4 -> 2) and DDR5 (8 -> 4),
 *                           which is the JEDEC coupling main.cpp already
 *                           applies to the hierarchy at the same site that
 *                           calls setDeviceWidth(). Leaving it would have the
 *                           object and the hierarchy describe two different
 *                           parts of the same run.
 *
 * WHAT DOES NOT MOVE: rank_databus_bits and channel_databus_bits. A rank is 64
 * bits wide whatever devices build it, which is the whole point of the width
 * knob, so the rank rung of the ladder and the speed-bin reconciliation check
 * are untouched. And at x8 -- the default, and what every corpus cell ran --
 * all four assignments above reproduce the factory values exactly, so no
 * existing result moves. Only x4 and x16 move, and they were wrong.
 *
 * HBM is excluded and says so: a stack has no x4/x8/x16 device width. main.cpp
 * already refuses the key for HBM; this is the wrapper refusing it on its own
 * authority, since the wrapper is reachable without main.cpp.
 *
 * RESIDUAL, stated rather than papered over: the DEFAULT Ramulator2 config
 * this wrapper generates in parseConfiguration() still names a fixed org
 * preset per technology ("DDR4_8Gb_x8", "DDR5_8Gb_x8", ...), so on that path
 * the timing model's device organization does not follow this key -- only the
 * YAML main.cpp writes for the zsim path does (writeRamulatorConfigYaml takes
 * the width). This function fixes the widths the wrapper REPORTS; it does not
 * claim to have re-bound the preset the timing model would parse. */
void RamulatorWrapper::setDeviceWidth(const std::string& w) {
    device_width_ = w;
    applyDeviceWidthToArchitecture();  // no-op until dram_arch_ exists
}

void RamulatorWrapper::applyDeviceWidthToArchitecture() {
    if (!dram_arch_ || device_width_.empty()) return;

    int w_bits = 0;
    if (device_width_ == "x4")       w_bits = 4;
    else if (device_width_ == "x8")  w_bits = 8;
    else if (device_width_ == "x16") w_bits = 16;
    if (w_bits == 0) {
        static bool warned_bad_width = false;
        if (!warned_bad_width) {
            warned_bad_width = true;
            std::cerr << "[mem] WARNING: device width \"" << device_width_
                      << "\" is not one of x4/x8/x16. The architecture object "
                         "keeps its factory organization, so the chip DQ width, "
                         "the devices per rank and every figure derived from "
                         "them describe an x"
                      << dram_arch_->datapath.chip_io_bits.value_bits
                      << " part, not the configured one." << std::endl;
        }
        return;
    }

    std::string dt = dram_type_;
    std::transform(dt.begin(), dt.end(), dt.begin(), ::toupper);
    if (dt.rfind("HBM", 0) == 0) {
        static bool warned_hbm_width = false;
        if (!warned_hbm_width) {
            warned_hbm_width = true;
            std::cerr << "[mem] NOTE: a device width (" << device_width_
                      << ") was set for " << dt
                      << ", which has no x4/x8/x16 device organization -- an "
                         "HBM stack's interface is fixed 128-bit channels. The "
                         "width is IGNORED for the architecture object and the "
                         "widths derived from it." << std::endl;
        }
        return;
    }

    if (arch_device_width_bits_ == w_bits) return;  // already stamped

    // The width currently described by this object: the stamp if one was
    // applied, otherwise the factory's own chip DQ width.
    const int base_w = arch_device_width_bits_ > 0
                           ? arch_device_width_bits_
                           : dram_arch_->datapath.chip_io_bits.value_bits;
    if (base_w <= 0) return;

    // Prefetch LENGTH (8n, 16n, ...) recovered from the object it was written
    // on, so the JEDEC number is not re-tabulated in a second place.
    const int prefetch_length =
        dram_arch_->datapath.prefetch_datapath_bits.value_bits / base_w;

    dram_arch_->datapath.chip_io_bits.value_bits = w_bits;
    dram_arch_->datapath.chip_io_bits.status =
        pimid::memory::VerificationStatus::VERIFIED;
    dram_arch_->datapath.chip_io_bits.source =
        "JEDEC device organization " + device_width_ +
        ", from the configured memory.dram.device_width -- the same key that "
        "sets the array-energy device count. NOT read from a Ramulator device "
        "object; this wrapper's default config still names a fixed x8 org "
        "preset.";
    dram_arch_->datapath.chip_io_bits.notes =
        "External package DQ pins of one device";

    const int rank_bits = dram_arch_->datapath.rank_databus_bits.value_bits;
    if (rank_bits > 0) {
        dram_arch_->organization.chips_per_rank = rank_bits / w_bits;
    }

    if (prefetch_length > 0) {
        dram_arch_->datapath.prefetch_datapath_bits.value_bits =
            prefetch_length * w_bits;
        dram_arch_->datapath.prefetch_datapath_bits.notes =
            std::to_string(prefetch_length) + "n prefetch x " +
            std::to_string(w_bits) + "-bit device I/O";
    }

    // JEDEC couples the bank-group count to the width on DDR4 and DDR5; the
    // same table main.cpp applies to the hierarchy at the calling site.
    if (dt == "DDR4")      dram_arch_->organization.bank_groups_per_chip = (w_bits == 16) ? 2 : 4;
    else if (dt == "DDR5") dram_arch_->organization.bank_groups_per_chip = (w_bits == 16) ? 4 : 8;

    arch_device_width_bits_ = w_bits;

    if (w_bits != 8) {
        static bool announced_width = false;
        if (!announced_width) {
            announced_width = true;
            std::cerr << "[mem] NOTE: " << dt << " architecture object stamped "
                      << device_width_ << ": chip DQ " << w_bits << " bits, "
                      << dram_arch_->organization.chips_per_rank
                      << " devices per " << rank_bits << "-bit rank. The rank "
                         "and channel buses are unchanged -- a rank is 64 bits "
                         "wide whatever devices build it." << std::endl;
        }
    }
}

void RamulatorWrapper::loadConfig(const std::string& config_path) {
    config_path_ = config_path;
    parseConfiguration();
}

void RamulatorWrapper::parseConfiguration() {
    // Try to load PIMID configuration and convert to Ramulator format
    if (config_path_.empty()) {
        // Generate correct Ramulator2 YAML config based on DRAM type
        std::string dt = dram_type_;
        std::transform(dt.begin(), dt.end(), dt.begin(), ::toupper);

        // Helper lambda: generate Ramulator2 YAML config
        auto makeConfig = [](const std::string& dram_impl, const std::string& org_preset,
                            const std::string& timing_preset) -> std::string {
            return "Frontend:\n  impl: GEM5\n\nMemorySystem:\n  impl: GenericDRAM\n  clock_ratio: 1\n"
                   "  DRAM:\n    impl: " + dram_impl + "\n    org:\n      preset: " + org_preset +
                   "\n    timing:\n      preset: " + timing_preset +
                   "\n  Controller:\n    impl: Generic\n    Scheduler:\n      impl: FRFCFS\n"
                   "    RefreshManager:\n      impl: AllBank\n"
                   "    RowPolicy:\n      impl: ClosedRowPolicy\n      cap: 4\n"
                   "  AddrMapper:\n    impl: RoBaRaCoCh\n";
        };

        /* 1.11.52 (audit D016): BANDWIDTH IS DERIVED, and a preset that
         * cannot express the modelled rate says so.
         *
         * These branches carried a literal MB/s beside a Ramulator timing
         * preset, and the two drifted apart from the rate the ENERGY path
         * prices at: DDR5 was configured at the DDR5_3200AN preset with a
         * literal 25600 MB/s while pimid_energy and CACTI-IO price DDR5 at
         * 4800 MT/s; GDDR6 carried 64000 MB/s against a 14000 MT/s energy
         * rate. Bandwidth is now rate x channel width x channels from the
         * SAME rate table the energy path uses (spec primitives), so the
         * literal cannot drift.
         *
         * The preset mismatch is NOT silently resolved: upstream Ramulator2
         * ships no DDR5 timing bin above 3200, so the timing model genuinely
         * cannot run at the modelled 4800. That is a modelling limitation,
         * and it is announced once here rather than hidden in two numbers
         * that disagree. */
        auto derivedBwMBs = [](const std::string& t, uint32_t chans) -> uint64_t {
            double mts = PIMID::CactiIOWrapper::dramRateMTs(t);
            int wbits  = PIMID::CactiIOWrapper::dramChannelWidthBits(t);
            if (mts <= 0.0 || wbits <= 0) return 0;
            return static_cast<uint64_t>(mts * (wbits / 8.0) * chans);
        };
        auto sayPresetRate = [](const std::string& t, const char* preset,
                                double preset_mts) {
            double mts = PIMID::CactiIOWrapper::dramRateMTs(t);
            if (mts > 0.0 && preset_mts > 0.0 &&
                std::fabs(mts - preset_mts) > 1.0) {
                std::cerr << "[mem] NOTE: " << t << " is modelled at " << mts
                          << " MT/s (energy, termination and bandwidth), while "
                             "the Ramulator2 timing preset in use is " << preset
                          << " at " << preset_mts << " MT/s -- upstream ships "
                             "no timing bin at the modelled rate. Timing comes "
                             "from the preset; everything else from the "
                             "modelled rate." << std::endl;
            }
        };

        if (dt == "DDR3") {
            config_yaml_ = makeConfig("DDR3", "DDR3_8Gb_x8", "DDR3_1600H");
            channels_ = 1; ranks_per_channel_ = 1; banks_per_rank_ = 8;
            capacity_ = 8ULL * 1024 * 1024 * 1024;
            bandwidth_ = derivedBwMBs("DDR3", channels_);   // 1.11.52 (D016)
            sayPresetRate("DDR3", "DDR3_1600H", 1600);
        } else if (dt == "DDR5") {
            config_yaml_ = makeConfig("DDR5", "DDR5_8Gb_x8", "DDR5_3200AN");
            channels_ = 1; ranks_per_channel_ = 1; banks_per_rank_ = 16;
            capacity_ = 8ULL * 1024 * 1024 * 1024;
            bandwidth_ = derivedBwMBs("DDR5", channels_);   // 1.11.52 (D016)
            sayPresetRate("DDR5", "DDR5_3200AN", 3200);
        } else if (dt == "LPDDR5") {
            config_yaml_ = makeConfig("LPDDR5", "LPDDR5_8Gb_x16", "LPDDR5_6400");
            channels_ = 1; ranks_per_channel_ = 1; banks_per_rank_ = 16;
            capacity_ = 8ULL * 1024 * 1024 * 1024;
            bandwidth_ = derivedBwMBs("LPDDR5", channels_);  // 1.11.52 (D016)
            sayPresetRate("LPDDR5", "LPDDR5_6400", 6400);
        } else if (dt == "GDDR6") {
            /* 1.11.60 (audit round 4, C007): the preset named here did not
             * exist. `grep GDDR6_2000_1.35V_x16 external/ramulator/` returns
             * nothing; the four GDDR6 timing presets this fork ships are
             * GDDR6_2000_{1350mV,1250mV}_{double,quad} (GDDR6.cpp:34-37), and
             * Ramulator2 throws ConfigurationError on an unrecognised name.
             * It survived because this generated YAML is never handed to
             * Ramulator on this path -- initialize() builds an instance only
             * when config_path_ is non-empty, and then config_yaml_ holds the
             * FILE's contents -- so the string reached nothing but the note
             * below. Named to match what the run really writes for the timing
             * model (main.cpp's GDDR6 emission), so that if this path is ever
             * made live it selects a preset that exists. */
            config_yaml_ = makeConfig("GDDR6", "GDDR6_8Gb_x16", "GDDR6_2000_1350mV_double");
            // GDDR6 is a dual-channel part (2 x 16-bit channels per device,
            // JESD250; see docs/dram_specs.md: 32 GB/s/channel x 2 = 64 GB/s).
            // channels_ = 1 here used to contradict that spec and made the
            // analytical model treat the full 64 GB/s as one channel's rate.
            channels_ = 2; ranks_per_channel_ = 1; banks_per_rank_ = 16;
            capacity_ = 8ULL * 1024 * 1024 * 1024;
            bandwidth_ = derivedBwMBs("GDDR6", channels_);   // 1.11.52 (D016)
            /* 1.11.60 (audit round 4, C007): both facts in this note were
             * wrong. The preset named did not exist in the tree, and the
             * 16000 MT/s attributed to it is not a rate any GDDR6 preset
             * carries -- all four ship rate = 2000 in Ramulator2's timing
             * table, the column GDDR6.cpp:33 documents as "rate (in MT/s)"
             * and from which GDDR6.cpp:259 derives tCK as 1e6/(rate/2) ps. So
             * a reader reconciling PIMID's 14000 MT/s energy basis against the
             * timing model was handed a third number under a name that
             * matched nothing. The note now carries the preset the run writes
             * and the rate that preset holds; the claim it exists to make --
             * upstream ships no GDDR6 timing bin at the modelled rate -- is
             * unchanged and still true, since 2000 is the only rate on offer. */
            sayPresetRate("GDDR6", "GDDR6_2000_1350mV_double", 2000);
        } else if (dt == "HBM2") {
            config_yaml_ = makeConfig("HBM2", "HBM2_4Gb", "HBM2_2.4Gbps");
            channels_ = 8; ranks_per_channel_ = 1; banks_per_rank_ = 16;
            capacity_ = 4ULL * 1024 * 1024 * 1024;
            bandwidth_ = 307000;  // 307 GB/s for HBM2 @ 2.4 Gb/s (1024b x 2.4 / 8)
        } else if (dt == "HBM3") {
            config_yaml_ = makeConfig("HBM3", "HBM3_4Gb", "HBM3_6.4Gbps");
            channels_ = 16; ranks_per_channel_ = 1; banks_per_rank_ = 16;
            capacity_ = 4ULL * 1024 * 1024 * 1024;
            bandwidth_ = 819000;  // 819 GB/s for HBM3 @ 6.4 Gb/s (1024b x 6.4 / 8)
        } else {
            // Default: DDR4-2400
            config_yaml_ = makeConfig("DDR4", "DDR4_8Gb_x8", "DDR4_2400R");
            channels_ = 1; ranks_per_channel_ = 1; banks_per_rank_ = 8;
            capacity_ = 8ULL * 1024 * 1024 * 1024;
            bandwidth_ = 19200;  // 19.2 GB/s for DDR4-2400
        }

    } else {
        // Load configuration from file
        std::ifstream config_file(config_path_);
        if (config_file.is_open()) {
            std::stringstream buffer;
            buffer << config_file.rdbuf();
            config_yaml_ = buffer.str();

            try {
                YAML::Node config = YAML::Load(config_yaml_);

                // Parse basic parameters
                if (config["dram"]) {
                    auto dram = config["dram"];
                    if (dram["channels"]) channels_ = dram["channels"].as<uint32_t>();
                    if (dram["ranks"]) ranks_per_channel_ = dram["ranks"].as<uint32_t>();
                    if (dram["banks"]) banks_per_rank_ = dram["banks"].as<uint32_t>();
                    if (dram["capacity"]) capacity_ = dram["capacity"].as<uint64_t>();
                    if (dram["bandwidth"]) bandwidth_ = dram["bandwidth"].as<uint64_t>();
                }
            } catch (const YAML::Exception& e) {
                std::cerr << "Warning: Failed to parse YAML config: " << e.what() << std::endl;
                std::cerr << "Using default DDR4 configuration" << std::endl;
            }
        }
    }
}

void RamulatorWrapper::createRamulatorInstance() {
    try {
        // Parse YAML configuration for Ramulator
        YAML::Node config = YAML::Load(config_yaml_);

        // Create Ramulator memory system using factory
        auto memory_system_raw = Ramulator::Factory::create_memory_system(config);
        ramulator_memory_system_ = std::shared_ptr<Ramulator::IMemorySystem>(memory_system_raw);

        if (!ramulator_memory_system_) {
            return;
        }

    } catch (const std::exception& e) {
        // Ramulator2 instance creation failed -- timing/power queries
        // will use DRAMArchitectureV2 fallback values instead.
        ramulator_memory_system_.reset();
    }
}

bool RamulatorWrapper::send(Address addr, MemoryRequestType type,
                             std::function<void(Address)> callback) {
    // Track statistics
    if (type == MemoryRequestType::READ) {
        total_reads_++;
    } else if (type == MemoryRequestType::WRITE) {
        total_writes_++;
    }

    // If Ramulator is available, use it
    if (ramulator_memory_system_) {
        Ramulator::Request req = createRamulatorRequest(addr, type);

        // Set up callback to track completion
        req.callback = [this, addr, callback](Ramulator::Request& completed_req) {
            handleRequestCompletion(completed_req);
            if (callback) {
                callback(addr);
            }
        };

        bool accepted = ramulator_memory_system_->send(req);

        if (accepted && callback) {
            // Track pending request
            pending_requests_.push_back({addr, type, current_cycle_, callback});
        }

        return accepted;
    } else {
        // Fallback: accept all requests immediately
        if (callback) {
            callback(addr);
        }
        return true;
    }
}

bool RamulatorWrapper::canAccept() const {
    if (ramulator_memory_system_) {
        // Ramulator checks internally if it can accept more requests
        // We assume it can if queue is not full (checked in send())
        return true;
    }
    return true;
}

void RamulatorWrapper::tick() {
    current_cycle_++;

    if (ramulator_memory_system_) {
        ramulator_memory_system_->tick();
    }

    // Tick PIM components
    if (pim_enabled_ && pim_plugin_) {
        pim_plugin_->tick();
    }
}

Ramulator::Request RamulatorWrapper::createRamulatorRequest(
    Address addr, MemoryRequestType type) {

    int req_type = (type == MemoryRequestType::READ) ?
                   Ramulator::Request::Type::Read :
                   Ramulator::Request::Type::Write;

    return Ramulator::Request(static_cast<Ramulator::Addr_t>(addr), req_type);
}

void RamulatorWrapper::handleRequestCompletion(Ramulator::Request& req) {
    /* 1.11.57 (latent D009): this is where the row-buffer counters were meant
     * to be updated, and the "placeholder for now" that stood here was the
     * whole of their implementation -- so getRowHits()/getRowMisses()/
     * getRowConflicts() returned 0 for the lifetime of every wrapper while
     * four consumers treated them as measurements. The counters are gone
     * (see ramulator_wrapper.h); this hook stays because it is the callback
     * Ramulator2 invokes on completion and a real instance would need it.
     * There is nothing to read yet: no Ramulator2 instance is ever created in
     * this tree, because every construction site passes an empty config path.
     * Whoever gives this wrapper a live instance should populate the row
     * statistics from req/the controller HERE, and should add the counters
     * back at the same time rather than reviving the empty ones. */
    (void)req;
}

double RamulatorWrapper::getReadEnergy() const {
    updateEnergyMetrics();
    return cached_read_energy_;
}

double RamulatorWrapper::getWriteEnergy() const {
    updateEnergyMetrics();
    return cached_write_energy_;
}

double RamulatorWrapper::getActivationEnergy() const {
    // Energy for row activations (ACT command)
    // Based on DRAM power model from literature and DRAM architecture
    //
    // DDR4 typical values from NVIDIA-HPCA17, DAS-MICRO15:
    //   - Row activation: ~2-3 nJ per activation
    //   - Includes: wordline driver, bitline precharge, sense amp settling
    //
    // HBM2: Lower due to shorter bitlines and TSV architecture (~1-1.5 nJ)

    double activation_energy_per_op_nJ = 2.5;  // Default DDR4 value

    // Use actual energy from DRAM architecture if available
    if (dram_arch_) {
        // Bank energy includes activation + column access
        // Estimate activation is ~60% of total bank energy
        activation_energy_per_op_nJ = dram_arch_->energy.bank_energy_pJ * 0.6 / 1000.0;
    }

    /* 1.11.57 (latent D009): the activation count came from row_misses_ +
     * row_conflicts_, two counters that were never incremented, so the sum was
     * always 0 and the "if no statistics" branch was the ONLY branch: every
     * activation count this function ever returned was accesses/2, i.e. a
     * hardcoded 50% row-miss rate wearing the clothes of a measurement. The
     * run does measure the row-miss fraction, it just arrives by another road
     * (setRowMissFraction, from the PE memory interface); use it, and fall
     * back to the stated 0.5 only when the run carried no measurement -- the
     * same convention arrayReadNJ() uses for the same quantity. */
    const double miss_frac = (row_miss_frac_ >= 0.0 && row_miss_frac_ <= 1.0)
                             ? row_miss_frac_ : 0.5;
    double estimated_activations =
        static_cast<double>(total_reads_ + total_writes_) * miss_frac;

    return estimated_activations * activation_energy_per_op_nJ;
}

double RamulatorWrapper::getPrechargeEnergy() const {
    // Energy for row precharges (PRE command)
    // Based on DRAM power model from literature
    //
    // DDR4 typical values:
    //   - Precharge: ~1.5-2 nJ per precharge
    //   - Includes: bitline discharge, sense amp reset
    //
    // HBM2: Lower due to TSV architecture (~0.8-1 nJ)

    double precharge_energy_per_op_nJ = 1.8;  // Default DDR4 value

    // Use actual energy from DRAM architecture if available
    if (dram_arch_) {
        // Estimate precharge is ~40% of total bank energy
        precharge_energy_per_op_nJ = dram_arch_->energy.bank_energy_pJ * 0.4 / 1000.0;
    }

    // Estimate number of precharges (roughly equal to activations)
    // 1.11.57 (latent D009): same substitution as getActivationEnergy above --
    // the two counters this used to read were structurally 0.
    const double miss_frac = (row_miss_frac_ >= 0.0 && row_miss_frac_ <= 1.0)
                             ? row_miss_frac_ : 0.5;
    double estimated_precharges =
        static_cast<double>(total_reads_ + total_writes_) * miss_frac;

    return estimated_precharges * precharge_energy_per_op_nJ;
}

double RamulatorWrapper::getRefreshEnergy() const {
    // Energy for DRAM refresh operations
    // Based on JEDEC specs and literature
    //
    // DDR4 refresh parameters:
    //   - tREFI = 7.8us (average refresh interval)
    //   - Each refresh activates one row per bank
    //   - Energy per refresh ~= activation energy
    //
    // Refresh energy = (num_refreshes) x (energy_per_refresh) x (num_banks)

    double refresh_energy_per_row_nJ = 2.0;  // Default: similar to activation

    // Use DRAM architecture energy if available
    if (dram_arch_) {
        // Refresh energy is similar to activation (same operation, different trigger)
        refresh_energy_per_row_nJ = dram_arch_->energy.bank_energy_pJ * 0.6 / 1000.0;
    }

    /* 1.11.57 (latent D014): tREFI IS PER TECHNOLOGY, and this function used
     * DDR4's 7.8 us for all seven. pimid_energy.h's IDD table has carried a
     * trefi_ns column since 1.9.10 -- DDR5 and HBM refresh every 3.9 us, GDDR6
     * every 1.9 us -- so an HBM3 run counted half the refreshes it should and
     * a GDDR6 run counted a quarter. It was invisible because this function's
     * only caller is getTotalEnergy(), whose only caller is printStats() and
     * an unreachable PowerModelManager; no reported refresh number comes from
     * here (the reported one is getRefreshPowerMW(), which reads the same
     * per-tech column correctly). Read the column instead of restating one
     * technology's value as if it were all of them.
     *
     * RESIDUAL, stated: current_cycle_ is this wrapper's own tick count, and
     * nothing in this tree ticks it, so refresh_count is 0 in practice. The
     * clock domain is the DRAM clock from the architecture object, which is
     * the domain tREFI is specified in. */
    const double trefi_ns =
        Ramulator::pimid_energy::iddFor(dram_type_).trefi_ns;

    double clock_period_ns = 1.0;  // Default 1ns modeling cycle
    if (dram_arch_) {
        clock_period_ns = 1000.0 / dram_arch_->timing.clock_freq_mhz;
    }

    double tREFI_cycles = (trefi_ns > 0.0 && clock_period_ns > 0.0)
                          ? (trefi_ns / clock_period_ns) : 0.0;
    if (!(tREFI_cycles >= 1.0)) return 0.0;
    uint64_t refresh_count = current_cycle_ / static_cast<uint64_t>(tREFI_cycles);

    // Total refresh energy = refreshes x banks x energy_per_refresh
    uint32_t total_banks = banks_per_rank_ * ranks_per_channel_ * channels_;
    return refresh_count * total_banks * refresh_energy_per_row_nJ;
}

double RamulatorWrapper::getLeakagePower() const {
    updateEnergyMetrics();
    return cached_leakage_power_;
}

double RamulatorWrapper::getTotalEnergy() const {
    /* 1.11.57 (latent D015): ACTIVATE AND PRECHARGE ARE NOT ADDED HERE ANY
     * MORE, because they were already inside the read and write terms. All
     * three come from the same source: updateEnergyMetrics() sets the per
     * access read energy to bank_energy_pJ x 64 B, which is the FULL access --
     * activate, column access and precharge -- while getActivationEnergy() and
     * getPrechargeEnergy() return a further 0.6 and 0.4 of the same
     * bank_energy_pJ per row miss. Summing all four charged the array roughly
     * twice for every row miss. main.cpp's own intensive path states the rule
     * this now follows ("getArrayReadEnergyNJ folds activation and column
     * access, so act/pre are NOT added separately"); this function predates it
     * and contradicted it. It was invisible because getTotalEnergy() is
     * reached only from printStats() (no callers) and from
     * PowerModelManager::... (never instantiated, and which additionally reads
     * this nJ value into a variable named total_energy_j).
     *
     * The two accessors are kept -- DRAMModel::tick() reports them as their own
     * line items, which is legitimate -- they simply must not be summed with a
     * total that already contains them. */
    /* 1.11.57 (latent D015, second unit): the leakage term was
     * getLeakagePower() * current_cycle_ / 1e6, which is 1000x too small.
     * getLeakagePower() returns MILLIWATTS and this total is in NANOJOULES:
     * mW x ns = 1e-3 W x 1e-9 s = 1e-12 J = 1e-3 nJ, so the divisor is 1e3,
     * not 1e6. The cycle period is the DRAM clock the wrapper's other
     * cycle-domain arithmetic uses, rather than an unstated 1 ns. */
    double clock_period_ns = 1.0;
    if (dram_arch_ && dram_arch_->timing.clock_freq_mhz > 0.0)
        clock_period_ns = 1000.0 / dram_arch_->timing.clock_freq_mhz;
    const double leakage_nJ =
        getLeakagePower() * (static_cast<double>(current_cycle_) *
                             clock_period_ns) / 1000.0;
    return getReadEnergy() + getWriteEnergy() + getRefreshEnergy() + leakage_nJ;
}

Cycle RamulatorWrapper::getAverageLatency() const {
    if (total_reads_ + total_writes_ == 0) {
        return 0;
    }

    // Calculate average DRAM latency based on row buffer hit/miss behavior
    // This is a realistic calculation based on DRAM timing parameters
    //
    // Row Hit: tCAS (Column Access Strobe) - ~13-15ns for DDR4-2400
    // Row Miss (Conflict): tRP + tRCD + tCAS - ~40ns for DDR4-2400
    //
    // Formula: avg_latency = hit_rate * hit_latency + miss_rate * miss_latency

    // Get DRAM timing parameters from architecture if available
    // For DDR4-2400: tCAS=13.32ns, tRCD=13.32ns, tRP=13.32ns
    // For HBM2: tCAS=12.5ns, tRCD=12.5ns, tRP=12.5ns
    double tCAS_cycles = 16;   // ~13.32ns @ 1.2GHz for DDR4-2400
    double tRCD_cycles = 16;   // ~13.32ns @ 1.2GHz
    double tRP_cycles = 16;    // ~13.32ns @ 1.2GHz

    // If DRAM architecture is available, use actual timing
    if (dram_arch_) {
        double clock_period_ns = 1000.0 / dram_arch_->timing.clock_freq_mhz;
        tCAS_cycles = std::ceil(dram_arch_->timing.tCAS_ns / clock_period_ns);
        tRCD_cycles = std::ceil(dram_arch_->timing.tRCD_ns / clock_period_ns);
        tRP_cycles = std::ceil(dram_arch_->timing.tRP_ns / clock_period_ns);
    }

    /* 1.11.57 (latent D009): the hit rate came from row_hits_ against three
     * counters that were never incremented, so the "use actual statistics from
     * Ramulator" branch could never be taken and every average latency this
     * function returned used the 0.5 in the else. The measured quantity is
     * row_miss_frac_, set by the caller from the run's own PE-memory-interface
     * row statistics; use it, and keep 0.5 only as the stated fallback for a
     * run that carried no measurement. */
    double hit_rate = (row_miss_frac_ >= 0.0 && row_miss_frac_ <= 1.0)
                      ? (1.0 - row_miss_frac_) : 0.5;

    // Row hit latency (just column access)
    Cycle hit_latency = static_cast<Cycle>(tCAS_cycles);

    // Row miss/conflict latency (precharge + activate + column)
    Cycle miss_latency = static_cast<Cycle>(tRP_cycles + tRCD_cycles + tCAS_cycles);

    // Weighted average latency
    double avg_latency = hit_rate * hit_latency + (1.0 - hit_rate) * miss_latency;

    return static_cast<Cycle>(avg_latency);
}

// ---------------------------------------------------------------------------
// 1.9.10: JEDEC IDD/VDD table + intensive per-access / background accessors.
//
// Root cause of the historical "0.000 nJ/access" in the power report:
//   (1) getReadEnergy()/getWriteEnergy() return total_reads_ * per-access, and
//       runPowerAnalysis queried them on a FRESH standalone oracle that never
//       processes accesses (total_reads_ == 0) while treating the result as a
//       per-access value; and
//   (2) updateEnergyMetrics() early-returns when current_cycle_==last_energy_update_
//       which is 0==0 on that oracle, so nothing is ever computed.
// The DRAM-arch presets DO carry non-zero bank energy (DDR5 1.6, HBM3 0.8
// pJ/byte), so the array term always existed -- it was just never surfaced as an
// intensive quantity. The accessors below fix that and add IDD-based interface
// (off-chip I/O) and background+refresh power, so a run yields real numbers.
// 1.9.10: the intensive DRAM energy physics has been RELOCATED into Ramulator2
// (external/ramulator/src/dram/pimid_energy.h). These accessors are now thin
// readers -- they forward the wrapper's own timing getters + the user override
// knobs into the Ramulator2-resident model. main.cpp is untouched (same API).
double RamulatorWrapper::getArrayReadEnergyNJ() const {
    return Ramulator::pimid_energy::arrayReadNJ(
        dram_type_, getTRC(), getTRAS(), getTBurst(), energy_bank_override_pJ_per_byte_,
        device_width_,        // 1.11.46 (L181): whole-rank basis
        row_miss_frac_);      // 1.11.52 (D003): measured row-miss fraction
}
double RamulatorWrapper::getArrayWriteEnergyNJ() const {
    return Ramulator::pimid_energy::arrayWriteNJ(
        dram_type_, getTRC(), getTRAS(), getTBurst(), energy_bank_override_pJ_per_byte_,
        device_width_,        // 1.11.46 (L181)
        row_miss_frac_);      // 1.11.52 (D003)
}
double RamulatorWrapper::getTerminationEnergyNJ() const {
    /* 1.11.40 (audit N8, user ruling: harness the model, do not table the
     * answer). The DQ termination energy now comes from CACTI-IO -- a real
     * off-chip IO model built from extracted parameters -- instead of the
     * hand-written SSTL/POD/LVSTL scheme table in pimid_energy.h.
     *
     * WHY THE SWITCH MATTERS, measured at gate 1155b (pJ/bit, this term only):
     *     tech     hand table   CACTI-IO   ratio
     *     DDR3       4.7508      (model)    ~2.6x on the FULL interface
     *     DDR4       2.5568                 ~2.1x
     *     DDR5       1.4323                 ~3.3x
     *     LPDDR5     0.0349      4.9547     142x on the full interface
     *
     * 1.11.57 (latent D010): NOT ONE OF THOSE FOUR ROWS REPRODUCES ANY MORE.
     * They are kept above as the historical measurement they are -- gate 1155b
     * ran against the 1.11.40 scheme table -- and must not be read as a
     * current property of the code. Recomputed from today's terminationNJ()
     * and today's rate table:
     *     DDR3   (1.35/2)^2 / 74 / 1.6e9   = 3.848  pJ/bit, not 4.7508
     *            (4.7508 was the pre-1.11.46 SSTL-15 1.5 V part; 1.11.46 moved
     *             DDR3 to the 1.35 V DDR3L the IDD row already described)
     *     DDR4   0.5*1.2^2 / 88 / 2.4e9    = 3.409  pJ/bit, not 2.5568
     *            (2.5568 is the 3200 MT/s figure; 1.11.52 moved DDR4 to the
     *             DDR4-2400 part this tree simulates)
     *     DDR5   0.5*1.1^2 / 88 / 3.2e9    = 2.148  pJ/bit, not 1.4323
     *            (1.4323 is the 4800 MT/s figure; 1.11.57 C001/D017 moved DDR5
     *             to the simulated DDR5_3200AN preset)
     *     LPDDR5 0.5*0.5^2 / 280 / 6.4e9   = 0.0698 pJ/bit, not 0.0349
     *            (exactly 2x: the 1.11.26 LVSTL rework introduced the 0.5 HIGH
     *             duty this row lacked)
     * Three of the four moved because the SPEED BIN was corrected, which is
     * the point: a measured comparison table is only valid against the inputs
     * it was measured with, and this one outlived three of them.
     *
     * The 142x that the paragraph below builds on therefore rests on a
     * superseded LPDDR5 row. The ARGUMENT survives -- 0.0698 pJ/bit against
     * CACTI-IO's 4.9547 is still 71x, and still a number with no physical
     * meaning for a mobile DQ interface -- but the factor is 71, not 142.
     * All of this was invisible because it is a source comment: nothing prints
     * it and no emitted value is derived from it.
     * The DDR gaps are the hand table modelling ONLY termination while the
     * interface also burns driver-switching and PHY energy. LPDDR5's gap (71x
     * on today's numbers, 142x as originally measured) is a different failure:
     * LVSTL exists precisely to eliminate static termination current, so the
     * one term the table modelled is the one term LVSTL makes negligible, and
     * a per-bit termination energy of 0.07 pJ is a number with no physical
     * meaning for a DQ interface. Real mobile DRAM interfaces sit near
     * 3-6 pJ/bit.
     *
     * TERM-BY-TERM, not aggregate: this call is the TERMINATION line, so it
     * takes CACTI-IO's termination component alone. Substituting the model's
     * full interface total here would silently fold driver and PHY energy into
     * a quantity named "termination" and make the swap uncheckable.
     *
     * The explicit override still wins, and CACTI-IO refusing (GDDR6 has no
     * parameter set) falls back to the scheme table with that stated -- a
     * refusal must not silently become zero. */
    /* 1.11.57 (latent D017): the transfer rate is handed to the scheme table
     * instead of the scheme table keeping its own copy. dramRateMTs() is the
     * one place in the tree that answers "what rate is this part", and it is
     * already what the bandwidth, the burst time and the CACTI-IO termination
     * figure come from -- the duplicate column inside pimid_energy.h had
     * already drifted to DDR5-4800 after C001 moved this table to the
     * simulated DDR5_3200AN preset. */
    const double rate = PIMID::CactiIOWrapper::dramRateMTs(dram_type_);
    if (energy_term_override_pJ_per_bit_ >= 0.0)
        return Ramulator::pimid_energy::terminationNJ(
            dram_type_, energy_term_override_pJ_per_bit_, rate);

    const int    ndq  = PIMID::CactiIOWrapper::dramChannelWidthBits(dram_type_);
    if (rate > 0.0 && ndq > 0) {
        /* activity = 1.0: this is an energy PER ACCESS, so the interface is
         * active for the whole of it. Averaging by a duty cycle here would
         * charge the access for the idle time around it. */
        PIMID::LinkIOResult io =
            PIMID::CactiIOWrapper::computeDramIO(dram_type_, ndq, rate, 1.0);
        /* ONLY AN EXACT MAP MAY REPLACE A RESULT. DDR3/DDR4/DDR5 map exactly
         * onto CACTI-IO parameter sets. LPDDR5 and HBM do not -- they borrow
         * LPDDR2 and WideIO, whose parameters were fitted for different
         * interfaces, and the borrowed numbers do not survive a sanity check
         * (LPDDR5 comes out near 34 pJ/bit against a published 3-6). Letting an
         * approximate map rewrite a result would replace one unsourced number
         * with another and call it a model. Those technologies keep the
         * existing path; the model's figure is reported as a cross-check
         * elsewhere, not substituted here. */
        if (io.valid && io.exact_map && io.energy_pj_per_bit_term > 0.0)
            return io.energy_pj_per_bit_term * 512.0 / 1000.0;   // per 64 B
    }
    /* Fall back, and say so rather than reporting zero. */
    static bool warned = false;
    if (!warned) {
        warned = true;
        std::cerr << "[power] NOTE: CACTI-IO has no parameter set for '"
                  << dram_type_ << "'; DQ termination falls back to the "
                     "pimid_energy scheme table (termination term only)."
                  << std::endl;
        /* 1.11.52 (audit D008): name the UNSOURCED input where it is used.
         * LPDDR5's termination resistance is an assumption (Micron states
         * programmable VSS ODT and gives VDDQ/RON, not Rtt), and it is the
         * larger half of the 280-ohm loop, so the reported number swings
         * ~1.75x for a 2x error in it. Disclosing this only in a comment in
         * another file is not disclosure. */
        if (dram_type_ == "LPDDR5") {
            /* 1.11.59 (LPDDR5 IO sourcing): the note now says WHICH END of the
             * only citable range the assumption sits at, because that fixes
             * the sign of the error rather than leaving it open. */
            std::cerr << "[power] NOTE: the LPDDR5 termination number rests on "
                         "an UNSOURCED Rtt = 240 ohm (of a 280-ohm loop); "
                         "Micron's datasheets give VDDQ (0.50 V) and RON "
                         "(40 ohm) but defer the ODT ohm table to a General "
                         "LPDDR5 AC/DC specification this tree does not hold, "
                         "and no LVSTL document exists in the JESD8-* set here. "
                         "The only citable bound is host-side: Intel 743844-015 "
                         "Table 89 gives RODT(DQ) = 30-240 ohm with no typical, "
                         "so 240 is the MAXIMUM of that range -- the weakest "
                         "termination, hence the LOWEST termination energy it "
                         "allows; the 30 ohm end would raise this term about "
                         "4x. Treat LPDDR5 termination as an optimistic "
                         "assumption, not a sourced value." << std::endl;
        }
    }
    return Ramulator::pimid_energy::terminationNJ(
        dram_type_, energy_term_override_pJ_per_bit_, rate);   // 1.11.57 (D017)
}

bool RamulatorWrapper::getTerminationEnergyBandNJ(double& lo_nj, double& hi_nj,
                                                  std::string& provenance) const {
    /* 1.11.59: report a BAND where the termination rests on an unsourced Rtt.
     *
     * The project rule is that a quantity the tools cannot produce is stated
     * as a band with its provenance, never as a constant -- the pJ/bit link
     * energies already work this way. LPDDR5 is the one technology whose Rtt
     * this tree cannot source: Micron gives VDDQ and RON and defers the ODT
     * ohm table to a General LPDDR5 AC/DC specification not held here, and
     * there is no LVSTL document in the JESD8-* set. A web search of every
     * freely available datasheet found the encoding MR11 OP[6:4] = 000B for
     * "ODT disabled" (YM5XCBQ3B2-T16 64 Gb LPDDR5, IDD table note 2, p.10)
     * and no ohm ladder and no stated default anywhere.
     *
     * The only citable range is host-side: Intel 743844-015 Table 89 gives
     * RODT(DQ) = 30..240 ohm with an EMPTY typical column. Termination
     * current is V/(RON + Rtt), so the two ends of that range bracket the
     * term by the ratio of their loop resistances -- with RON = 40 ohm
     * (Micron, IDD note 4), (40+240)/(40+30) = 4.0. The applied value stays
     * the 240 ohm end, which is the weakest termination and therefore the
     * LOWEST energy the range permits; this returns the interval so a reader
     * sees the assumption's width and its direction rather than a bare point.
     *
     * Returns false for technologies whose electricals ARE sourced (DDR3,
     * DDR4, DDR5, GDDR6) -- there is no band to report, only a value. */
    lo_nj = hi_nj = 0.0;
    provenance.clear();
    std::string dt = dram_type_;
    std::transform(dt.begin(), dt.end(), dt.begin(), ::toupper);
    if (dt != "LPDDR5") return false;

    const double applied = getTerminationEnergyNJ();
    if (!(applied > 0.0)) return false;

    const double ron    = 40.0;    // Micron LPDDR5X IDD table note 4
    const double rtt_hi = 240.0;   // Intel 743844-015 Tbl 89 max -- the value applied
    const double rtt_lo = 30.0;    // same table, min
    const double ratio  = (ron + rtt_hi) / (ron + rtt_lo);   // 4.0

    lo_nj = applied;               // weakest termination = lowest energy
    hi_nj = applied * ratio;       // strongest termination in the citable range
    provenance = "Rtt UNSOURCED; band = RODT(DQ) 30-240 ohm "
                 "(Intel 743844-015 Tbl 89 p.211, typical column empty) "
                 "with RON 40 ohm (Micron LPDDR5X IDD note 4); "
                 "applied value is the 240 ohm end, the lowest-energy bound";
    return true;
}

/* 1.11.57 (latent D011), SUPERSEDED BY 1.11.58: both are wired in now.
 *
 * The block below described a real defect -- these two had no caller, so the
 * 1.11.40 interface correction reached no reported number and the DQ
 * interface was termination-only in every result. 1.11.58 consumed both at
 * device and system scope, so that is no longer true: driver+PHY and the IO
 * area now enter the reported energy and the reported area, and the split is
 * printed. The one thing the original note said that STILL holds is the
 * reach: these return zero unless CACTI-IO has an exact parameter map, which
 * exists for DDR3, DDR4, DDR5 and GDDR6 -- and not for LPDDR5 (Rtt
 * unsourced; see the band accessor above) or HBM2/HBM3 (no electrical row at
 * all; HBM specifies driver strength in mA, not ohms, and its interface is
 * unterminated by design). The historical note is kept below for the record.
 *
 * ---- as written at 1.11.57 ----
 * THESE TWO ARE COMPUTED AND NOBODY READS THEM.
 *
 * getInterfaceDynamicEnergyNJ() and getInterfaceAreaMM2() have no caller
 * anywhere in src/ or include/ -- grep finds only these definitions and their
 * declarations in ramulator_wrapper.h. The two reported DQ-interface numbers,
 * main.cpp's device-scope and system-scope interface lines, both consume
 * getTerminationEnergyNJ() alone. So the DQ interface in every PIMID result is
 * still TERMINATION ONLY, and the 1.11.40 correction these functions carry --
 * that driver switching and PHY are the terms that matter, and that leaving
 * them out understates LPDDR5's interface by roughly two orders of magnitude
 * (see the measured table above) -- is present in the code and absent from
 * every number the simulator prints.
 *
 * They are kept, not deleted: they are a correct model of a term PIMID does
 * not yet charge, and deleting them would delete the correction rather than
 * land it. What is fixed here is the silence -- an interface-energy header
 * that advertises a fix no reported number receives is worse than no header.
 * Wiring them in belongs in main.cpp (an owner of that file must add them to
 * the interface line and re-derive, because it MOVES every DRAM interface
 * energy the corpus reports), which is why it is not done here. */
double RamulatorWrapper::getInterfaceDynamicEnergyNJ() const {
    /* 1.11.40: driver switching + PHY, per 64 B. PIMID has never modelled
     * these -- the interface was termination-only -- so this is new accounting
     * rather than a re-attribution. Zero when CACTI-IO has no parameter set,
     * which is honest: we do not know it, rather than it being absent.
     * UNUSED AND UNVALIDATED as of 1.11.57 -- see the note above. */
    const double rate = PIMID::CactiIOWrapper::dramRateMTs(dram_type_);
    const int    ndq  = PIMID::CactiIOWrapper::dramChannelWidthBits(dram_type_);
    if (rate <= 0.0 || ndq <= 0) return 0.0;
    PIMID::LinkIOResult io =
        PIMID::CactiIOWrapper::computeDramIO(dram_type_, ndq, rate, 1.0);
    if (!io.valid || !io.exact_map) return 0.0;   // exact maps only, as above
    const double non_term = io.energy_pj_per_bit - io.energy_pj_per_bit_term;
    return (non_term > 0.0) ? non_term * 512.0 / 1000.0 : 0.0;
}

double RamulatorWrapper::getInterfaceAreaMM2() const {
    /* 1.11.40: IO area, wired in at 1.11.58 (see the D011 note above).
     *
     * 1.11.60 (audit round 4, C009): TWO ZEROS, ONE RETURN VALUE, AND ONE OF
     * THEM HAD TO BE SAID OUT LOUD.
     *
     * This returned 0.0 for "CACTI-IO has no exact parameter map for this
     * technology" and 0.0 for "CACTI-IO computed the area and then WITHHELD
     * it because the run is above the polynomial's validity crossover". They
     * are not the same fact. The second is GDDR6's case and only GDDR6's:
     * its bus_freq is rate/2 = 7000 MHz against the 3162 MHz crossover, while
     * its exact_map IS true (POD135 electricals, sourced), so it receives the
     * driver+PHY ENERGY term from this very call and loses the AREA from it.
     * The consumer's gate is `if (io_area > 0.0)`, so it printed nothing at
     * all -- and a GDDR6 RANK or HOST_MC cell's system-comparable area total
     * omitted the DQ interface silicon with no line saying so, beside a DDR4
     * cell that adds 2.444 mm^2/die and prints one. The correction 1.11.58
     * landed to stop the interface being invisible was invisible again for
     * one technology.
     *
     * It cannot be repaired by returning a number: the polynomial genuinely
     * has nothing credible to say at 7000 MHz, and substituting the
     * extrapolation is what the withholding exists to prevent. So the zero
     * stands and the run STATES which zero it is, once per wrapper, from
     * here -- the consumer's gate cannot say it, because the consumer never
     * enters the branch. interfaceAreaWithheld() exposes the same distinction
     * to a caller that wants to test rather than read. */
    const double rate = PIMID::CactiIOWrapper::dramRateMTs(dram_type_);
    const int    ndq  = PIMID::CactiIOWrapper::dramChannelWidthBits(dram_type_);
    if (rate <= 0.0 || ndq <= 0) return 0.0;
    PIMID::LinkIOResult io =
        PIMID::CactiIOWrapper::computeDramIO(dram_type_, ndq, rate, 1.0);
    if (!io.valid || !io.exact_map) return 0.0;
    if (io.io_area_withheld) {
        io_area_withheld_ = true;
        if (!warned_io_area_withheld_) {
            warned_io_area_withheld_ = true;
            std::cerr << "[power] WARNING: " << dram_type_
                      << " DQ-interface AREA is WITHHELD, not zero. CACTI-IO's "
                         "area polynomial is valid to 3162 MHz and this "
                         "interface runs at " << (rate / 2.0)
                      << " MHz, where the cubic term dominates and the value "
                         "is an extrapolation. The interface ENERGY from the "
                         "same evaluation is reported normally (the power path "
                         "does not use that polynomial); only the area is "
                         "missing. Any area total for this technology omits "
                         "the DQ interface silicon -- it is incomplete, not "
                         "small. " << io.not_modelled << std::endl;
        }
        return 0.0;
    }
    io_area_withheld_ = false;
    return io.io_area_mm2;
}
double RamulatorWrapper::getRefreshPowerMW() const {
    return Ramulator::pimid_energy::refreshMW(dram_type_);
}
double RamulatorWrapper::getBackgroundPowerMW() const {
    return Ramulator::pimid_energy::backgroundMW(dram_type_);
}
int RamulatorWrapper::getBackgroundUnits(const std::string& device_width,
                                         int ranks_per_channel,
                                         int channels) const {
    return Ramulator::pimid_energy::backgroundUnits(dram_type_, device_width,
                                                    ranks_per_channel, channels);  // 1.11.20 D13 / 1.11.52 A015
}
double RamulatorWrapper::getBackgroundSystemMW(double r_idle, bool pg_enabled,
                                               const std::string& device_width,
                                               int ranks_per_channel,
                                               int channels) const {
    /* 1.11.56 (audit D006): NAME THE UNSOURCED COLUMN WHERE IT GOVERNS A
     * PRINTED NUMBER. pimid_energy.h's header calls the whole IDD table
     * part-number-sourced, but idd2p is not one of the sourced columns: the
     * HBM rows were entered as a "30-40% of IDD2N" rule of thumb (and HBM2's
     * own entry is 41.2%, outside that band), and the rest are rounded
     * figures, not datasheet reads. idd2p is only reachable through the
     * power-down state, so it changes nothing unless power gating is on AND
     * the run measured some idle residency -- which is exactly when the
     * Background line stops being a pure IDD2N/IDD3N number. Say so once,
     * there, instead of leaving the disclosure in a comment in another repo. */
    if (pg_enabled && r_idle > 0.0) {
        static bool warned_idd2p = false;
        if (!warned_idd2p) {
            warned_idd2p = true;
            std::cerr << "[power] NOTE: memory.power_down is on and the run "
                         "measured idle residency, so the Background line for '"
                      << dram_type_ << "' rests on the APPROXIMATE IDD2P "
                         "(precharge power-down) column of pimid_energy.h. That "
                         "column is not part-number-sourced -- the HBM entries "
                         "are a 30-40%-of-IDD2N rule of thumb -- so treat the "
                         "idle share of Background as an assumption, not a "
                         "datasheet value." << std::endl;
        }
    }
    return Ramulator::pimid_energy::backgroundSystemMW(dram_type_, r_idle,
                                                       pg_enabled, device_width,
                                                       ranks_per_channel, channels);
}

void RamulatorWrapper::updateEnergyMetrics() const {
    if (current_cycle_ == last_energy_update_ && last_energy_update_ != 0) {
        return;  // Already updated this cycle (do NOT skip the initial cycle-0 pass)
    }

    // Energy per memory access, from the intensive IDD path in
    // external/ramulator/src/dram/pimid_energy.h (see below).

    /* 1.11.57 (latent D012 + D013): THE TOOL ANSWERS BOTH OF THESE.
     *
     * D013: write energy was read x 1.2 under a comment reading "writes are
     * typically 15-20% higher than read" -- the same unsourced ratio 1.11.5
     * removed from the intensive path when it wired writes to IDD4W. The
     * wrapper already exposes the IDD-derived pair, so the ratio does not have
     * to be asserted: getArrayWriteEnergyNJ()/getArrayReadEnergyNJ() is the
     * ratio JEDEC's own IDD4W/IDD4R implies for this technology (about 1.07 on
     * DDR4's row, not 1.20). Take the pair directly.
     *
     * D012: the leakage literals contradicted the comment above them by 100x.
     * The comment states "DDR4: ~80-100 mW/GB, HBM2: ~50-60 mW/GB" and the
     * code assigned 0.85 and 0.55 mW/GB -- two orders of magnitude apart, with
     * no way to tell which was meant. Neither is measured, and the tool has a
     * real answer: standby power for the whole memory system is exactly what
     * pimid_energy's IDD path computes (backgroundSystemMW), and it is per
     * device, not per gigabyte. Derive the per-GB figure from the sourced
     * IDD2N/IDD3N background instead of choosing between two literals that
     * disagree with each other.
     *
     * Both were invisible because cached_write_energy_ and
     * cached_leakage_power_ leave this class only through getWriteEnergy() and
     * getLeakagePower(), whose callers are DRAMModel::tick() -- never called --
     * and printStats(), which has no callers. */
    const double BYTES_PER_ACCESS = 64.0;
    double read_energy_per_access  = getArrayReadEnergyNJ();
    double write_energy_per_access = getArrayWriteEnergyNJ();
    double capacity_gb = capacity_ / (1024.0 * 1024.0 * 1024.0);

    if (dram_arch_ && read_energy_per_access <= 0.0) {
        /* The IDD path refused (no row for this technology); the architecture
         * object's bank energy is the remaining tool-sourced figure. */
        read_energy_per_access =
            dram_arch_->energy.bank_energy_pJ * BYTES_PER_ACCESS / 1000.0;
        write_energy_per_access = read_energy_per_access;
    }

    /* Background power for the memory system this wrapper describes, divided
     * by its capacity -- a derived mW/GB, with the derivation visible. r_idle
     * = 0 and power gating off: this is the standby floor, not a residency
     * weighted figure, which is what a leakage line means. */
    double leakage_power_mw = getBackgroundSystemMW(0.0, false, device_width_,
                                                    ranks_per_channel_,
                                                    channels_);

    // Calculate total energy
    cached_read_energy_ = total_reads_ * read_energy_per_access;
    cached_write_energy_ = total_writes_ * write_energy_per_access;

    /* 1.11.57 (D012): mW for the memory system as configured. The old form
     * was capacity_gb x a literal mW/GB whose two candidate values differed
     * by 100x; capacity_gb is kept only to report the implied per-GB figure
     * to anyone who wants it. */
    (void)capacity_gb;
    cached_leakage_power_ = leakage_power_mw;

    last_energy_update_ = current_cycle_;
}

void RamulatorWrapper::printStats() const {
    std::cout << "\n=== Ramulator DRAM Statistics ===" << std::endl;
    std::cout << "Total Reads:     " << total_reads_ << std::endl;
    std::cout << "Total Writes:    " << total_writes_ << std::endl;
    /* 1.11.57 (latent D009): the Row Hits / Row Misses / Row Conflicts lines
     * and the hit rate derived from them are gone. They printed three counters
     * that were never incremented, so this block reported "Row Hit Rate: 0%"
     * for every run that had any traffic at all -- a fabricated statistic in a
     * block headed "Statistics". What the wrapper actually knows about row
     * behaviour is the miss fraction the caller measured and handed in, and
     * whether it was measured at all. */
    if (row_miss_frac_ >= 0.0 && row_miss_frac_ <= 1.0) {
        std::cout << "Row Miss Frac:   " << row_miss_frac_
                  << " (measured by the caller)" << std::endl;
    } else {
        std::cout << "Row Miss Frac:   not measured in this run" << std::endl;
    }

    std::cout << "\n=== Energy Metrics ===" << std::endl;
    std::cout << "Read Energy:     " << getReadEnergy() << " nJ" << std::endl;
    std::cout << "Write Energy:    " << getWriteEnergy() << " nJ" << std::endl;
    std::cout << "Activation:      " << getActivationEnergy() << " nJ" << std::endl;
    std::cout << "Precharge:       " << getPrechargeEnergy() << " nJ" << std::endl;
    std::cout << "Refresh:         " << getRefreshEnergy() << " nJ" << std::endl;
    std::cout << "Leakage Power:   " << getLeakagePower() << " mW" << std::endl;
    std::cout << "Total Energy:    " << getTotalEnergy() << " nJ" << std::endl;

    // If Ramulator is active, print its statistics
    if (ramulator_memory_system_) {
        std::cout << "\n=== Ramulator Detailed Statistics ===" << std::endl;
        // Ramulator's finalize() prints statistics
        // We don't call it here to avoid ending simulation
    }
}

void RamulatorWrapper::resetStats() {
    total_reads_ = 0;
    total_writes_ = 0;
    cached_read_energy_ = 0.0;
    cached_write_energy_ = 0.0;
    cached_leakage_power_ = 0.0;
    last_energy_update_ = 0;
    pending_requests_.clear();

    // Reset PIM stats
    if (pim_enabled_ && pim_plugin_) {
        pim_plugin_->resetStats();
    }
}

// ============================================================================
// PIM-Specific Methods
// ============================================================================

void RamulatorWrapper::enablePIMSupport(const std::string& dram_type) {
    if (pim_enabled_) {
        std::cout << "PIM support already enabled!\n";
        return;
    }

    dram_type_ = dram_type;
    pim_enabled_ = true;

    std::cout << "Enabling PIM support for " << dram_type_ << "...\n";

    // Create DRAM architecture based on type
    // 1.11.59 (audit C018): a fresh object carries its factory organization,
    // so nothing is stamped on it yet; the width is re-applied below.
    arch_device_width_bits_ = 0;
    if (dram_type_ == "DDR4") {
        dram_arch_ = pimid::memory::createDDR4_2400_Verified();
        std::cout << "Using DDR4-2400 architecture specs\n";
    } else if (dram_type_ == "DDR5") {
        dram_arch_ = pimid::memory::createDDR5_4800_Verified();
        std::cout << "Using DDR5-4800 architecture specs\n";
        std::cout << "  Key DDR5 features: 16n prefetch, 8 bank groups, dual subchannels\n";
    } else if (dram_type_ == "HBM2") {
        dram_arch_ = pimid::memory::createHBM2_Verified();
        std::cout << "Using HBM2 architecture specs\n";
    } else if (dram_type_ == "HBM3") {
        dram_arch_ = pimid::memory::createHBM3_Verified();
        std::cout << "Using HBM3 architecture specs\n";
        std::cout << "  Key HBM3 features: 4.0 GT/s, 16 pseudo-channels, 512 GB/s peak\n";
    } else {
        std::cerr << "Unknown DRAM type: " << dram_type_ << ", using DDR4\n";
        dram_arch_ = pimid::memory::createDDR4_2400_Verified();
    }

    // 1.11.59 (audit C018): this path builds a NEW architecture object, so the
    // configured device width has to be stamped onto it again.
    applyDeviceWidthToArchitecture();

    // Initialize PIM components
    initializePIMComponents();

    std::cout << "PIM support enabled!\n";
}

void RamulatorWrapper::initializePIMComponents() {
    if (!dram_arch_) {
        std::cerr << "ERROR: DRAM architecture not initialized!\n";
        return;
    }

    // Create bandwidth tracker
    bandwidth_tracker_ = std::make_shared<PIMBandwidthTracker>(dram_arch_);

    // Create PIM controller plugin
    pim_plugin_ = std::make_shared<PIMControllerPlugin>(dram_arch_, dram_type_);

    /* 1.11.57 (latent D019): ASK THE ACCESSORS THAT ARE RIGHT HERE.
     *
     * This block hardcoded "int num_subarrays = 16;  // Typical" and, below,
     * "8  // chips per rank (typical)" -- two organization facts stated as
     * literals a few hundred lines above getSubarraysPerBank() and
     * getChipsPerRank(), which resolve exactly those quantities from the
     * architecture object this function has already dereferenced. The literals
     * are wrong for most of the technologies this wrapper serves: HBM2/HBM3
     * are single-die-per-channel parts, and the subarray count is per
     * technology in the architecture object. It was invisible because
     * initializePIMComponents() runs only from enablePIMSupport(), which has
     * no callers anywhere in the tree -- the PIM plugin, the bandwidth tracker
     * and the internal network are never constructed in a PIMID run. */
    int num_subarrays = static_cast<int>(getSubarraysPerBank());
    int num_bank_groups = dram_arch_->organization.bank_groups_per_chip;
    int num_banks = dram_arch_->organization.banks_per_bank_group * num_bank_groups;
    int chips_per_rank = static_cast<int>(getChipsPerRank());

    bandwidth_tracker_->initialize(
        channels_,
        ranks_per_channel_,
        num_bank_groups,
        num_banks,
        num_subarrays
    );

    pim_plugin_->initialize(
        channels_,
        ranks_per_channel_,
        num_bank_groups,
        num_banks,
        num_subarrays
    );

    // Create internal network
    internal_network_ = createInternalDRAMNetwork(
        dram_type_,
        num_subarrays,
        dram_arch_->organization.banks_per_bank_group,
        num_bank_groups,
        chips_per_rank   // 1.11.57 (latent D019): was the literal 8
    );

    std::cout << "PIM components initialized:\n";
    std::cout << "  Channels: " << channels_ << "\n";
    std::cout << "  Ranks: " << ranks_per_channel_ << "\n";
    std::cout << "  Bank Groups: " << num_bank_groups << "\n";
    std::cout << "  Banks: " << num_banks << "\n";
    std::cout << "  Subarrays: " << num_subarrays << "\n";
    std::cout << "  Chips per rank: " << chips_per_rank << "\n";
}

bool RamulatorWrapper::sendPIM(Address addr, MemoryRequestType type,
                              PIMRequestPayload* pim_payload,
                              std::function<void(Address)> callback) {
    if (!pim_enabled_) {
        std::cerr << "ERROR: PIM support not enabled! Call enablePIMSupport() first.\n";
        return false;
    }

    if (!pim_payload) {
        std::cerr << "ERROR: PIM payload is null!\n";
        return false;
    }

    // Create Ramulator request with PIM payload
    Ramulator::Request req = createPIMRequest(addr, type, pim_payload);

    // Set up callback
    req.callback = [this, addr, callback, pim_payload](Ramulator::Request& completed_req) {
        handleRequestCompletion(completed_req);

        // Calculate total latency including PIM-specific components
        uint64_t total_latency = completed_req.depart - completed_req.arrive;
        total_latency += pim_payload->data_movement_cycles;
        total_latency += pim_payload->network_cycles;

        // Call user callback
        if (callback) {
            callback(addr);
        }

        // Call PIM completion callback
        if (pim_payload->pim_completion_callback) {
            pim_payload->pim_completion_callback();
        }
    };

    // Send to Ramulator
    if (ramulator_memory_system_) {
        bool accepted = ramulator_memory_system_->send(req);
        if (accepted && callback) {
            pending_requests_.push_back({addr, type, current_cycle_, callback});
        }
        return accepted;
    }

    // Fallback
    if (callback) {
        callback(addr);
    }
    return true;
}

Ramulator::Request RamulatorWrapper::createPIMRequest(
    Address addr, MemoryRequestType type, PIMRequestPayload* pim_payload) {

    int req_type = (type == MemoryRequestType::READ) ?
                   Ramulator::Request::Type::Read :
                   Ramulator::Request::Type::Write;

    Ramulator::Request req(static_cast<Ramulator::Addr_t>(addr), req_type);

    // Attach PIM payload
    req.m_payload = static_cast<void*>(pim_payload);

    return req;
}

void RamulatorWrapper::registerPE(PIMGranularity granularity, int pe_id, int target_bank) {
    if (!pim_enabled_) {
        std::cerr << "WARNING: PIM support not enabled!\n";
        return;
    }

    if (pim_plugin_) {
        pim_plugin_->registerPE(granularity, pe_id, target_bank);
    }
}

double RamulatorWrapper::getBandwidthLimit(PIMGranularity granularity) const {
    if (pim_enabled_ && pim_plugin_) {
        return pim_plugin_->getBandwidthLimit(granularity);
    }
    return 0.0;
}

int RamulatorWrapper::getPortBitwidth(PIMGranularity granularity) const {
    if (pim_enabled_ && pim_plugin_) {
        return pim_plugin_->getPortBitwidth(granularity);
    }
    return 0;
}

double RamulatorWrapper::getEffectiveBandwidthPerPE(PIMGranularity granularity,
                                                   int target_id) const {
    if (pim_enabled_ && pim_plugin_) {
        return pim_plugin_->getEffectiveBandwidthPerPE(granularity, target_id);
    }
    return 0.0;
}

// ============================================================================
// Subarray-Level Characteristics
// ============================================================================

double RamulatorWrapper::getTRCD() const {
    // Techs WITHOUT their own spec struct borrow the DDR4 struct as an
    // organization proxy (initialize()), so per-tech timing must take
    // precedence over dram_arch_ here.
    if (dram_type_ == "GDDR6") return 14.8;   // JESD250 16 Gb/s vendor spec
    if (dram_type_ == "LPDDR5") return 18.0;  // JESD209-5 tRCDpb
    if (dram_type_ == "DDR3") return 13.75;   // DDR3-1600K
    if (dram_arch_) {
        return dram_arch_->timing.tRCD_ns;
    }
    return 13.32;  // DDR4-2400 default
}

double RamulatorWrapper::getTCAS() const {
    if (dram_type_ == "GDDR6") return 14.8;   // JESD250
    if (dram_type_ == "LPDDR5") return 18.0;  // JESD209-5
    if (dram_type_ == "DDR3") return 13.75;   // DDR3-1600K CL11
    if (dram_arch_) {
        return dram_arch_->timing.tCAS_ns;
    }
    return 13.32;  // DDR4-2400 default (CL16)
}

double RamulatorWrapper::getTRP() const {
    /* 1.11.46 (L170): per-tech, same precedence as getTRCD/getTRAS -- tRC
     * (= tRAS + tRP) feeds the array-energy activate term. Same sources as
     * getTRAS above. */
    if (dram_type_ == "GDDR6")  return 14.8;   // vendor spec class (single point)
    if (dram_type_ == "LPDDR5") return 18.0;   // Micron LPDDR5-6400 tRPpb
    if (dram_type_ == "DDR3")   return 13.75;  // JESD79-3D, DDR3-1600K
    if (dram_arch_) {
        return dram_arch_->timing.tRP_ns;
    }
    return 13.32;  // DDR4-2400 default
}

double RamulatorWrapper::getTRAS() const {
    /* 1.11.46 (FIX-PRE-FLEET L170): the same per-tech precedence getTRCD has.
     * DDR3/LPDDR5/GDDR6 borrow the DDR4-2400 arch struct as an ORGANIZATION
     * proxy, but its TIMINGS fed the array-energy formulas (idd0*tRC -
     * idd3n*tRAS ...), pricing three technologies' arrays on a fourth's
     * clock. Values: DDR3-1600K from JESD79-3D (in hand, normative);
     * LPDDR5-6400 from the Micron datasheets in hand (tRAS min 42 ns);
     * GDDR6 from the same 16 Gb/s vendor-spec class getTRCD already cites --
     * a single-point source, flagged as such. */
    if (dram_type_ == "GDDR6")  return 28.0;   // vendor spec class (single point)
    if (dram_type_ == "LPDDR5") return 42.0;   // Micron LPDDR5-6400, tRAS min
    if (dram_type_ == "DDR3")   return 35.0;   // JESD79-3D, DDR3-1600K
    if (dram_arch_) {
        return dram_arch_->timing.tRAS_ns;
    }
    return 32.0;  // DDR4-2400 default
}

/* 1.11.57 (latent D020): getTRRD() is DELETED. It returned tRAS/4 under the
 * word "Approximation" -- an invented relation between two JEDEC timings that
 * are independently specified (DDR4-2400's tRRD_S is 3.3 ns against a tRAS of
 * 32 ns, so the quarter rule overstates it by 2.4x) -- and it had no callers
 * anywhere in src/, include/ or tools/: only its own definition and its
 * declaration in the header. Nothing could read the wrong number, which is why
 * it survived four releases of timing work. Deleted rather than corrected,
 * because the correct value is a per-technology JEDEC figure this wrapper does
 * not carry, and a caller who needs tRRD should have it sourced then, not
 * inherit a division that looks like one. */
double RamulatorWrapper::getTRC() const {
    // tRC = tRAS + tRP
    return getTRAS() + getTRP();
}

double RamulatorWrapper::getTBurst() const {
    /* 1.11.46 (L170/L168): burst time is beats/rate -- specification
     * arithmetic, not a table: DDR3-1600 BL8 -> 5.0 ns; LPDDR5-6400 BL16 ->
     * 2.5 ns; GDDR6-14000 BL16 -> 1.14 ns. The DDR4-2400 fallback priced all
     * three at 3.33 ns, and the SPEED BIN now matches the IDD row's part for
     * each technology (L168). */
    /* 1.11.52 (audit D002): DDR4 and DDR5 join the same rule. The list
     * above covered three technologies and left DDR4/DDR5 on the
     * architecture object's tBurst, which is the DDR4-2400 bin (3.33 ns) --
     * while BOTH DDR4 termination paths (CACTI-IO's dramRateMTs and the
     * pimid_energy scheme table) price the same part at 3200 MT/s. One
     * part, two speed bins, and the array/termination split of a single
     * access disagreed by 1.33x. Burst time is beats/rate arithmetic from
     * the SAME rate table the termination path uses, so the two halves of
     * an access can no longer be priced at different rates. */
    if (dram_type_ == "GDDR6")  return 16.0 * 1000.0 / 14000.0;  // 1.143 ns
    if (dram_type_ == "LPDDR5") return 16.0 * 1000.0 / 6400.0;   // 2.5 ns
    if (dram_type_ == "DDR3")   return  8.0 * 1000.0 / 1600.0;   // 5.0 ns
    /* 1.11.52 (D002, resolved at the RATE): DDR4/DDR5 keep the architecture
     * object's burst, because the fix belongs one level up -- the rate table
     * now names the part this tree actually simulates, so array, termination
     * and bandwidth all derive from one number. */
    if (dram_arch_) {
        return dram_arch_->timing.tBurst_ns;
    }
    return 3.33;  // DDR4-2400 default (8-beat burst @ 2400 MT/s)
}

uint32_t RamulatorWrapper::getSubarraysPerBank() const {
    if (dram_arch_) {
        return dram_arch_->organization.subarrays_per_bank;
    }
    return 4;  // Typical default
}

uint32_t RamulatorWrapper::getBanksPerBankGroup() const {
    if (dram_arch_) {
        return dram_arch_->organization.banks_per_bank_group;
    }
    return 4;  // DDR4 default
}

uint32_t RamulatorWrapper::getBankGroupsPerChip() const {
    if (dram_arch_) {
        return dram_arch_->organization.bank_groups_per_chip;
    }
    return 4;  // DDR4 default
}

uint32_t RamulatorWrapper::getChipsPerRank() const {
    if (dram_arch_) {
        return dram_arch_->organization.chips_per_rank;
    }
    return 8;  // x8 DDR4 default
}

uint32_t RamulatorWrapper::getRanksPerChannel() const {
    if (dram_arch_) {
        return dram_arch_->organization.ranks_per_channel;
    }
    return ranks_per_channel_;
}

uint64_t RamulatorWrapper::getSubarraySizeKB() const {
    if (dram_arch_) {
        return dram_arch_->organization.subarray_size_kb;
    }
    return 512;  // 512 KB typical
}

uint64_t RamulatorWrapper::getBankSizeMB() const {
    if (dram_arch_) {
        return dram_arch_->organization.bank_size_mb;
    }
    return 2;  // 2 MB typical for DDR4
}

uint64_t RamulatorWrapper::getChipSizeMB() const {
    if (dram_arch_) {
        return dram_arch_->organization.chip_size_mb;
    }
    return 128;  // 128 MB (1 Gb chip) typical
}

/* 1.11.60 (ONE FABRIC, user requirement: "the table is legit with accurate
 * references to the upstream tools/models"): per-rung provenance, taken from
 * the architecture object's OWN VerifiedValue source strings -- the same
 * struct that carries each width -- never restated here. Rung 2 (bank group)
 * has no upstream field and is the declared x2 interleaving assumption from
 * 1.11.58; rung 6 is derived arithmetic and says so. */
std::string RamulatorWrapper::getLadderRungProvenance(int rung) const {
    auto fmt = [](const memory::VerifiedValue& v) -> std::string {
        const char* st =
            v.status == memory::VerificationStatus::VERIFIED  ? "VERIFIED"  :
            v.status == memory::VerificationStatus::INFERRED  ? "INFERRED"  :
            v.status == memory::VerificationStatus::ESTIMATED ? "ESTIMATED" : "UNKNOWN";
        return std::string(st) + ": " + v.source;
    };
    if (!dram_arch_) return "NO ARCHITECTURE OBJECT: per-technology placeholder table";
    switch (rung) {
        case 0: return fmt(dram_arch_->datapath.gsa_datapath_bits);
        case 1: return fmt(dram_arch_->datapath.bank_serialization_bits);
        case 2: return "ASSERTED: bank serialization x 2, an interleaving "
                       "assumption -- no upstream field exists (declared since "
                       "1.11.58; also in the stated-constant register)";
        case 3: return fmt(dram_arch_->datapath.chip_io_bits);
        case 4: return fmt(dram_arch_->datapath.rank_databus_bits);
        case 5: return fmt(dram_arch_->datapath.channel_databus_bits);
        case 6: return "DERIVED: channel_databus_bits x the technology's "
                       "channel count (arithmetic, not a sourced field)";
    }
    return "unknown rung";
}

int RamulatorWrapper::getSubarrayPortBits() const {
    if (dram_arch_) {
        // DRAMArchitectureV2 uses datapath stages
        return dram_arch_->datapath.gsa_datapath_bits.value_bits;  // GSA width
    }
    return 256;  // DDR4 default (256 bits from GSA)
}

int RamulatorWrapper::getBankPortBits() const {
    if (dram_arch_) {
        return dram_arch_->datapath.bank_serialization_bits.value_bits;
    }
    return 8;  // DDR4 default (NARROW!)
}

int RamulatorWrapper::getBankGroupPortBits() const {
    /* 1.11.57 (latent D020, PROMOTED -- this one is LIVE): the x2 below is an
     * unsourced multiplier, and since 1.11.56 it reaches a reported number.
     * The audit recorded it as latent on the grounds that this accessor had no
     * callers; that stopped being true when buildHierarchy started taking the
     * per-level link ladder from the architecture object (src/main.cpp, the
     * "Hierarchy link ladder from the <tech> architecture object" line), where
     * w[2] is this value. It is left AT ITS PRESENT VALUE deliberately -- a
     * silent change here would move every hierarchy level-2 link width and
     * bandwidth in the corpus -- but it no longer passes as sourced.
     *
     * What it is: the architecture object carries no bank-group datapath
     * field. The only sourced neighbour is the bank serialization width, and
     * the x2 asserts that a bank group's port is twice a bank's. That is a
     * plausible reading of bank-group interleaving and it is not a
     * specification value; nothing in JEDEC fixes a bank-group port width,
     * because a bank group is not an interface boundary. The honest fix is a
     * bank_group_port_bits field in DRAMArchitectureV2 with a source string
     * per technology, which is a change to the architecture objects and to
     * every number they feed. */
    static bool announced = false;
    if (!announced) {
        announced = true;
        std::cerr << "[mem] NOTE: the bank-group port width is bank "
                     "serialization width x 2 -- an UNSOURCED multiplier, not "
                     "a JEDEC value; a bank group is not an interface boundary "
                     "and no specification fixes its port width. It sets "
                     "hierarchy level 2's link width and bandwidth."
                  << std::endl;
    }
    if (dram_arch_) {
        return dram_arch_->datapath.bank_serialization_bits.value_bits * 2;
    }
    return 16;  // DDR4 default (8-bit bank serialization x 2)
}

int RamulatorWrapper::getChipIOBits() const {
    if (dram_arch_) {
        return dram_arch_->datapath.chip_io_bits.value_bits;
    }
    return 8;  // x8 DDR4 default
}

int RamulatorWrapper::getRankDataBits() const {
    if (dram_arch_) {
        return dram_arch_->datapath.rank_databus_bits.value_bits;
    }
    return 64;  // DDR4 default (8 x x8 chips)
}

/* 1.11.57 (audit C007): ONE CHANNEL for every family. The HBM objects used to
 * hold the whole stack in this field, so this accessor answered "the channel"
 * with 8 or 16 channels' worth and the ladder's channel rung sat 8x/16x above
 * the rung beneath it. The aggregate is channel x channels, which is what the
 * system root is for.
 *
 * ONE THING THIS DOES NOT FIX, stated so nobody reads the ladder as finished:
 * the system root (L6) is built by the caller as this width x
 * hierarchy_channels_per_system, which is the multi-DEVICE count and is 1 for
 * a single stack -- the DRAM channel count is hierarchy_dram_channels. With
 * the stack no longer smuggled into the channel rung, L6 is now one channel
 * wide on HBM rather than the stack. Correcting it means multiplying by the
 * DRAM channel count at that site, which is src/main.cpp's to make. */
int RamulatorWrapper::getChannelDataBits() const {
    if (dram_arch_) {
        return dram_arch_->datapath.channel_databus_bits.value_bits;
    }
    return 64;  // DDR4 default
}

/* 1.11.57 (audit C004): WHICH PART is this wrapper's architecture object
 * describing? For DDR3, LPDDR5 and GDDR6 the answer is "DDR4", because no
 * object exists for them and the DDR4-2400 one is read instead. A caller that
 * is about to attribute widths, bandwidths or a link ladder to the technology
 * it asked for can compare this against that technology first. */
std::string RamulatorWrapper::getArchitectureTechnology() const {
    if (dram_arch_) return dram_arch_->technology;
    return "";
}

double RamulatorWrapper::getSubarrayBandwidth() const {
    if (dram_arch_) {
        // Subarray bandwidth limited by GSA width
        double gsa_bits = dram_arch_->datapath.gsa_datapath_bits.value_bits;
        double clock_mhz = dram_arch_->timing.clock_freq_mhz;
        return (gsa_bits / 8.0) * (clock_mhz / 1000.0);  // GB/s
    }
    // Calculate from defaults: 256 bits @ 1.2 GHz
    return (256.0 / 8.0) * 1.2;  // 38.4 GB/s internal
}

/* 1.11.57 (audit C003): these two used to return a stored literal while their
 * five neighbours in this block compute a width times a clock. The ladder in
 * main.cpp reads all seven and back-derives each rung's CLOCK as BW * 8 /
 * width, so a literal that had stopped tracking the core clock did not present
 * as a wrong bandwidth -- it presented as a wrong frequency, and then as a
 * wrong transfer time on every crossing of that rung. Derived at the source
 * now (DRAMArchitectureV2::getBankEffectiveBW), so the two rungs follow a
 * speed-bin change like the rest of the ladder. The no-object fallbacks below
 * are DDR4's own derivation, 8 bits / 8 x 1.2 GHz and its x2. */
double RamulatorWrapper::getBankBandwidth() const {
    if (dram_arch_) {
        // Bank bandwidth limited by the serialization path, at the core clock
        return dram_arch_->getBankEffectiveBW();
    }
    return 1.2;  // 8 bits / 8 x 1.2 GHz, the DDR4-2400 default
}

double RamulatorWrapper::getBankGroupBandwidth() const {
    if (dram_arch_) {
        return dram_arch_->getBankGroupEffectiveBW();
    }
    return 2.4;  // 16 bits / 8 x 1.2 GHz, the DDR4-2400 default
}

double RamulatorWrapper::getChipIOBandwidth() const {
    if (dram_arch_) {
        double io_bits = dram_arch_->datapath.chip_io_bits.value_bits;
        double data_rate = dram_arch_->timing.data_rate_mtps;
        return (io_bits / 8.0) * (data_rate / 1000.0);  // GB/s
    }
    return (8.0 / 8.0) * 2.4;  // 2.4 GB/s @ 2400 MT/s
}

double RamulatorWrapper::getRankBandwidth() const {
    if (dram_arch_) {
        return dram_arch_->getRankBW();
    }
    return (64.0 / 8.0) * 2.4;  // 19.2 GB/s for DDR4-2400
}

double RamulatorWrapper::getChannelBandwidth() const {
    if (dram_arch_) {
        double channel_bits = dram_arch_->datapath.channel_databus_bits.value_bits;
        double data_rate = dram_arch_->timing.data_rate_mtps;
        return (channel_bits / 8.0) * (data_rate / 1000.0);  // GB/s
    }
    return (64.0 / 8.0) * 2.4;  // 19.2 GB/s for DDR4-2400
}

double RamulatorWrapper::getSubarrayEnergyPerByte() const {
    if (dram_arch_) {
        return dram_arch_->energy.subarray_energy_pJ;
    }
    return 1.0;  // 1 pJ/byte DDR4 default
}

double RamulatorWrapper::getBankEnergyPerByte() const {
    if (dram_arch_) {
        return dram_arch_->energy.bank_energy_pJ;
    }
    return 2.0;  // 2 pJ/byte DDR4 default
}

/* 1.11.57 (latent D020): getBankGroupEnergyPerByte() is DELETED. It returned
 * the bank energy x 1.5 as "approximate as slightly higher than bank energy" --
 * a 50% surcharge for crossing a boundary that costs nothing in the energy
 * model, asserted with no source -- and it had no callers: only its definition
 * and its header declaration. The energy ladder that IS consumed
 * (architecture_extractor.h) reads the architecture object's own per-tier
 * fields, never this. A caller who needs bank-group energy should get a
 * sourced field on DRAMArchitectureV2, not a multiplier. */
double RamulatorWrapper::getChipEnergyPerByte() const {
    if (dram_arch_) {
        return dram_arch_->energy.chip_energy_pJ;
    }
    return 5.0;  // 5 pJ/byte DDR4 default
}

double RamulatorWrapper::getRankEnergyPerByte() const {
    if (dram_arch_) {
        return dram_arch_->energy.rank_energy_pJ;
    }
    return 10.0;  // 10 pJ/byte DDR4 default
}

/* 1.11.57 (latent D020): getChannelEnergyPerByte() is DELETED, for the same
 * reason as getBankGroupEnergyPerByte() above -- rank energy x 1.5 as
 * "channel energy includes rank + controller overhead", an unsourced 50%
 * controller allowance, with no callers anywhere in the tree. The memory
 * controller's energy is McPAT's to report, and it is reported there. */

/* 1.11.23: these four were CIRCULAR. Each returned the architecture field
 * that architecture_extractor.h assigns FROM it, so the "primary" branch was
 * a no-op that handed back a hand-written literal, while the real derivation
 * sat unreachable in the dram_arch_==null fallback. The fallback was the
 * correct path all along.
 *
 * Derived now, from the JEDEC timing Ramulator2 already parses and the energy
 * model already consumes -- no literal, no fallback asymmetry:
 *   subarray  tRCD + tCAS            row activate + column access
 *   bank      tRP + tRCD + tCAS      the row-miss path
 *   bankgroup bank                  floor: the real term is tCCD_L - tCCD_S,
 *                                    which this wrapper does not expose
 *   chip      bank + tBurst          the burst leaves through the chip I/O
 *
 * RESIDUAL, stated: the per-tech tRCD/tCAS/tRP values these compose from are
 * JEDEC speed-bin figures TRANSCRIBED into getTRCD()/getTCAS()/getTRP() with
 * citations (JESD250 for GDDR6, JESD209-5 tRCDpb for LPDDR5, DDR3-1600K,
 * DDR4-2400 CL17) rather than read from the Ramulator2 timing preset this
 * wrapper already names ("DDR4_2400R", "DDR3_1600H", ...). Sourced, but
 * transcribed; reading the preset directly is the follow-up. */
double RamulatorWrapper::getSubarrayAccessLatency() const {
    return getTRCD() + getTCAS();
}

double RamulatorWrapper::getBankAccessLatency() const {
    return getTRP() + getTRCD() + getTCAS();
}

double RamulatorWrapper::getBankGroupAccessLatency() const {
    /* The bank-group tier's only real cost is the longer same-group
     * column-to-column delay, tCCD_L - tCCD_S. This wrapper does not expose
     * tCCD, so that term is NOT AVAILABLE -- and the previous code covered
     * the gap with a 1.1 multiplier, i.e. asserted a 10% penalty with no
     * source, charged even to technologies that have no bank groups at all.
     * Report the bank latency, which is the correct FLOOR, rather than
     * manufacture a penalty. Wiring tCCD through is the follow-up. */
    return getBankAccessLatency();
}

double RamulatorWrapper::getChipAccessLatency() const {
    return getBankAccessLatency() + getTBurst();
}

double RamulatorWrapper::getRankAccessLatency() const {
    if (dram_arch_) {
        return dram_arch_->timing.rank_access_ns;
    }
    return 80.0;  // Typical DDR4
}

/* 1.11.57 (latent D020): getChannelAccessLatency() is DELETED. It returned the
 * rank access latency x 1.2 with the comment "Channel adds MC overhead" -- a
 * 20% controller penalty with no source, charged uniformly to every
 * technology -- and it had no callers. Note that getBankGroupAccessLatency()
 * two functions up already REFUSED the same shape of number in 1.11.23,
 * dropping its 1.1x and returning the bank latency as an honest floor with the
 * missing term named (tCCD_L - tCCD_S); this function kept the pattern that
 * one abandoned. The controller's queueing and overhead are modelled in the
 * hierarchy/NoC path and in McPAT, not by a multiplier on a JEDEC timing. */

const pimid::memory::DRAMArchitectureV2* RamulatorWrapper::getDRAMArchitecture() const {
    return dram_arch_.get();
}

} // namespace pimid

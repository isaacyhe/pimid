/** $lic$
 * Copyright (C) 2012-2015 by Massachusetts Institute of Technology
 * Copyright (C) 2010-2013 by The Board of Trustees of Stanford University
 *
 * This file is part of zsim.
 *
 * zsim is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2.
 *
 * If you use this software in your research, we request that you reference
 * the zsim paper ("ZSim: Fast and Accurate Microarchitectural Simulation of
 * Thousand-Core Systems", Sanchez and Kozyrakis, ISCA-40, June 2013) as the
 * source of the simulator in any publications that use this software, and that
 * you send us a citation of your work.
 *
 * zsim is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <sstream>
#include <string.h>
#include <string>
#include <vector>
#include "libconfig.h"  // Use C API instead of C++ for -fno-exceptions compatibility
#include "log.h"

using std::string;
using std::stringstream;
using std::vector;

// Restrict use of long long, which libconfig uses as its int64
typedef long long lc_int64;  // NOLINT(runtime/int)

Config::Config(const char* inFile) {
    inCfg = new config_t();
    outCfg = new config_t();
    config_init(inCfg);
    config_init(outCfg);

    if (config_read_file(inCfg, inFile) != CONFIG_TRUE) {
        const char* errFile = config_error_file(inCfg);
        int errLine = config_error_line(inCfg);
        const char* errText = config_error_text(inCfg);
        config_error_t errType = config_error_type(inCfg);

        if (errType == CONFIG_ERR_FILE_IO) {
            panic("Input config file %s could not be read", inFile);
        } else {
            // Parse error
            const char* peFile = errFile ? errFile : inFile;
            panic("Input config file %s could not be parsed, line %d, error: %s", peFile, errLine, errText);
        }
    }
}

Config::~Config() {
    config_destroy(inCfg);
    config_destroy(outCfg);
    delete inCfg;
    delete outCfg;
}

// Helper function: Add "*"-prefixed vars, which are used by our scripts but not zsim, to outCfg
// Returns number of copied vars
static uint32_t copyNonSimVars(config_setting_t* s1, config_setting_t* s2, std::string prefix) {
    uint32_t copied = 0;
    int len = config_setting_length(s1);
    for (int i = 0; i < len; i++) {
        config_setting_t* elem = config_setting_get_elem(s1, i);
        const char* name = config_setting_name(elem);
        if (name && name[0] == '*') {
            if (config_setting_get_member(s2, name) != NULL) {
                panic("Setting %s was read, should be private", (prefix + name).c_str());
            }
            // Copy by type
            int type = config_setting_type(elem);
            config_setting_t* ns = config_setting_add(s2, name, type);
            if (!ns) panic("Failed to add setting %s", (prefix + name).c_str());

            if      (type == CONFIG_TYPE_INT)     config_setting_set_int(ns, config_setting_get_int(elem));
            else if (type == CONFIG_TYPE_INT64)   config_setting_set_int64(ns, config_setting_get_int64(elem));
            else if (type == CONFIG_TYPE_BOOL)    config_setting_set_bool(ns, config_setting_get_bool(elem));
            else if (type == CONFIG_TYPE_STRING)  config_setting_set_string(ns, config_setting_get_string(elem));
            else panic("Unknown type for priv setting %s, cannot copy", (prefix + name).c_str());
            copied++;
        }

        if (config_setting_is_group(elem)) {
            config_setting_t* s2child = config_setting_get_member(s2, name);
            if (s2child) {
                copied += copyNonSimVars(elem, s2child, prefix + name + ".");
            }
        }
    }
    return copied;
}

// Helper function: Compares two settings recursively, checking for inclusion
// Returns number of settings without inclusion (given but unused)
static uint32_t checkIncluded(config_setting_t* s1, config_setting_t* s2, std::string prefix) {
    uint32_t unused = 0;
    int len = config_setting_length(s1);
    for (int i = 0; i < len; i++) {
        config_setting_t* elem = config_setting_get_elem(s1, i);
        const char* name = config_setting_name(elem);
        if (!name) continue;

        config_setting_t* s2child = config_setting_get_member(s2, name);
        if (!s2child) {
            warn("Setting %s not used during configuration", (prefix + name).c_str());
            unused++;
        } else if (config_setting_is_group(elem)) {
            unused += checkIncluded(elem, s2child, prefix + name + ".");
        }
    }
    return unused;
}



//Called when initialization ends. Writes output config, and emits warnings for unused input settings
void Config::writeAndClose(const char* outFile, bool strictCheck) {
    uint32_t nonSimVars = copyNonSimVars(config_root_setting(inCfg), config_root_setting(outCfg), std::string(""));
    uint32_t unused = checkIncluded(config_root_setting(inCfg), config_root_setting(outCfg), std::string(""));

    if (nonSimVars) info("Copied %d non-sim var%s to output config", nonSimVars, (nonSimVars > 1)? "s" : "");
    if (unused) {
        if (strictCheck) {
            panic("%d setting%s not used during configuration", unused, (unused > 1)? "s" : "");
        } else {
            warn("%d setting%s not used during configuration", unused, (unused > 1)? "s" : "");
        }
    }

    if (config_write_file(outCfg, outFile) != CONFIG_TRUE) {
        panic("Output config file %s could not be written", outFile);
    }
}


bool Config::exists(const char* key) {
    return config_lookup(inCfg, key) != NULL;
}

//Helper functions
template<typename T> static const char* getTypeName();
template<> const char* getTypeName<int>() {return "uint32";}
template<> const char* getTypeName<lc_int64>() {return "uint64";}
template<> const char* getTypeName<bool>() {return "bool";}
template<> const char* getTypeName<const char*>() {return "string";}
template<> const char* getTypeName<double>() {return "double";}

// C API setting types
template<typename T> static int getSType();
template<> int getSType<int>() {return CONFIG_TYPE_INT;}
template<> int getSType<lc_int64>() {return CONFIG_TYPE_INT64;}
template<> int getSType<bool>() {return CONFIG_TYPE_BOOL;}
template<> int getSType<const char*>() {return CONFIG_TYPE_STRING;}
template<> int getSType<double>() {return CONFIG_TYPE_FLOAT;}

template<typename T> static bool getEq(T v1, T v2);
template<> bool getEq<int>(int v1, int v2) {return v1 == v2;}
template<> bool getEq<lc_int64>(lc_int64 v1, lc_int64 v2) {return v1 == v2;}
template<> bool getEq<bool>(bool v1, bool v2) {return v1 == v2;}
template<> bool getEq<const char*>(const char* v1, const char* v2) {return strcmp(v1, v2) == 0;}
template<> bool getEq<double>(double v1, double v2) {return v1 == v2;}

// Helper to set a value on a config_setting_t based on type
template<typename T> static void setSettingValue(config_setting_t* s, T val);
template<> void setSettingValue<int>(config_setting_t* s, int val) { config_setting_set_int(s, val); }
template<> void setSettingValue<lc_int64>(config_setting_t* s, lc_int64 val) { config_setting_set_int64(s, val); }
template<> void setSettingValue<bool>(config_setting_t* s, bool val) { config_setting_set_bool(s, val ? CONFIG_TRUE : CONFIG_FALSE); }
template<> void setSettingValue<const char*>(config_setting_t* s, const char* val) { config_setting_set_string(s, val); }
template<> void setSettingValue<double>(config_setting_t* s, double val) { config_setting_set_float(s, val); }

// Helper to get a value from a config_setting_t based on type
template<typename T> static T getSettingValue(config_setting_t* s);
template<> int getSettingValue<int>(config_setting_t* s) { return config_setting_get_int(s); }
template<> lc_int64 getSettingValue<lc_int64>(config_setting_t* s) { return config_setting_get_int64(s); }
template<> bool getSettingValue<bool>(config_setting_t* s) { return config_setting_get_bool(s) == CONFIG_TRUE; }
template<> const char* getSettingValue<const char*>(config_setting_t* s) { return config_setting_get_string(s); }
template<> double getSettingValue<double>(config_setting_t* s) { return config_setting_get_float(s); }

template<typename T> static void writeVar(config_setting_t* setting, const char* key, T val) {
    //info("writeVal %s", key);
    const char* sep = strchr(key, '.');
    if (sep) {
        assert(*sep == '.');
        uint32_t plen = (size_t)(sep-key);
        char prefix[plen+1];
        strncpy(prefix, key, plen);
        prefix[plen] = 0;
        // libconfig strdups all passed strings, so it's fine that prefix is local.
        config_setting_t* child = config_setting_get_member(setting, prefix);
        if (!child) {
            child = config_setting_add(setting, prefix, CONFIG_TYPE_GROUP);
            if (!child) {
                panic("libconfig error adding group setting %s", prefix);
            }
        }
        writeVar(child, sep+1, val);
    } else {
        config_setting_t* existing = config_setting_get_member(setting, key);
        if (!existing) {
            config_setting_t* ns = config_setting_add(setting, key, getSType<T>());
            if (!ns) {
                panic("libconfig error adding leaf setting %s", key);
            }
            setSettingValue(ns, val);
        } else {
            //If this panics, what the hell are you doing in the code? Multiple reads and different defaults??
            T origVal = getSettingValue<T>(existing);
            if (!getEq(val, origVal)) panic("Duplicate writes to out config key %s with different values!", key);
        }
    }
}

template<typename T> static void writeVar(config_t* cfg, const char* key, T val) {
    config_setting_t* setting = config_root_setting(cfg);
    writeVar(setting, key, val);
}


// Helper to check if the setting type matches expected type
template<typename T> static bool checkSettingType(config_setting_t* s);
template<> bool checkSettingType<int>(config_setting_t* s) {
    int t = config_setting_type(s);
    return t == CONFIG_TYPE_INT || t == CONFIG_TYPE_INT64;
}
template<> bool checkSettingType<lc_int64>(config_setting_t* s) {
    int t = config_setting_type(s);
    return t == CONFIG_TYPE_INT || t == CONFIG_TYPE_INT64;
}
template<> bool checkSettingType<bool>(config_setting_t* s) {
    return config_setting_type(s) == CONFIG_TYPE_BOOL;
}
template<> bool checkSettingType<const char*>(config_setting_t* s) {
    return config_setting_type(s) == CONFIG_TYPE_STRING;
}
template<> bool checkSettingType<double>(config_setting_t* s) {
    int t = config_setting_type(s);
    return t == CONFIG_TYPE_FLOAT || t == CONFIG_TYPE_INT || t == CONFIG_TYPE_INT64;
}

template<typename T>
T Config::genericGet(const char* key, T def) {
    T val;
    config_setting_t* setting = config_lookup(inCfg, key);
    if (setting) {
        if (!checkSettingType<T>(setting)) {
            panic("Type error on optional setting %s, expected type %s", key, getTypeName<T>());
        }
        val = getSettingValue<T>(setting);
    } else {
        val = def;
    }
    writeVar(outCfg, key, val);
    return val;
}

template<typename T>
T Config::genericGet(const char* key) {
    T val;
    config_setting_t* setting = config_lookup(inCfg, key);
    if (setting) {
        if (!checkSettingType<T>(setting)) {
            panic("Type error on mandatory setting %s, expected type %s", key, getTypeName<T>());
        }
        val = getSettingValue<T>(setting);
    } else {
        panic("Mandatory setting %s (%s) not found", key, getTypeName<T>())
    }
    writeVar(outCfg, key, val);
    return val;
}

//Template specializations for access interface
template<> uint32_t Config::get<uint32_t>(const char* key) {return (uint32_t) genericGet<int>(key);}
template<> uint64_t Config::get<uint64_t>(const char* key) {return (uint64_t) genericGet<lc_int64>(key);}
template<> bool Config::get<bool>(const char* key) {return genericGet<bool>(key);}
template<> const char* Config::get<const char*>(const char* key) {return genericGet<const char*>(key);}
template<> double Config::get<double>(const char* key) {return (double) genericGet<double>(key);}

template<> uint32_t Config::get<uint32_t>(const char* key, uint32_t def) {return (uint32_t) genericGet<int>(key, (int)def);}
template<> uint64_t Config::get<uint64_t>(const char* key, uint64_t def) {return (uint64_t) genericGet<lc_int64>(key, (lc_int64)def);}
template<> bool Config::get<bool>(const char* key, bool def) {return genericGet<bool>(key, def);}
template<> const char* Config::get<const char*>(const char* key, const char* def) {return genericGet<const char*>(key, def);}
template<> double Config::get<double>(const char* key, double def) {return (double) genericGet<double>(key, (double)def);}

//Get subgroups in a specific key
void Config::subgroups(const char* key, std::vector<const char*>& grps) {
    config_setting_t* s = config_lookup(inCfg, key);
    if (s) {
        int n = config_setting_length(s); //0 if not a group or list
        for (int i = 0; i < n; i++) {
            config_setting_t* elem = config_setting_get_elem(s, i);
            if (config_setting_is_group(elem)) {
                grps.push_back(config_setting_name(elem));
            }
        }
    }
}


/* Config value parsing functions */

//Range parsing, for process masks

//Helper, from http://oopweb.com/CPP/Documents/CPPHOWTO/Volume/C++Programming-HOWTO-7.html
void Tokenize(const string& str, vector<string>& tokens, const string& delimiters) {
    // Skip delimiters at beginning.
    string::size_type lastPos = 0; //dsm: DON'T //str.find_first_not_of(delimiters, 0);
    // Find first "non-delimiter".
    string::size_type pos = str.find_first_of(delimiters, lastPos);

    while (string::npos != pos || string::npos != lastPos) {
        // Found a token, add it to the vector.
        tokens.push_back(str.substr(lastPos, pos - lastPos));
        // Skip delimiters.  Note the "not_of"
        lastPos = str.find_first_not_of(delimiters, pos);
        // Find next "non-delimiter"
        pos = str.find_first_of(delimiters, lastPos);
    }
}

struct Range {
    int32_t min;
    int32_t sup;
    int32_t step;

    explicit Range(string r)  {
        vector<string> t;
        Tokenize(r, t, ":");
        vector<uint32_t> n;
        for (auto s : t) {
            stringstream ss(s);
            uint32_t x = 0;
            ss >> x;
            if (ss.fail()) panic("%s in range %s is not a valid number", s.c_str(), r.c_str());
            n.push_back(x);
        }
        switch (n.size()) {
            case 1:
                min = n[0];
                sup = min + 1;
                step = 1;
                break;
            case 2:
                min = n[0];
                sup = n[1];
                step = 1;
                break;
            case 3:
                min = n[0];
                sup = n[1];
                step = n[2];
                break;
            default:
                panic("Range '%s' can only have 1-3 numbers delimited by ':', %ld parsed", r.c_str(), n.size());
        }

        //Final error-checking
        if (min < 0 || step < 0 || sup < 0) panic("Range %s has negative numbers", r.c_str());
        if (step == 0) panic("Range %s has 0 step!", r.c_str());
        if (min >= sup) panic("Range %s has min >= sup!", r.c_str());
    }

    void fill(vector<bool>& mask) {
        for (int32_t i = min; i < sup; i += step) {
            if (i >= (int32_t)mask.size() || i < 0) panic("Range %d:%d:%d includes out-of-bounds %d (mask limit %ld)", min, step, sup, i, mask.size()-1);
            mask[i] = true;
        }
    }
};

std::vector<bool> ParseMask(const std::string& maskStr, uint32_t maskSize) {
    vector<bool> mask;
    mask.resize(maskSize);

    vector<string> ranges;
    Tokenize(maskStr, ranges, " ");
    for (auto r : ranges) {
        if (r.length() == 0) continue;
        Range range(r);
        range.fill(mask);
    }
    return mask;
}

//List parsing
template <typename T>
std::vector<T> ParseList(const std::string& listStr, const char* delimiters) {
    vector<string> nums;
    Tokenize(listStr, nums, delimiters);

    vector<T> res;
    for (auto n : nums) {
        if (n.length() == 0) continue;
        stringstream ss(n);
        T x;
        ss >> x;
        if (ss.fail()) panic("%s in list [%s] could not be parsed", n.c_str(), listStr.c_str());
        res.push_back(x);
    }
    return res;
}

//Instantiations
template std::vector<uint32_t> ParseList<uint32_t>(const std::string& listStr, const char* delimiters);
template std::vector<uint64_t> ParseList<uint64_t>(const std::string& listStr, const char* delimiters);
template std::vector<std::string> ParseList(const std::string& listStr, const char* delimiters);

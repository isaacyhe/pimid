/**
 * NVSim global variable definitions for library use.
 * These are normally defined in main.cpp but we exclude main.cpp from the library build.
 */

#include "InputParameter.h"
#include "Technology.h"
#include "MemCell.h"
#include "Wire.h"

// Global variables required by NVSim internal modules
InputParameter *inputParameter = nullptr;
Technology *tech = nullptr;
MemCell *cell = nullptr;
Wire *localWire = nullptr;
Wire *globalWire = nullptr;

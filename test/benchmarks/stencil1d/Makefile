# Stencil1d Workload Makefile
# Builds message passing and shared memory versions

CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra

# Paths
PIMID_ROOT = ../../../..
DAC26_ADAPTER = $(PIMID_ROOT)/DAC26/pimid_adapter
PIMID_INCLUDE = $(PIMID_ROOT)/pimid/include
POWER_MODELS = $(PIMID_ROOT)/pimid/power_models
MEMORY_MODELS = $(PIMID_ROOT)/pimid/memory_models

INCLUDES = -I$(DAC26_ADAPTER) \
           -I$(PIMID_INCLUDE) \
           -I$(POWER_MODELS)/include \
           -I$(MEMORY_MODELS)/include

LIBS = -lpthread -lm

# Source files
SIMULATOR_SRC = $(DAC26_ADAPTER)/pim_simulator.cpp
CACTI_SRC = $(MEMORY_MODELS)/src/cacti_wrapper.cpp
MCPAT_SRC = $(POWER_MODELS)/src/mcpat_model.cpp
POWER_SRC = $(POWER_MODELS)/src/power_model.cpp

COMMON_SRCS = $(SIMULATOR_SRC) $(CACTI_SRC) $(MCPAT_SRC) $(POWER_SRC)

# Targets
TARGET_MESSAGE = stencil1d_message
TARGET_SHARED = stencil1d_shared

.PHONY: all message shared clean help

all: message shared

message: $(TARGET_MESSAGE)

$(TARGET_MESSAGE): stencil1d_message.cpp $(COMMON_SRCS)
	@echo "Building $@..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(COMMON_SRCS) $(LIBS) -o $@

shared: $(TARGET_SHARED)

$(TARGET_SHARED): stencil1d_shared.cpp $(COMMON_SRCS)
	@echo "Building $@..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(COMMON_SRCS) $(LIBS) -o $@

clean:
	rm -f $(TARGET_MESSAGE) $(TARGET_SHARED) *.o

help:
	@echo "Stencil1d Workload Build System"
	@echo ""
	@echo "Targets:"
	@echo "  make             Build both versions (default)"
	@echo "  make message     Build message passing version"
	@echo "  make shared      Build shared memory version"
	@echo "  make clean       Remove all built files"
	@echo ""
	@echo "Usage:"
	@echo "  ./stencil1d_message <num_subarrays> <problem_size> <is_libcom>"
	@echo "  ./stencil1d_shared <num_subarrays> <problem_size> <is_libcom>"
	@echo ""
	@echo "Examples:"
	@echo "  ./stencil1d_message 8 1024 0    # 8 subarrays, H-tree"
	@echo "  ./stencil1d_shared 8 1024 1     # 8 subarrays, LIBCom"

# Co-simulation workloads: serial host + parallel device, three device-side
# data models {serial, shared_memory, message_passing}. See README.md.
CXX      ?= g++
HOOKS    := ../../external/zsim/misc/hooks
CXXFLAGS := -std=c++11 -O2 -Wall -I$(HOOKS)
LDFLAGS  := -lpthread

KERNELS  := reduction_tree spmv_csr histogram_merge bfs_iterative vector_add
VARIANTS := serial shared_memory message_passing
TARGETS  := $(foreach k,$(KERNELS),$(foreach v,$(VARIANTS),$(k)/$(k)_$(v)))

all: $(TARGETS)

# pattern: <dir>/<dir>_<variant> from <dir>/<dir>_<variant>.cpp
%: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGETS)

.PHONY: all clean

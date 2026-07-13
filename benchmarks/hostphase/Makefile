# Co-simulation has NO special workloads: any ordinary workload (e.g.
# benchmarks/pim_kernels/*) runs in co-sim -- its ROI region executes on the
# device, its out-of-ROI code on the host, transfers are charged on the link.
#
# This directory only keeps the host-phase programs used by the composed
# cross-check harness (examples/figures/co_sim.sh).
CXX      ?= g++
HOOKS    := ../../external/zsim/misc/hooks
CXXFLAGS := -std=c++11 -O2 -Wall -I$(HOOKS)
LDFLAGS  := -lpthread

HOSTPHASE := $(foreach k,reduction histogram spmv stencil stream gemv bfs,$(k)_hostphase)

all: $(HOSTPHASE)

%: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(HOSTPHASE)

.PHONY: all clean

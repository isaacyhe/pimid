ZSIM_HOOKS := ../../external/zsim/misc/hooks
CXX := g++
CXXFLAGS := -std=c++17 -O2 -fopenmp -I$(ZSIM_HOOKS)

babelstream: babelstream.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f babelstream

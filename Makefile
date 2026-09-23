CXX = g++
CXXFLAGS = -Wall -Wextra -Wpedantic -std=c++11 -O3

all: rummikub

rummikub: src/solver.cc
	$(CXX) $(CXXFLAGS) -o $@ $<

test: rummikub
	./rummikub examples/small.in | diff - examples/small.out && echo "OK: examples/small.in"
	python3 scripts/group_value.py > /dev/null && echo "OK: group value formula"

clean:
	rm -f rummikub

.PHONY: all test clean

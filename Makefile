CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -g

server: server.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

run: server
	./server

clean:
	rm -f server

.PHONY: run clean

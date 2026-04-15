CXX ?= g++
CXXFLAGS ?= -std=c++11 -Wall -Wextra -Wpedantic -O2 -pthread
LDFLAGS ?= -pthread

all: quill-runtime.o libquill-runtime.so iterative_averaging nqueens

test: runtime_smoke
	QUILL_WORKERS=4 ./runtime_smoke

quill-runtime.o: quill-runtime.cpp
	$(CXX) $(CXXFLAGS) -fPIC -c quill-runtime.cpp

libquill-runtime.so: quill-runtime.o
	$(CXX) -shared -o libquill-runtime.so quill-runtime.o $(LDFLAGS)

iterative_averaging: iterative_averaging.cpp quill-runtime.o
	$(CXX) $(CXXFLAGS) -o iterative_averaging iterative_averaging.cpp quill-runtime.o $(LDFLAGS)

nqueens: nqueens.cpp quill-runtime.o
	$(CXX) $(CXXFLAGS) -o nqueens nqueens.cpp quill-runtime.o $(LDFLAGS)

runtime_smoke: tests/runtime_smoke.cpp quill-runtime.o
	$(CXX) $(CXXFLAGS) -I. -o runtime_smoke tests/runtime_smoke.cpp quill-runtime.o $(LDFLAGS)

clean:
	rm -f iterative_averaging nqueens runtime_smoke quill-runtime.o libquill-runtime.so

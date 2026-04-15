CXX ?= g++
CXXFLAGS ?= -std=c++11 -Wall -Wextra -Wpedantic -O2 -pthread
LDFLAGS ?= -pthread

all: quill-runtime.o libquill-runtime.so nqueens

quill-runtime.o: quill-runtime.cpp
	$(CXX) $(CXXFLAGS) -fPIC -c quill-runtime.cpp

libquill-runtime.so: quill-runtime.o
	$(CXX) -shared -o libquill-runtime.so quill-runtime.o $(LDFLAGS)

nqueens: nqueens.cpp libquill-runtime.so
	$(CXX) $(CXXFLAGS) nqueens.cpp -o nqueens -L. -lquill-runtime $(LDFLAGS)

clean:
	rm -f nqueens quill-runtime.o libquill-runtime.so

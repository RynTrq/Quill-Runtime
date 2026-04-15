#ifndef QUILL_H
#define QUILL_H

#include <cstdint>
#include <cstddef>
#include <functional>
#include <string>

namespace quill {

void init_runtime();
void finalize_runtime();

void start_finish();
void end_finish();

void async(std::function<void()> &&lambda);
void parallel_for(uint64_t lowbound, uint64_t highbound, std::function<void(uint64_t)> loopBody);

void parseNUMAConfig(const std::string& configFile);
void assignWorkersToNUMADomains();

template <typename T>
T* numa_alloc(std::size_t numElements) {
    return numElements == 0 ? nullptr : new T[numElements]();
}

template <typename T>
void numa_dealloc(T* pointer) {
    delete[] pointer;
}

} // namespace quill

#endif

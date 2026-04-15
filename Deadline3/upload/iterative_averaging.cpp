#include "quill.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <sys/time.h>
#include <utility>

namespace {

const uint64_t kDefaultSize = 48ULL * 256ULL * 2048ULL;
const int kDefaultIterations = 64;

double* next_values = nullptr;
double* current_values = nullptr;

long get_usecs() {
    timeval t;
    gettimeofday(&t, nullptr);
    return t.tv_sec * 1000000L + t.tv_usec;
}

uint64_t parse_positive_u64(const char* value, uint64_t fallback) {
    if (value == nullptr || *value == '\0') {
        return fallback;
    }

    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(value, &end, 10);
    return (*end == '\0' && parsed > 0) ? static_cast<uint64_t>(parsed) : fallback;
}

int parse_positive_int(const char* value, int fallback) {
    if (value == nullptr || *value == '\0') {
        return fallback;
    }

    char* end = nullptr;
    const long parsed = std::strtol(value, &end, 10);
    return (*end == '\0' && parsed > 0) ? static_cast<int>(parsed) : fallback;
}

void run_parallel(uint64_t size, int iterations) {
    for (int iteration = 0; iteration < iterations; ++iteration) {
        quill::start_finish();
        quill::parallel_for(1, size + 1, [size](uint64_t j) {
            if (j > 0 && j < size + 1) {
                next_values[j] = (current_values[j - 1] + current_values[j + 1]) / 2.0;
            }
        });
        quill::end_finish();
        std::swap(next_values, current_values);
    }
}

} // namespace

int main(int argc, char** argv) {
    const uint64_t size = argc > 1 ? parse_positive_u64(argv[1], kDefaultSize) : kDefaultSize;
    const int iterations = argc > 2 ? parse_positive_int(argv[2], kDefaultIterations) : kDefaultIterations;

    quill::init_runtime();

    next_values = quill::numa_alloc<double>(size + 2);
    current_values = quill::numa_alloc<double>(size + 2);
    if (next_values == nullptr || current_values == nullptr) {
        quill::numa_dealloc(next_values);
        quill::numa_dealloc(current_values);
        quill::finalize_runtime();
        std::cerr << "Failed to allocate averaging buffers" << std::endl;
        return 1;
    }

    std::memset(next_values, 0, sizeof(double) * static_cast<std::size_t>(size + 2));
    std::memset(current_values, 0, sizeof(double) * static_cast<std::size_t>(size + 2));
    current_values[size + 1] = 1.0;

    const long start = get_usecs();
    try {
        run_parallel(size, iterations);
    } catch (const std::exception& error) {
        quill::numa_dealloc(next_values);
        quill::numa_dealloc(current_values);
        quill::finalize_runtime();
        std::cerr << error.what() << std::endl;
        return 1;
    }
    const long end = get_usecs();

    std::cout << "IterativeAveraging(size=" << size << ", iterations=" << iterations
              << ") Time = " << static_cast<double>(end - start) / 1000000.0
              << " sec" << std::endl;

    quill::numa_dealloc(next_values);
    quill::numa_dealloc(current_values);
    quill::finalize_runtime();
    return 0;
}

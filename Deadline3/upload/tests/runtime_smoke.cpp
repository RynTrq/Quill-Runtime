#include "quill.h"

#include <atomic>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_async_finish() {
    std::atomic<int> total(0);

    quill::start_finish();
    for (int i = 0; i < 1000; ++i) {
        quill::async([&total]() {
            total.fetch_add(1, std::memory_order_relaxed);
        });
    }
    quill::end_finish();

    require(total.load(std::memory_order_relaxed) == 1000, "async/finish lost work");
}

void test_nested_async() {
    std::atomic<int> total(0);

    quill::start_finish();
    quill::async([&total]() {
        total.fetch_add(1, std::memory_order_relaxed);
        for (int i = 0; i < 50; ++i) {
            quill::async([&total]() {
                total.fetch_add(1, std::memory_order_relaxed);
            });
        }
    });
    quill::end_finish();

    require(total.load(std::memory_order_relaxed) == 51, "nested async did not finish all descendants");
}

void test_parallel_for() {
    std::atomic<uint64_t> sum(0);
    const uint64_t n = 10000;

    quill::start_finish();
    quill::parallel_for(0, n, [&sum](uint64_t i) {
        sum.fetch_add(i, std::memory_order_relaxed);
    });
    quill::end_finish();

    require(sum.load(std::memory_order_relaxed) == (n - 1) * n / 2, "parallel_for produced the wrong sum");
}

void test_exception_propagation() {
    bool caught = false;

    quill::start_finish();
    quill::async([]() {
        throw std::runtime_error("expected smoke-test failure");
    });

    try {
        quill::end_finish();
    } catch (const std::runtime_error&) {
        caught = true;
    }

    require(caught, "task exceptions were not rethrown by end_finish");
}

} // namespace

int main() {
    quill::init_runtime();

    test_async_finish();
    test_nested_async();
    test_parallel_for();
    test_exception_propagation();

    quill::finalize_runtime();
    std::cout << "runtime_smoke: OK" << std::endl;
    return 0;
}

#include "quill.h"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <sys/time.h>
#include <vector>

namespace {

const int kSolutions[] = {
    1, 0, 0, 2, 10, 4, 40, 92,
    352, 724, 2680, 14200, 73712, 365596,
    2279184, 14772512
};

std::atomic<int> solutions_found(0);

long get_usecs() {
    timeval t;
    gettimeofday(&t, nullptr);
    return t.tv_sec * 1000000L + t.tv_usec;
}

bool has_conflict(const std::vector<int>& positions) {
    const int depth = static_cast<int>(positions.size());
    for (int row = 0; row < depth; ++row) {
        const int column = positions[static_cast<std::size_t>(row)];
        for (int other = row + 1; other < depth; ++other) {
            const int other_column = positions[static_cast<std::size_t>(other)];
            const int row_delta = other - row;
            if (column == other_column ||
                column == other_column - row_delta ||
                column == other_column + row_delta) {
                return true;
            }
        }
    }
    return false;
}

void nqueens_kernel(std::vector<int> positions, int size) {
    if (static_cast<int>(positions.size()) == size) {
        solutions_found.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    for (int column = 0; column < size; ++column) {
        std::vector<int> next = positions;
        next.push_back(column);
        if (!has_conflict(next)) {
            quill::async([next, size]() {
                nqueens_kernel(next, size);
            });
        }
    }
}

void verify_queens(int size) {
    if (size < 1 || size > static_cast<int>(sizeof(kSolutions) / sizeof(kSolutions[0]))) {
        std::cout << "No reference solution count for N=" << size << std::endl;
        return;
    }

    const int expected = kSolutions[size - 1];
    const int actual = solutions_found.load(std::memory_order_relaxed);
    if (actual != expected) {
        throw std::runtime_error("Incorrect answer: expected " + std::to_string(expected) +
                                 ", got " + std::to_string(actual));
    }
    std::cout << "OK" << std::endl;
}

} // namespace

int main(int argc, char* argv[]) {
    int n = 11;
    if (argc > 1) {
        n = std::atoi(argv[1]);
    }
    if (n < 1) {
        std::cerr << "Usage: " << argv[0] << " [positive-board-size]" << std::endl;
        return 1;
    }

    quill::init_runtime();
    solutions_found.store(0, std::memory_order_relaxed);

    const long start = get_usecs();
    try {
        quill::start_finish();
        nqueens_kernel(std::vector<int>(), n);
        quill::end_finish();

        const long end = get_usecs();
        verify_queens(n);
        std::cout << "NQueens(" << n << ") Time = "
                  << static_cast<double>(end - start) / 1000000.0
                  << " sec" << std::endl;
    } catch (const std::exception& error) {
        quill::finalize_runtime();
        std::cerr << error.what() << std::endl;
        return 1;
    }

    quill::finalize_runtime();
    return 0;
}

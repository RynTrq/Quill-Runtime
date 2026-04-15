#include "quill.h"
#include "quill-runtime.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

constexpr int kDefaultDequeCapacity = 4096;
thread_local int worker_id = 0;

struct WorkerQueue {
    explicit WorkerQueue(std::size_t capacity) : capacity(capacity) {}

    bool push(std::function<void()> task) {
        std::lock_guard<std::mutex> guard(mutex);
        if (tasks.size() >= capacity) {
            return false;
        }
        tasks.push_back(std::move(task));
        return true;
    }

    bool pop(std::function<void()>& task) {
        std::lock_guard<std::mutex> guard(mutex);
        if (tasks.empty()) {
            return false;
        }
        task = std::move(tasks.back());
        tasks.pop_back();
        return true;
    }

    bool steal(std::function<void()>& task) {
        std::lock_guard<std::mutex> guard(mutex);
        if (tasks.empty()) {
            return false;
        }
        task = std::move(tasks.front());
        tasks.pop_front();
        return true;
    }

    std::mutex mutex;
    std::deque<std::function<void()>> tasks;
    std::size_t capacity;
};

std::vector<std::thread> workers;
std::vector<std::unique_ptr<WorkerQueue>> queues;
std::vector<int> worker_domains;
std::vector<int> configured_hierarchy;

std::atomic<bool> shutting_down(false);
std::atomic<int> pending_tasks(0);
std::atomic<int> next_submitter(0);
std::atomic<int> rr_victim(0);

std::mutex runtime_mutex;
std::condition_variable work_available;
std::condition_variable finish_complete;
std::exception_ptr first_task_exception;

bool parse_int(const char* value, int& parsed) {
    if (value == nullptr || *value == '\0') {
        return false;
    }

    char* end = nullptr;
    const long result = std::strtol(value, &end, 10);
    if (*end != '\0' || result <= 0 || result > 1000000) {
        return false;
    }

    parsed = static_cast<int>(result);
    return true;
}

int clamp_worker_count(int requested) {
    const unsigned int hardware = std::max(1u, std::thread::hardware_concurrency());
    const int upper_bound = static_cast<int>(std::min<unsigned int>(hardware * 4u, 256u));
    return std::min(std::max(1, requested), upper_bound);
}

int current_worker() {
    if (worker_id >= 0 && worker_id < static_cast<int>(queues.size())) {
        return worker_id;
    }
    return next_submitter.fetch_add(1, std::memory_order_relaxed) % std::max(1, num_workers);
}

void record_exception(std::exception_ptr exception) {
    std::lock_guard<std::mutex> guard(runtime_mutex);
    if (!first_task_exception) {
        first_task_exception = exception;
    }
}

bool steal_from_same_domain(int thief, std::function<void()>& task) {
    if (worker_domains.empty()) {
        return false;
    }

    const int thief_domain = worker_domains[static_cast<std::size_t>(thief)];
    const int start = rr_victim.fetch_add(1, std::memory_order_relaxed);
    for (int offset = 0; offset < num_workers; ++offset) {
        const int victim = (start + offset) % num_workers;
        if (victim == thief || worker_domains[static_cast<std::size_t>(victim)] != thief_domain) {
            continue;
        }
        if (queues[static_cast<std::size_t>(victim)]->steal(task)) {
            return true;
        }
    }
    return false;
}

bool get_task(int id, std::function<void()>& task) {
    if (queues[static_cast<std::size_t>(id)]->pop(task)) {
        return true;
    }

    if (steal_from_same_domain(id, task)) {
        return true;
    }

    const int start = rr_victim.fetch_add(1, std::memory_order_relaxed);
    for (int offset = 0; offset < num_workers; ++offset) {
        const int victim = (start + offset) % num_workers;
        if (victim == id) {
            continue;
        }
        if (queues[static_cast<std::size_t>(victim)]->steal(task)) {
            return true;
        }
    }
    return false;
}

void complete_one_task() {
    const int remaining = pending_tasks.fetch_sub(1, std::memory_order_acq_rel) - 1;
    finish_counter = remaining;
    if (remaining == 0) {
        finish_complete.notify_all();
    }
}

void execute_task(std::function<void()> task) {
    try {
        task();
    } catch (...) {
        record_exception(std::current_exception());
    }
    complete_one_task();
}

bool try_execute_one_task(int id) {
    std::function<void()> task;
    if (!get_task(id, task)) {
        return false;
    }
    execute_task(std::move(task));
    return true;
}

void worker_loop(int id) {
    worker_id = id;
    while (!shutting_down.load(std::memory_order_acquire)) {
        if (try_execute_one_task(id)) {
            continue;
        }

        std::unique_lock<std::mutex> lock(runtime_mutex);
        work_available.wait_for(lock, std::chrono::milliseconds(1), [] {
            return shutting_down.load(std::memory_order_acquire) ||
                   pending_tasks.load(std::memory_order_acquire) > 0;
        });
    }
}

} // namespace

pthread_t threads[MAX_THREADS_SIZE];
volatile bool shutdown = false;
volatile int finish_counter = 0;
pthread_mutex_t finish_lock = PTHREAD_MUTEX_INITIALIZER;
int num_workers = 1;
deque* Deque = nullptr;

int get_num_workers() {
    int requested = 1;
    if (!parse_int(std::getenv("QUILL_WORKERS"), requested)) {
        requested = 1;
    }
    return clamp_worker_count(requested);
}

void communicate(int) {}
void destructor(void*) {}

namespace quill {

void parseNUMAConfig(const std::string& configFile) {
    configured_hierarchy.clear();

    std::ifstream file(configFile.c_str());
    if (!file) {
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        const std::size_t comment = line.find('#');
        if (comment != std::string::npos) {
            line.erase(comment);
        }
        std::replace(line.begin(), line.end(), ':', ' ');

        std::istringstream stream(line);
        int level = 0;
        int count = 0;
        if (stream >> level >> count && level >= 0 && count > 0) {
            if (static_cast<std::size_t>(level) >= configured_hierarchy.size()) {
                configured_hierarchy.resize(static_cast<std::size_t>(level) + 1, 1);
            }
            configured_hierarchy[static_cast<std::size_t>(level)] = count;
        }
    }
}

void assignWorkersToNUMADomains() {
    const int domains = configured_hierarchy.empty() ? 1 : std::max(1, configured_hierarchy[0]);
    worker_domains.assign(static_cast<std::size_t>(num_workers), 0);
    for (int i = 0; i < num_workers; ++i) {
        worker_domains[static_cast<std::size_t>(i)] = i % domains;
    }
}

void init_runtime() {
    if (!queues.empty()) {
        return;
    }

    shutting_down.store(false, std::memory_order_release);
    shutdown = false;
    pending_tasks.store(0, std::memory_order_release);
    finish_counter = 0;
    first_task_exception = nullptr;

    num_workers = get_num_workers();
    if (configured_hierarchy.empty()) {
        parseNUMAConfig("numa_config.txt");
    }
    assignWorkersToNUMADomains();

    queues.clear();
    queues.reserve(static_cast<std::size_t>(num_workers));
    for (int i = 0; i < num_workers; ++i) {
        queues.push_back(std::unique_ptr<WorkerQueue>(new WorkerQueue(kDefaultDequeCapacity)));
    }

    worker_id = 0;
    workers.reserve(static_cast<std::size_t>(std::max(0, num_workers - 1)));
    for (int i = 1; i < num_workers; ++i) {
        workers.push_back(std::thread(worker_loop, i));
    }
}

void start_finish() {
    if (queues.empty()) {
        init_runtime();
    }
    if (pending_tasks.load(std::memory_order_acquire) != 0) {
        throw std::logic_error("quill::start_finish cannot begin while tasks are still pending");
    }
    first_task_exception = nullptr;
    finish_counter = 0;
}

void end_finish() {
    if (queues.empty()) {
        return;
    }

    while (pending_tasks.load(std::memory_order_acquire) > 0) {
        if (!try_execute_one_task(current_worker())) {
            std::unique_lock<std::mutex> lock(runtime_mutex);
            finish_complete.wait_for(lock, std::chrono::milliseconds(1), [] {
                return pending_tasks.load(std::memory_order_acquire) == 0;
            });
        }
    }

    std::exception_ptr exception;
    {
        std::lock_guard<std::mutex> guard(runtime_mutex);
        exception = first_task_exception;
        first_task_exception = nullptr;
    }
    if (exception) {
        std::rethrow_exception(exception);
    }
}

void finalize_runtime() {
    if (queues.empty()) {
        return;
    }

    end_finish();
    shutting_down.store(true, std::memory_order_release);
    shutdown = true;
    work_available.notify_all();

    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers.clear();
    queues.clear();
    worker_domains.clear();
    num_workers = 1;
}

void async(std::function<void()>&& lambda) {
    if (!lambda) {
        return;
    }
    if (queues.empty()) {
        init_runtime();
    }

    pending_tasks.fetch_add(1, std::memory_order_acq_rel);
    finish_counter = pending_tasks.load(std::memory_order_acquire);

    const int owner = current_worker();
    if (!queues[static_cast<std::size_t>(owner)]->push(std::move(lambda))) {
        complete_one_task();
        throw std::runtime_error("quill worker deque capacity exceeded");
    }
    work_available.notify_one();
}

void parallel_for(uint64_t lowbound, uint64_t highbound, std::function<void(uint64_t)> loopBody) {
    if (!loopBody || highbound <= lowbound) {
        return;
    }

    if (queues.empty()) {
        init_runtime();
    }

    const uint64_t total = highbound - lowbound;
    const uint64_t chunk_count = std::min<uint64_t>(total, std::max(1, num_workers * 4));
    const uint64_t chunk_size = (total + chunk_count - 1) / chunk_count;

    for (uint64_t begin = lowbound; begin < highbound; begin += chunk_size) {
        const uint64_t end = std::min(highbound, begin + chunk_size);
        async([begin, end, loopBody]() {
            for (uint64_t i = begin; i < end; ++i) {
                loopBody(i);
            }
        });
    }
}

} // namespace quill

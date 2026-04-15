#include "quill.h"
#include "quill-runtime.h"
#include <pthread.h>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <unistd.h>
#include <functional>
#include <cstring>
#include <ctime>

using namespace std;

#define MAX_THREADS_SIZE 10
#define DEQUE_SIZE 100001

volatile bool shutdown = false;
pthread_t threads[MAX_THREADS_SIZE];
static pthread_key_t id_key;
volatile int finish_counter = 0;
pthread_mutex_t finish_lock;

int num_workers = 1;

struct Worker;
extern Worker* workers;

Worker* workers = nullptr;
bool a[MAX_THREADS_SIZE] = {false}; // status flags
int r[MAX_THREADS_SIZE] = {-1}; // request cells
std::function<void()>* t[MAX_THREADS_SIZE]; // transfer cells

void communicate(int i);

int get_num_workers() {
    const char* workers_str = getenv("QUILL_WORKERS");
    return workers_str ? max(1, atoi(workers_str)) : 1;
}

struct Worker {
    int deque_head;
    int deque_tail;
    std::function<void()> deque_array[DEQUE_SIZE];

    pthread_mutex_t counter_mutex;

    Worker() 
        : deque_head(0), deque_tail(0) {
        pthread_mutex_init(&counter_mutex, nullptr);
    }

    ~Worker() {
        pthread_mutex_destroy(&counter_mutex);
    }

    bool is_empty() const {
        return deque_head == deque_tail;
    }

    bool is_full() const {
        return (deque_head + 1) % DEQUE_SIZE == deque_tail;
    }

    void push(std::function<void()> task) {
        if (is_full()) return;
        deque_array[deque_head] = task;
        deque_head = (deque_head + 1) % DEQUE_SIZE;
    }

    std::function<void()> pop() {
        if (is_empty()) return nullptr;
        deque_head = (deque_head - 1 + DEQUE_SIZE) % DEQUE_SIZE;
        auto task = deque_array[deque_head];
        deque_array[deque_head] = nullptr;
        return task;
    }
};



void destructor(void* value) {
    delete static_cast<int*>(value);
}

void update_status(int i) {
    bool b = !workers[i].is_empty();
    // printf("%d \n", a[i]);

    if (a[i] != b) {
        a[i] = b;
    }
}

void acquire(int i) {
    while (finish_counter != 0) {
        t[i] = nullptr;
        int k = rand() % num_workers;

        // printf("k %d \n", k);
        if (k == i) continue;

        //To check the status flag of Victim and compare swap the request cell of Victim to i (Thief id)
        if (a[k] && __sync_bool_compare_and_swap(&r[k], -1, i)){ 
            // If transfer cells is null 
            // printf("request cell %d and cell id\n",r[k]);

            while (t[i] == nullptr && finish_counter != 0) {
                communicate(i);
            }

            if (t[i] != nullptr) {
                workers[i].push(*t[i]);
                r[i] = -1;
                return;
            }
        }
        // if(shutdown) break;
        communicate(i);
    }
}

void communicate(int i) {
    int j = r[i];
    // No request
    if (j == -1) return;

    if (workers[i].is_empty()) {
        t[j] = nullptr;
    } 
    else {
        t[j] = new std::function<void()>(workers[i].pop());
    }

    r[i] = -1;
}

std::function<void()> pop_task_from_runtime() {
    int* thread_id_ptr = static_cast<int*>(pthread_getspecific(id_key));
    int thread_id = *thread_id_ptr;
    Worker& worker = workers[thread_id];

    std::function<void()> task = worker.pop();
    if (task) {
        return task;
    }

    return nullptr;
}

void find_and_execute_task() {
    std::function<void()> task = pop_task_from_runtime();
    if (task) { //Non - Empty Queue
        int* thread_id_ptr = static_cast<int*>(pthread_getspecific(id_key));    
        
        update_status(*thread_id_ptr);
        communicate(*thread_id_ptr);
    
        task();

        pthread_mutex_lock(&finish_lock);
        finish_counter--;
        pthread_mutex_unlock(&finish_lock);

    } else { //Empty Queue
        int* thread_id_ptr = static_cast<int*>(pthread_getspecific(id_key));
        acquire(*thread_id_ptr);
    }
}

void* worker_routine(void* arg) {
    int* thread_id = static_cast<int*>(arg);
    pthread_setspecific(id_key, thread_id);

    while (!shutdown) {
        find_and_execute_task();
    }

    return nullptr;
}

void quill::init_runtime() {
    num_workers = get_num_workers();
    workers = new Worker[num_workers];

    pthread_key_create(&id_key, destructor);
    int* main_thread_id = new int(0);
    pthread_setspecific(id_key, main_thread_id);

    pthread_mutex_init(&finish_lock, nullptr);

    for (int i = 0; i < num_workers; ++i) {
        r[i] = -1;
        t[i] = nullptr;
    }

    for (int i = 1; i < num_workers; ++i) {
        int* thread_id = new int(i);
        pthread_create(&threads[i], nullptr, worker_routine, thread_id);
    }
}

void quill::start_finish() {
    pthread_mutex_lock(&finish_lock);
    finish_counter = 0;
    pthread_mutex_unlock(&finish_lock);
}

void quill::end_finish() {
    while (finish_counter != 0) {
        find_and_execute_task();
    }
}

void quill::async(std::function<void()>&& lambda) {
    pthread_mutex_lock(&finish_lock);
    finish_counter++;
    pthread_mutex_unlock(&finish_lock);

    int* thread_id_ptr = static_cast<int*>(pthread_getspecific(id_key));
    int thread_id = *thread_id_ptr;
    workers[thread_id].push(std::move(lambda));
    update_status(thread_id);
}


void quill::finalize_runtime() {
    shutdown = true;
    for (int i = 0; i < num_workers; ++i) {
        t[i] = nullptr;
    }
    for (int i = 1; i < num_workers; ++i) {
        pthread_join(threads[i], nullptr);
    }

    delete[] workers;
    workers = nullptr;
}
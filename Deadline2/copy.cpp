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

int num_workers = 1;  // Default value is 1

int get_num_workers() {
    cout << "Entering get_num_workers" << endl;
    const char* workers_str = getenv("QUILL_WORKERS");
    if (workers_str != nullptr) {
        int workers = atoi(workers_str);
        if(workers > 0)
            return workers;
    }
    return 1;
}

void destructor(void* value) {
    cout << "Entering destructor" << endl;
    int* ptr = static_cast<int*>(value);
    delete ptr;
}

class deque {
public:
    int key, size;
    pthread_mutex_t lock2;
    int head;
    int tail;
    std::function<void()> array[DEQUE_SIZE];
    int task_count = 0;  // Local task counter
    int tasks_executed = 0;  // Number of tasks executed by this thread
    int steal_attempts = 0;  // Number of steal attempts by this thread
    int successful_steals = 0;  // Number of successful steals by this thread

    deque() : key(0), size(DEQUE_SIZE), head(0), tail(0) {
        pthread_mutex_init(&lock2, nullptr);
    }

    deque(int id) : key(id), size(DEQUE_SIZE), head(0), tail(0) {
        pthread_mutex_init(&lock2, nullptr);
    }

    bool NotEmpty() {
        return head != tail;
    }

    bool isEmpty() {
        return head == tail;
    }

    bool isFull() {
        return ((head + 1) % size == tail);
    }

    // void push(std::function<void()> task) {
    //     // No need for a lock in push operation as push operation can only be done by 1 worker (Every worker has their own deque)
    //     if (isFull()) {
    //         for (int i = tail, j = 0; i != head; i = (i + 1) % size, j++) {
    //             array[j] = array[i];
    //         }
    //         tail = 0;
    //         head = size - 1;
    //     }
        
    //     array[head] = task;
    //     head = (head + 1) % size;
    // }
    void push(std::function<void()> task) {
        if (isFull()) {
            cerr << "Deque is full, cannot push new task." << endl;
            return;
        }
        
        array[head] = task;  // Use move semantics to avoid copying
        head = (head + 1) % size;
    }

    std::function<void()> pop() {
        std::function<void()> task = nullptr;
        pthread_mutex_lock(&lock2);
        if (head != tail) {
            head = (head - 1 + size) % size;
            task = array[head];
            array[head] = nullptr;
        }
        pthread_mutex_unlock(&lock2);
        return task;
    }

    std::function<void()> steal() {

        std::function<void()> task = nullptr;
        pthread_mutex_lock(&lock2);
        if (head != tail) {
            task = array[tail];
            array[tail] = nullptr;
            tail = (tail + 1) % size;
        }
        pthread_mutex_unlock(&lock2);
        return task;
    }
};

deque* Deque = nullptr;

std::function<void()> pop_task_from_runtime() {
    // cout << "Entering pop_task_from_runtime" << endl;
    int* thread_id_ptr = static_cast<int*>(pthread_getspecific(id_key));
    if (!thread_id_ptr) {
        cerr << "Thread Error line 118" << endl;
        return nullptr;
    }
    int thread_id = *thread_id_ptr;

    std::function<void()> task = Deque[thread_id].pop();
    if (task) return task;

    srand(time(nullptr));
    int random_id = rand() % num_workers;

    Deque[thread_id].steal_attempts++;

    task = Deque[random_id].steal();
    if (task) {
        Deque[thread_id].successful_steals++;
    }

    return task;
}

void find_and_execute_task() {
    // cout << "Entering find_and_execute_task" << endl;
    std::function<void()> task = pop_task_from_runtime();
    if (task) {
        task();
        pthread_mutex_lock(&finish_lock);
        finish_counter--;
        pthread_mutex_unlock(&finish_lock);

        int* thread_id_ptr = static_cast<int*>(pthread_getspecific(id_key));
        if (thread_id_ptr) {
            int thread_id = *thread_id_ptr;
            Deque[thread_id].tasks_executed++;
        }
    }
}

void* worker_routine(void* arg) {
    cout << "Entering worker_routine" << endl;
    int* thread_id = static_cast<int*>(arg);
    pthread_setspecific(id_key, thread_id);

    while (!shutdown) {
        find_and_execute_task();
    }
    return nullptr;
}

void create_deque(int size) {
    cout << "Entering create_deque" << endl;
    for (int i = 0; i < size; ++i) {
        Deque[i] = deque(i);
    }
}

void quill::init_runtime() {
    cout << "Entering init_runtime" << endl;
    num_workers = get_num_workers();
    
    Deque = new deque[num_workers];
    
    pthread_key_create(&id_key, destructor);

    int* main_thread_id = new int(0);
    pthread_setspecific(id_key, main_thread_id);

    create_deque(num_workers);
    
    for (int i = 1; i < num_workers; ++i) {
        int* thread_id = new int(i);
        pthread_create(&threads[i], nullptr, worker_routine, thread_id);
    }
    pthread_mutex_init(&finish_lock, nullptr);
}

void quill::start_finish() {
    cout << "Entering start_finish" << endl;
    pthread_mutex_lock(&finish_lock);
    finish_counter = 0;
    pthread_mutex_unlock(&finish_lock);
}

void quill::end_finish() {
    cout << "Entering end_finish" << endl;
    while (finish_counter != 0) {
        find_and_execute_task();
    }
}

void quill::finalize_runtime() {
    cout << "Entering finalize_runtime" << endl;
    shutdown = true;
    for (int i = 1; i < num_workers; ++i) {
        pthread_join(threads[i], nullptr);
    }
    
    int total_tasks_created = 0;
    for (int i = 0; i < num_workers; ++i) {
        total_tasks_created += Deque[i].task_count;
        cout << "Thread " << i << " executed " << Deque[i].tasks_executed << " tasks." << endl;
        cout << "Thread " << i << " made " << Deque[i].steal_attempts << " steal attempts." << endl;
        cout << "Thread " << i << " had " << Deque[i].successful_steals << " successful steals.\n" << endl;
    }

    delete[] Deque;
    Deque = nullptr;
    cout << "Total tasks created: " << total_tasks_created << endl;
}

void quill::async(std::function<void()>&& lambda) {
    // cout << "Entering async" << endl;
    pthread_mutex_lock(&finish_lock);
    finish_counter++;
    pthread_mutex_unlock(&finish_lock);

    int* thread_id_ptr = static_cast<int*>(pthread_getspecific(id_key));
    if (!thread_id_ptr) {
        cerr << "Thread Error line 221" << endl;
        return;
    }
    int thread_id = *thread_id_ptr;

    std::function<void()> task_copy = lambda;

    Deque[thread_id].push(task_copy);
    Deque[thread_id].task_count++; 
}
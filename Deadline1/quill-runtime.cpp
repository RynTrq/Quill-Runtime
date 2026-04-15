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

#define MAX_THREADS_SIZE 20
#define DEQUE_SIZE 1000

volatile bool shutdown = false;
pthread_t threads[MAX_THREADS_SIZE];
static pthread_key_t id_key;
volatile int finish_counter = 0;
pthread_mutex_t finish_lock;

int num_workers = 1;  // Default value is 1

int get_num_workers() {
    const char* workers_str = getenv("QUILL_WORKERS");
    // printf(" - %s\n", workers_str);
    if (workers_str != nullptr) {
        int workers = atoi(workers_str);
        // return (workers > 0) ? workers : 1;
        if(workers > 0)
            return workers;
    }
    return 1;
}

void destructor(void* value) {
    int* ptr = static_cast<int*>(value);
    delete ptr;
}

//Raiyaan
class deque {
public:
    int key, size;
    pthread_mutex_t lock2;
    int head;
    int tail;
    std::function<void()> array[DEQUE_SIZE];

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

    void push(std::function<void()> task) {
        // No need for a lock in push operation as push operation can only be done by 1 worker (Every worker has there own deque)
        // pthread_mutex_lock(&lock2);
        
        if (isFull()) {
            for (int i = tail, j = 0; i != head; i = (i + 1) % size, j++) {
                array[j] = array[i];
            }
            tail = 0;
            head = size - 1;
        }
        
        array[head] = task;
        head = (head + 1) % size;
        
        // pthread_mutex_unlock(&lock2);
    }

    std::function<void()> pop() {
        pthread_mutex_lock(&lock2);
        std::function<void()> task = nullptr;
        if (head != tail) {
            head = (head - 1 + size) % size;
            task = array[head];
            array[head] = nullptr;
        }
        pthread_mutex_unlock(&lock2);
        return task;
    }

    std::function<void()> steal() {
        // lock is required in steal operation as steal can be done by any thread which has empty deque
        pthread_mutex_lock(&lock2);
        std::function<void()> task = nullptr;
        if (head != tail) {
            task = array[tail];
            array[tail] = nullptr;
            tail = (tail + 1) % size;
        }
        pthread_mutex_unlock(&lock2);
        return task;
    }
};

::deque* Deque = nullptr;

std::function<void()> pop_task_from_runtime() {
    int* thread_id_ptr = static_cast<int*>(pthread_getspecific(id_key));
    if (!thread_id_ptr) {
        cerr << "Thread Error line 118" << endl;
        return nullptr;
    }
    int thread_id = *thread_id_ptr;

    //We first try to pop a task
    std::function<void()> task = Deque[thread_id].pop();
    if (task) return task;
    //If pop doesnot work it means Deque is empty, we have to steal the task.

    // Find random woker
    srand(time(nullptr));
    int random_id = rand() % num_workers;
    //Try to steal
    task = Deque[random_id].steal();

    // if (!task) {
    //     printf("Steal not working");
    // }

    return task;
}

void find_and_execute_task() {
    std::function<void()> task = pop_task_from_runtime();
    if (task) {
        task();
        pthread_mutex_lock(&finish_lock);
        // printf("finish lock decrement");
        finish_counter--;
        pthread_mutex_unlock(&finish_lock);
    }
}

void* worker_routine(void* arg) {
    int* thread_id = static_cast<int*>(arg);
    pthread_setspecific(id_key, thread_id);

    while (!shutdown) {
        // wait for task to come
        find_and_execute_task();
    }
    return nullptr;
}

void create_deque(int size) {
    // printf("Deque");
    for (int i = 0; i < size; ++i) {
        Deque[i] = ::deque(i);
    }
}

void quill::init_runtime() {
    num_workers = get_num_workers();
    
    Deque = new ::deque[num_workers];
    
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
    pthread_mutex_lock(&finish_lock);
    // printf("finish counter started");
    finish_counter = 0;
    pthread_mutex_unlock(&finish_lock);
}

void quill::end_finish() {
    while (finish_counter != 0) {
        find_and_execute_task();
    }
}

void quill::finalize_runtime() {
    shutdown = true;
    for (int i = 1; i < num_workers; ++i) {
        //wait for all tasks to join and then delete the deque
        pthread_join(threads[i], nullptr);
    }
    delete[] Deque;
    Deque = nullptr;
}

//Dhruv
void quill::async(std::function<void()>&& lambda) {
    //another task
    pthread_mutex_lock(&finish_lock);
    finish_counter++;
    pthread_mutex_unlock(&finish_lock);

    int* thread_id_ptr = static_cast<int*>(pthread_getspecific(id_key));
    if (!thread_id_ptr) {
        cerr << "Thread Error line 221" << endl;
        return;
    }
    int thread_id = *thread_id_ptr;

    Deque[thread_id].push(lambda);
}

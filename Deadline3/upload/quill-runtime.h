#ifndef QUILL_RUNTIME_H
#define QUILL_RUNTIME_H

#include <pthread.h>
#include <functional>
#include <cstdint>  // Include for uint64_t

#define MAX_THREADS_SIZE 20
#define DEQUE_SIZE 1000

class deque;

extern pthread_t threads[MAX_THREADS_SIZE];
extern volatile bool shutdown;
extern volatile int finish_counter;

extern pthread_mutex_t finish_lock;
extern int num_workers;

extern deque* Deque;

int get_num_workers();
void communicate(int i);

void destructor(void* value);



#endif
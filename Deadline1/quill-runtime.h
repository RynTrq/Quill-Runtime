#ifndef QUILL_RUNTIME_H
#define QUILL_RUNTIME_H

#include <functional>
#include <pthread.h>

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
void destructor(void* value);

#endif

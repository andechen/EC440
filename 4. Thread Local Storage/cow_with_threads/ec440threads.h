#ifndef __EC440THREADS__
#define __EC440THREADS__

#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <setjmp.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define MAX_THREADS 128

/*
static unsigned long int ptr_demangle(unsigned long int p)
{
    unsigned long int ret;

    asm("movq %1, %%rax;\n"
        "rorq $0x11, %%rax;"
        "xorq %%fs:0x30, %%rax;"
        "movq %%rax, %0;"
    : "=r"(ret)
    : "r"(p)
    : "%rax"
    );
    return ret;
}*/

static unsigned long int ptr_mangle(unsigned long int p)
{
    unsigned long int ret;

    asm("movq %1, %%rax;\n"
        "xorq %%fs:0x30, %%rax;"
        "rolq $0x11, %%rax;"
        "movq %%rax, %0;"
    : "=r"(ret)
    : "r"(p)
    : "%rax"
    );
    return ret;
}

static void *start_thunk() {
  asm("popq %%rbp;\n"           //clean up the function prologue
      "movq %%r13, %%rdi;\n"    //put arg in $rdi
      "pushq %%r12;\n"          //push &start_routine
      "retq;\n"                 //return to &start_routine
      :
      :
      : "%rdi"
  );
  __builtin_unreachable();
}

// Save the return value of the threads start_routine function
void pthread_function_return_save(){
	unsigned long int reg;

	asm("movq %%rax, %0\n"
		:"=r"(reg));

    pthread_exit((void *) reg);
}

/* thread_status identifies the current state of a thread. You can add, rename,
 * or delete these values. This is only a suggestion. */
enum thread_status{
	TS_EXITED,
	TS_RUNNING,
	TS_READY,
	TS_EMPTY,
	TS_BLOCKED
};

// The thread control block stores information about a thread. 
typedef struct thread_control_block{
	pthread_t tid;
	void *stack;
	jmp_buf regs;
	enum thread_status status;
}thread_control_block;

// Schedule the thread execution using Round Robin 
static void schedule();

// Initialising threads after the first call of pthread_create
static void scheduler_init();

// Creating a thread
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg);

// Exit the thread
void pthread_exit(void *value_ptr);

// ID of the current thread
pthread_t pthread_self(void);

//***************************************Thread Sync***************************************//

// Mutex constructor
int pthread_mutex_init(pthread_mutex_t *restrict mutex, const pthread_mutexattr_t *restrict attr);

// Mutex destructor
int pthread_mutex_destroy(pthread_mutex_t *mutex);

// Lock the mutex
int pthread_mutex_lock(pthread_mutex_t *mutex);

// Unlock the mutex
int pthread_mutex_unlock(pthread_mutex_t *mutex);

// Barrier constructor
int pthread_barrier_init(pthread_barrier_t *restrict barrier, const pthread_barrierattr_t *restrict attr, unsigned count);

// Barrier destructor
int pthread_barrier_destroy(pthread_barrier_t *barrier);

// Filling the barrier
int pthread_barrier_wait(pthread_barrier_t *barrier);

#endif

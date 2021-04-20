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
int pthread_create(
	pthread_t *thread, const pthread_attr_t *attr,
	void *(*start_routine) (void *), void *arg);

// Exit the thread
void pthread_exit(void *value_ptr);

// ID of the current thread
pthread_t pthread_self(void);

//***************************************Thread Sync***************************************//

// Lock SIGALRM
static void lock(){
	sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    sigprocmask(SIG_BLOCK, &set, NULL);
}

// Unlock SIGALRM
static void unlock(){
	sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    sigprocmask(SIG_UNBLOCK, &set, NULL);
}

// Linked list struct
typedef struct linked_list{
	pthread_t tid;
	struct linked_list *next;
}linked_list;

// Put on the tail of the linked list
static void insert_tail(linked_list **list, linked_list **tail, pthread_t tid) {
    linked_list *to_ins = (linked_list *) malloc(sizeof(linked_list));

    to_ins->tid = tid;
    to_ins->next = NULL;

    if ((*list) == NULL) {
        (*list) = to_ins;
        (*tail) = to_ins;
    } else {
        list = &((*tail)->next);
        (*list) = to_ins;
        (*tail) = to_ins;
    }
}

// Get the head of the linked list
static void get_head(linked_list **list, linked_list **tail, pthread_t *tid) {
    linked_list *to_del;

    to_del = (*list);
    if (tid != NULL) {
        (*tid) = to_del->tid;
    }
    (*list) = to_del->next;
    free(to_del);
    if ((*list) == NULL) {
        (*tail) = NULL;
    }
}

// Check if the linked list is empty
static bool is_empty(linked_list *list) {
    return (list == NULL);
}

// Mutex struct
typedef struct{
	char locked;
	linked_list *wait_list;
	linked_list *wait_list_tail;
}MutexControlBlock;

// Mutex initialiser
int pthread_mutex_init(pthread_mutex_t *restrict mutex, const pthread_mutexattr_t *restrict attr);

// Mutex destructor
int pthread_mutex_destroy(pthread_mutex_t *mutex);

// Lock the mutex
int pthread_mutex_lock(pthread_mutex_t *mutex);

// Unlock the mutex
int pthread_mutex_unlock(pthread_mutex_t *mutex);

// Barrier struct
typedef struct{
	char init;
	char flag;
	pthread_t calling_thread;
	unsigned count;
	unsigned left;
}BarrierControlBlock;

// Barrier initialiser
int pthread_barrier_init(pthread_barrier_t *restrict barrier, const pthread_barrierattr_t *restrict attr, unsigned count);

// Barrier destructor
int pthread_barrier_destroy(pthread_barrier_t *barrier);

// Filling the barrier
int pthread_barrier_wait(pthread_barrier_t *barrier);

#endif

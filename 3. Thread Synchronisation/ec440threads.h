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
	TS_BLOCKED,
    TS_DEAD
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

//Postpones the execution of the calling thread until the target thread terminates
int pthread_join(pthread_t thread, void** value_ptr);

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

// Mutex constructor
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

// Barrier constructor
int pthread_barrier_init(pthread_barrier_t *restrict barrier, const pthread_barrierattr_t *restrict attr, unsigned count);

// Barrier destructor
int pthread_barrier_destroy(pthread_barrier_t *barrier);

// Filling the barrier
int pthread_barrier_wait(pthread_barrier_t *barrier);

/*..................Semaphore library..................*/

// A linked list (LL) node to store a queue entry 
struct QNode{ 
    pthread_t key; 
    struct QNode* next; 
}; 
  
// The queue, front stores the front node of LL and rear stores the 
// last node of LL 
struct Queue{ 
    struct QNode *front, *rear; 
}; 
  
// A utility function to create a new linked list node. 
struct QNode* newNode(pthread_t k){ 
    struct QNode* temp = (struct QNode*)malloc(sizeof(struct QNode)); 
    temp->key = k; 
    temp->next = NULL; 
    return temp; 
} 
  
// A utility function to create an empty queue 
struct Queue* createQueue(){ 
    struct Queue* q = (struct Queue*)malloc(sizeof(struct Queue)); 
    q->front = q->rear = NULL; 
    return q; 
} 
  
// The function to add a key k to q 
void enQueue(struct Queue* q, pthread_t k){ 
    // Create a new LL node 
    struct QNode* temp = newNode(k); 
  
    // If queue is empty, then new node is front and rear both 
    if (q->rear == NULL) { 
        q->front = q->rear = temp; 
        return; 
    } 
  
    // Add the new node at the end of queue and change rear 
    q->rear->next = temp; 
    q->rear = temp; 
} 
  
// Function to remove a key from given queue q 
pthread_t deQueue(struct Queue* q){ 
    // If queue is empty, return NULL. 
    if (q->front == NULL) 
        return (pthread_t) -1; 
  
    // Store previous front and move front one node ahead 
    struct QNode* temp = q->front; 
  
    q->front = q->front->next; 
  
    // If front becomes NULL, then change rear also as NULL 
    if (q->front == NULL) 
        q->rear = NULL; 
  
    return temp->key;
} 

//Struct which holds additional information needed for the semaphore implementation
struct semaphoreControlBlock{
    int value;                          /*Number of wakeup calls to semaphore*/
    struct Queue* blockedThreads;       /*Pointer to queue of waiting threads, queue implemented in queueHeader*/
    int isInitialized;                  /*Flag indicating whether the semaphore is initialized*/
};

//Intializes the semaphore referenced by sem
int sem_init(sem_t *sem, int pshared, unsigned value);

//Decrements the semphore referenced by sem
int sem_wait(sem_t *sem);

//increments the semphore referenced by sem
int sem_post(sem_t *sem);

//destroys the semphore referenced by sem
int sem_destroy(sem_t *sem);

#endif

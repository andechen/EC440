#ifndef __EC440THREADS__
#define __EC440THREADS__

#include <stdio.h> 
#include <stdlib.h> 
#include <pthread.h>

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

//
struct p_node{ 
    pthread_t key; 
    struct p_node* next; 
}; 

//
struct p_queue{ 
    struct p_node *front, *rear; 
}; 
  
//
struct p_node* Node(pthread_t key){ 
    struct p_node* temp = (struct p_node*)malloc(sizeof(struct p_node)); 
    temp->key = key; 
    temp->next = NULL; 
    return temp; 
} 
  
//
struct p_queue* Queue(){ 
    struct p_queue* queue = (struct p_queue*)malloc(sizeof(struct p_queue)); 
    queue->front = queue->rear = NULL; 
    return queue; 
} 

//
void enqueue(struct p_queue* queue, pthread_t key){ 
    struct p_node* temp = Node(key); 

    if (queue->rear == NULL){ 
        queue->front = queue->rear = temp; 
        return; 
    } 

    queue->rear->next = temp; 
    queue->rear = temp; 
} 

//
pthread_t dequeue(struct p_queue* queue){ 
    if (queue->front == NULL){
        return (pthread_t) -1;
    } 

    struct p_node* temp = queue->front; 
  
    queue->front = queue->front->next; 

    if (queue->front == NULL) 
        queue->rear = NULL; 
  
    return temp->key;
} 

#endif

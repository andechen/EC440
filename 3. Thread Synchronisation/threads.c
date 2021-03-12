#include "ec440threads.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <setjmp.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

/* You can support more threads. At least support this many. */
#define MAX_THREADS 128

/* Your stack should be this many bytes in size */
#define THREAD_STACK_SIZE 32767

/* Number of microseconds between scheduling events */
#define SCHEDULER_INTERVAL_USECS (50 * 1000)

/* Extracted from private libc headers. These are not part of the public
 * interface for jmp_buf.
 */
#define JB_RBX 0
#define JB_RBP 1
#define JB_R12 2
#define JB_R13 3
#define JB_R14 4
#define JB_R15 5
#define JB_RSP 6
#define JB_PC 7

// Define global variables
struct thread_control_block TCB_Table[MAX_THREADS];	// Table of all threads
pthread_t TID = 0;									// Currently running thread ID
struct sigaction signal_handler;					// Signal handler setup for SIGALRM


static void schedule(){
	// Set current thread to TS_READY
	switch(TCB_Table[TID].status){
		case TS_RUNNING	:
			TCB_Table[TID].status = TS_READY;
			break;
		case TS_EXITED	:
		case TS_READY	:
		case TS_BLOCKED	:
		case TS_EMPTY	:
			break;
	}

	// Finding the next thread to schedule
	pthread_t current_tid = TID;
	while(1){
		if(current_tid == MAX_THREADS - 1){
			current_tid = 0;
		}
		else{
			current_tid++;
		}

		// If the found thread's state is TS_READY, break the loop
		if(TCB_Table[current_tid].status == TS_READY){
			break;
		}
	}

	int jump = 0;
	// If the thread has not exited, save its state
	if(TCB_Table[TID].status != TS_EXITED){
		jump = setjmp(TCB_Table[TID].regs);
	}

	// Run the next thread
	if(!jump){
		TID = current_tid;
		TCB_Table[TID].status = TS_RUNNING;
		longjmp(TCB_Table[TID].regs, 1);
	}
}

static void scheduler_init(){
	// Initialise all threads as TS_EMPTY
	for(int i = 0; i < MAX_THREADS; i++){
		TCB_Table[i].status = TS_EMPTY;
		TCB_Table[i].tid = i;
	}

	// Setup the scheduler to SIGALRM at a specified interval
	__useconds_t usecs = SCHEDULER_INTERVAL_USECS;
	__useconds_t interval = SCHEDULER_INTERVAL_USECS;
	ualarm(usecs, interval);

	// Round Robin
	sigemptyset(&signal_handler.sa_mask);
	signal_handler.sa_handler = &schedule;
	signal_handler.sa_flags = SA_NODEFER;
	sigaction(SIGALRM, &signal_handler, NULL);
}

int pthread_create(
	pthread_t *thread, const pthread_attr_t *attr,
	void *(*start_routine) (void *), void *arg)
{
	// Create the timer and handler for the scheduler. Create thread 0.
	static bool is_first_call = true;
	int main_thread = 0;
	attr = NULL;

	if (is_first_call){
		scheduler_init();
		is_first_call = false;
		TCB_Table[0].status = TS_READY;
		main_thread = setjmp(TCB_Table[0].regs);
	}

	// New thread
	if (!main_thread){
		// Find an available thread ID and save it
        pthread_t current_tid = 1;
        while(TCB_Table[current_tid].status != TS_EMPTY && current_tid < MAX_THREADS){
            current_tid++;
        }

        if (current_tid == MAX_THREADS){
            fprintf(stderr, "ERROR: Max num of threads reached\n");
			exit(EXIT_FAILURE);
        }
        
        *thread = current_tid;

        // Save the state
        setjmp(TCB_Table[current_tid].regs);
        
		// Change PC to start_thunk
        TCB_Table[current_tid].regs[0].__jmpbuf[JB_PC] = ptr_mangle((unsigned long int)start_thunk); 
		
		// R13 -> arg
        TCB_Table[current_tid].regs[0].__jmpbuf[JB_R13] = (long) arg;  

		// R12 -> start_routine
        TCB_Table[current_tid].regs[0].__jmpbuf[JB_R12] = (unsigned long int) start_routine;
        
		// Create a new stack and set the pointer to the top of the stack
        TCB_Table[current_tid].stack = malloc(THREAD_STACK_SIZE);
		void* bottom_of_stack = TCB_Table[current_tid].stack + THREAD_STACK_SIZE;

		// Move the address of pthread_exit() to the top of the stack
		void* stackPointer = bottom_of_stack - sizeof(&pthread_function_return_save);
        void (*temp)(void*) = (void*) &pthread_function_return_save;
        stackPointer = memcpy(stackPointer, &temp, sizeof(temp));

		// Move the stack pointer(RSP) to the new stack
        TCB_Table[current_tid].regs[0].__jmpbuf[JB_RSP] = ptr_mangle((unsigned long int)stackPointer);

		// Status -> TS_READY
        TCB_Table[current_tid].status = TS_READY;

		// Set the tid
        TCB_Table[current_tid].tid = current_tid;

        schedule();
		
    }
    else{   
        main_thread = 0;
    }

	return 0;
}

void pthread_exit(void *value_ptr){
	// Status -> TS_EXITED
	TCB_Table[TID].status = TS_EXITED;

	// Wait...
	pthread_t tid = TCB_Table[TID].tid;
	if(tid != TID){
		TCB_Table[tid].status = TS_READY;
	}

	// Check if there are any threads that are ready to exit
	int threads_left = 0;
	int i;
	for(i = 0; i < MAX_THREADS; i++){
		switch(TCB_Table[i].status){
			case TS_READY	:
			case TS_RUNNING	:
			case TS_BLOCKED	:
				threads_left = 1;
				break;
			case TS_EXITED	:
			case TS_EMPTY	:
				break;
		}
	}

	if(threads_left){
		schedule();
	}

	for(i = 0; i < MAX_THREADS; i++){
		if(TCB_Table[i].status == TS_EXITED){
			free(TCB_Table[i].stack);
		}
	}
	exit(0);
}

pthread_t pthread_self(void){
	return TID;
}


//***************************************Thread Sync***************************************//
static void lock(){
	sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    sigprocmask(SIG_BLOCK, &set, NULL);
}

static void unlock(){
	sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    sigprocmask(SIG_UNBLOCK, &set, NULL);
}

typedef struct{
	pthread_t tid;
	void* data;
	struct linked_list_t *next;
}linked_list_t;



typedef struct{
	int locked;
	pthread_t owner;
	linked_list_t *wait_list;
	linked_list_t *wait_list_tail;
}MutexControlBlock;

int pthread_mutex_init(pthread_mutex_t *restrict mutex, const pthread_mutexattr_t *restrict attr){
	MutexControlBlock *MCB = (MutexControlBlock *) malloc(sizeof(MutexControlBlock));
	lock();

	//MCB = (MutexControlBlock *) mutex;
	MCB->locked = 0;
	MCB->owner = TID;
	
	
	mutex->__align = (long) MCB;
	unlock();
	return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex){
	MutexControlBlock *MCB = (MutexControlBlock *) (mutex->__align);
	lock();
	MCB->locked = 0;
	MCB->owner = -1;
	return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
 	MutexControlBlock *MCB = (MutexControlBlock *) (mutex->__align);

    lock();
    //if(MCB->owner)
    //MCB = (MutexControlBlock *) mutex;
	MCB->locked = 1;
	MCB->owner = TID;
	
	unlock();

	return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex){
	MutexControlBlock *MCB = (MutexControlBlock *) (mutex->__align);
	lock();

	if(MCB->owner != TID){
		unlock();
		return -1;
	}

	MCB->locked = 0;
	MCB->owner = -1;
	unlock();
	
	return 0;
}

typedef struct{
	pthread_mutex_t *mutex;
	pthread_cond_t *cond;
	unsigned count;
	unsigned left;
	unsigned round;
}BarrierControlBlock;

int pthread_barrier_init(pthread_barrier_t *restrict barrier, const pthread_barrierattr_t *restrict attr, unsigned count){
	if(count == 0){
		return EINVAL;
	}
	// BarrierControlBlock *BCB = (BarrierControlBlock *) (barrier->__align);

	// pthread_condattr_t condattr;
	// pthread_condattr_init(&condattr);

	// int ret = pthread_mutex_init(BCB->mutex, NULL);
	// if(ret){
	// 	return ret;
	// }

	// ret = pthread_cond_init(BCB->cond, &condattr);
	// if (ret) {
	// 	pthread_mutex_destroy(BCB->mutex);
	// 	return ret;
	// }

	// BCB->count = count;
	// BCB->left = count;
	// BCB->round = 0;

	return 0;
}

int pthread_barrier_destroy(pthread_barrier_t *barrier){
	// BarrierControlBlock *BCB = (BarrierControlBlock *) (barrier->__align);

	// if(BCB->count == 0){
	// 	return EINVAL;
	// }

	// BCB->count = 0;
	// int rm = pthread_mutex_destroy(BCB->mutex);
	// int rc = pthread_cond_destroy(BCB->cond);
	// return rm ? rm : rc;
	return 0;
}

int pthread_barrier_wait(pthread_barrier_t *barrier){
	// BarrierControlBlock *BCB = (BarrierControlBlock *) (barrier->__align);
	// pthread_mutex_lock(BCB->mutex);

	// if(--(BCB->left)){
	// 	unsigned round = BCB->round;
	// 	do{
	// 		pthread_cond_wait(BCB->cond, BCB->mutex);
	// 	}while(round == BCB->round);
	// 	pthread_mutex_unlock(BCB->mutex);
	// 	return 0;
	// }
	// else{
	// 	BCB->round++;
	// 	BCB->left = BCB->count;
	// 	pthread_cond_broadcast(BCB->cond);
	// 	pthread_mutex_unlock(BCB->mutex);
	// 	return 1;
	// }
	return 0;
}
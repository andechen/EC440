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
#include <semaphore.h>

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
thread_control_block TCB_Table[MAX_THREADS];	// Table of all threads
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

int pthread_join(pthread_t thread, void** value_ptr){
  
    switch(tcbArray[thread].status)
    {
        case TS_READY   :
        case TS_RUNNING :
        case TS_BLOCKED :
            //Set the status to Blocked and add it to the waiting queue of the target                
            TCB_Table[TID].status = TS_BLOCKED;
            TCB_Table[thread].waitingTid = TID;

            //Block until the target terminates
	        schedule();
            //At some point once the thread is finished, it will be sent back here
            
        case TS_DEAD:
            //Get the exit status of the target if value_ptr isn't NULL
            if (value_ptr){
                *value_ptr = TCB_Table[thread].exitStatus;
			}

            //Clean up the targets context
            free(TCB_Table[thread].stack);
            TCB_Table[thread].status = TS_EMPTY;
            break;

        case TS_EMPTY:
            printf("ERROR: Undefined behavoir\n");
            return ESRCH; //ESRCH 3 No such process
            break;
    }
    return 0;
}

//***************************************Thread Sync***************************************//

int pthread_mutex_init(pthread_mutex_t *restrict mutex, const pthread_mutexattr_t *restrict attr){
	MutexControlBlock *MCB = (MutexControlBlock *) malloc(sizeof(MutexControlBlock));

    MCB->locked = 0;
	MCB->wait_list = NULL;
	MCB->wait_list_tail = NULL;

	mutex->__align = (long) MCB;	// Align the struct pointer with the mutex
   	return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex){
	MutexControlBlock *MCB = (MutexControlBlock *) (mutex->__align);
	
	if(MCB->locked != 1){
		free((void *)(mutex->__align));
		return 0;
	}
	else{
		return -1;
	}
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
 	MutexControlBlock *MCB = (MutexControlBlock *) (mutex->__align);
	
	if(!MCB->locked){	// Thread grabs the lock
		lock();
		MCB->locked = 1;

		unlock();
		return 0;
	}
	else{				// Thread is blocked since the lock is busy
		lock();
		TCB_Table[TID].status = TS_BLOCKED;
		insert_tail(&MCB->wait_list, &MCB->wait_list_tail, TID);
		
		unlock();
		schedule();
		return EBUSY;
	}
}

int pthread_mutex_unlock(pthread_mutex_t *mutex){
	MutexControlBlock *MCB = (MutexControlBlock *) (mutex->__align);
	
	if(is_empty(MCB->wait_list)){	// No more threads waiting for the mutex
		lock();

		MCB->locked = 0;

		unlock();
		return 0;
	}
	else{										// More threads are waiting for the mutex
		lock();
		pthread_t next_thread;
		get_head(&MCB->wait_list, &MCB->wait_list_tail, &next_thread);
		MCB->locked = 1;

		TCB_Table[next_thread].status = TS_READY;
			
		unlock();
		schedule();
		return 0;
	}
}

int pthread_barrier_init(pthread_barrier_t *restrict barrier, const pthread_barrierattr_t *restrict attr, unsigned count){
	if(count == 0){
		return EINVAL;
	}

	BarrierControlBlock *BCB = (BarrierControlBlock*) malloc(sizeof(BarrierControlBlock));

	BCB->init = 1;
	BCB->flag = 0;
	BCB->calling_thread = -1;
	BCB->count = count;
	BCB->left = count;

	barrier->__align = (long) BCB;		// Align the struct pointer with the barrier
	return 0;
}

int pthread_barrier_destroy(pthread_barrier_t *barrier){
	BarrierControlBlock *BCB = (BarrierControlBlock *) (barrier->__align);

	if(BCB->init != 1){
		BCB->init = 0;
		BCB->calling_thread = -1;
		BCB->count = 0;
		BCB->left = 0;

		free((void *)(barrier->__align));
		return 0;
	}
	else{
		return -1;
	}
	return 0;
}

int pthread_barrier_wait(pthread_barrier_t *barrier){
	BarrierControlBlock *BCB = (BarrierControlBlock *) (barrier->__align);
	
	(BCB->left)--;

	if(BCB->flag != 1){						// Calling thread gets blocked
		lock();
		TCB_Table[TID].status = TS_BLOCKED;
		BCB->calling_thread = TID;
		BCB->flag = 1;

		unlock();
		schedule();
	}
	
	while(BCB->left != 0){					// Other threads wait here so that they don't exit the barrier
		schedule();
	}
	lock();				
	if(BCB->flag == 1){						// Unblock the calling thread
		BCB->flag = 0;
		TCB_Table[BCB->calling_thread].status = TS_READY;
	}		
	unlock();
	schedule();							
	if(BCB->calling_thread == TID){			// One thread returns PTHREAD_BARRIER_SERIAL_THREAD
		BCB->left = BCB->count;
		return PTHREAD_BARRIER_SERIAL_THREAD;
	}	
	else{									// Others return 0
		return 0;
	}
}

/*..................Semaphore library..................*/

int sem_init(sem_t *sem, int pshared, unsigned value){
    //Initialize semaphore control block to store addtional information needed by the semaphore
    struct semaphoreControlBlock* scbPtr = (struct semaphoreControlBlock*) malloc(sizeof(struct semaphoreControlBlock));

    scbPtr->value = value;
    scbPtr->blockedThreads = createQueue();
    scbPtr->isInitialized = 1;

    //Save that semaphore within the sem_t struct
    sem->__align = (long) scbPtr;
    
    return 0;
}

//Decrements the semphore referenced by sem
int sem_wait(sem_t *sem){
    struct semaphoreControlBlock* scbPtr = (struct semaphoreControlBlock*)(sem->__align);

    //If the value is less or equal to than zero, block
    if (scbPtr->value <= 0)
    {
        tcbArray[globalTid].status = THREAD_BLOCKED;
        enQueue(scbPtr->blockedThreads, globalTid);
        scheduleHandler();
    }
    //This either happens immediately
    else
    {
        (scbPtr->value)--;
        return 0;
    }

    //This will happen if the semaphore is woken
    if (tcbArray[globalTid].woke)
    {
        tcbArray[globalTid].woke = 0;
        return 0;
    }

    //Return statement just in case error occurs
    return -1;
}

//increments the semphore referenced by sem
int sem_post(sem_t *sem){
    struct semaphoreControlBlock* scbPtr = (struct semaphoreControlBlock*)(sem->__align);

    if (scbPtr->value >= 0)
    {
        //Either gets the waiting threads tid or returns -1 if the list is empty
        pthread_t waitingTid = deQueue(scbPtr->blockedThreads);
        
        if (waitingTid != -1)
        {
            //wakeup waiting thread
            tcbArray[waitingTid].woke = 1;
            tcbArray[waitingTid].status = THREAD_READY;
        }
        else
        {
            //If no thread was waiting, just increment the semaphore
            (scbPtr->value)++;
        }

        return 0;
    }
    
    return -1;
}

//destroys the semphore referenced by sem
int sem_destroy(sem_t *sem){
    struct semaphoreControlBlock* scbPtr = (struct semaphoreControlBlock*)(sem->__align);

    //Free's the semaphore control block for this semaphore
    if (scbPtr->isInitialized == 1)
    {
        free((void *)(sem->__align));   
    }
    else
    {
        return -1; //Error
    }
    
    return 0;
}
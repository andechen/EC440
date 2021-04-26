// Project 4 Test Case
// Author: Ivan Isakov

#include "tls.h"
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define LENGTH 14
#define THREAD_CNT 5
#define COUNTER_FACTOR 0xFFFFFF

#define BUFFER_SIZE 10000000

// const char *msg = "Hi, my name is In this project, we are implementing a library that  Hi, my name is In this project, we are implementing a library that provides protected memory regions for threads, which can be used for local storage. Recall that all threads share the same memory address space. This can be helpful for programs that need to share information across threads and don’t want to repeatedly copy every modification back and forth, but it can also be problematic when one thread modifies information that another thread assumed would remain unmodified. Hi, my name is In this project, we are implementing a library that provides protected memory regions for threads, which can be used for local storage. Recall that all threads share the same memory address space. This can be helpful for programs that need to share information across threads and don’t want to repeatedly copy every modification back and forth, but it can also be problematic when one thread modifies information that another thread assumed would remain unmodified";
const char *msg = "Hi my name is";
const char *msg2 = "Ivan";

int ret;
int count = 0;
pthread_t tid;
pthread_barrier_t barrier;
char buffer[BUFFER_SIZE] = {0};


void wasteTime(int a){
	for(long int i = 0; i < a*COUNTER_FACTOR; i++);
}

pthread_t threads[THREAD_CNT];

void* clone(void *arg){		
    ret = tls_clone(tid);
    assert(ret == 0);

    // Check multiple threads at the same time
    printf("Thread %lx enters the barrier\n", pthread_self());
    pthread_barrier_wait(&barrier);

    // read from the clone tls
    printf("Thread %lx: Trying to read the message from clone\n", pthread_self());
    ret = tls_read(0, BUFFER_SIZE, buffer);
    assert(ret == 0);

    printf("Read buffer: %s\n", buffer);
    // Use memcmp to check the validity -> strings.h
    // assert(memcmp(msg, buffer, LENGTH) == 0);

    printf("Thread %lx: Read successful. Read message: %s\n", pthread_self(), buffer);

    printf("Writing to tls: %s\n", msg2);
    assert(tls_write(0, 5, msg2) == 0);
    
    printf("Thread %lx: Trying to read the message from write\n", pthread_self());
    assert(tls_read(0, 5, buffer) == 0);
    
    assert(memcmp(msg2, buffer, 5) == 0);
    printf("Thread %lx: Read successful. Read message: %s\n", pthread_self(), buffer);

	printf("Thread %lx finished\n\n",pthread_self());
    count++;
    assert(tls_destroy() == 0);

	return NULL;
}

int main(int argc, char **argv){
    pthread_barrier_init(&barrier, NULL, THREAD_CNT);
        
    // create large tls
    printf("Main: Trying to create a TLS of size %d\n", BUFFER_SIZE);
    ret = tls_create(BUFFER_SIZE);
    assert(ret == 0);
    printf("Main TLS: Created\n\n");
    
    tid = pthread_self();
	
    // write some data
    memset(buffer, 'a', BUFFER_SIZE);
    // printf("Buffer: %s\n", buffer);

    printf("Main: Trying to write string %s to TLS...\n", msg);
    ret = tls_write(0, LENGTH, msg);
    // printf("Buffer after main: %s\n", buffer);
    assert(ret == 0);
    printf("...Main: Write successful\n\n");


    // create threads to clone to 
    for (int i = 0; i < THREAD_CNT; i++) {
		pthread_create(&threads[i], NULL, &clone, (void *)(intptr_t)i);
	}


    for(long int i = 0; i < THREAD_CNT; i++){
		wasteTime(10);
	}

    assert(tls_read(0, LENGTH, buffer) == 0);
    
    
    printf("Thread main: Read successful. Read message: %s\n", buffer);

    assert(tls_write(0, 5, msg2) == 0);
    
    printf("Thread main: Trying to read the message from write\n");
    assert(tls_read(0, 5, buffer) == 0);
    assert(memcmp(msg2, buffer, 5) == 0);
    printf("Thread main: Read successful. Read message: %s\n", buffer);

    for(long int i = 0; i < THREAD_CNT; i++){
		wasteTime(10);
	}

    // check that all of threads completed and none segfaulted
    assert(count == THREAD_CNT);

	return (EXIT_SUCCESS);
}

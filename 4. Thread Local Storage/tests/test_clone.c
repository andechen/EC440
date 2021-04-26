// Project 4 Test Case
// Author: Ivan Isakov

#include "tls.h"
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define LENGTH 15
#define THREAD_CNT 10
#define COUNTER_FACTOR 0xFFFFFF

#define BUFFER_SIZE 10000000

const char *msg = "Hi, my name is";

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
    printf("Thread %lx: Trying to read the message\n", pthread_self());
    ret = tls_read(0, LENGTH, buffer);
    assert(ret == 0);

    // Use memcmp to check the validity -> strings.h
    int n = memcmp(msg, buffer, LENGTH);
    assert(n == 0);

    printf("Thread %lx: Read successful. Read message: %s\n", pthread_self(), buffer);

	printf("Thread %lx finished\n\n",pthread_self());
    count++;
    tls_destroy();
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
    printf("Main: Trying to write string %s to TLS...\n", msg);
    ret = tls_write(0, LENGTH, msg);
    
    assert(ret == 0);
    printf("...Main: Write successful\n\n");


    // create threads to clone to 
    for (int i = 0; i < THREAD_CNT; i++) {
		pthread_create(&threads[i], NULL, &clone, (void *)(intptr_t)i);
	}

    for(long int i = 0; i < THREAD_CNT; i++){
		wasteTime(10);
	}

    // check that all of threads completed and none segfaulted
    assert(count == THREAD_CNT);

	return (EXIT_SUCCESS);
}

//#define _GNU_SOURCE
#include "tls.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <assert.h>
#include <string.h>

// int tls_create(unsigned int size);
// int tls_destroy();
// int tls_read(unsigned int offset, unsigned int length, char *buffer);
// int tls_write(unsigned int offset, unsigned int length, const char *buffer);
// int tls_clone(pthread_t tid);

int ret;
pthread_t tid;

int main(int argc, char** argv) {
    char buffer[1000] = {0};
    char *msg = "Hello";
        
    
    printf("Main: Trying to obtain TLS of size 1000...\n");
    ret = tls_create(1000);
    assert(ret == 0);
    printf("...Main: Obtained\n\n");
    
    printf("Main: Trying to write string %s to TLS...\n", msg);
    ret = tls_write(0, 6, msg);
    assert(ret == 0);
    printf("...Main: Write successful\n\n");
    
    printf("Main: Trying to read back the message...\n");
    ret = tls_read(0, 6, buffer);
    assert(ret == 0);
    printf("...Main: Read successful. Received message: %s\n\n", buffer);

    printf("Main: Trying to destroy my TLS...\n");
    ret = tls_destroy();
    assert(ret == 0);
    printf("...Main: Destroy successful\n");

    return (EXIT_SUCCESS);
}


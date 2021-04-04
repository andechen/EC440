#ifndef TLS_H_
#define TLS_H_
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>

#define HASH 23

// Creates a local storage area (TLS) of a given size for the current thread
int tls_create(unsigned int size);

// Frees local storage area (TLS) for current thread
int tls_destroy();

// Reads length bytes from the TLS of the currently executing thread, starting at 
// position offset and writes into memory location pointed to by buffer
int tls_read(unsigned int offset, unsigned int length, char *buffer);

// Reads length bytes from the memory location pointed to by buffer and writes them 
// into the local storage area of the currently executing thread, starting at position offset
int tls_write(unsigned int offset, unsigned int length, const char *buffer);

// Clones the TLS of the target thread tid as CoW
int tls_clone(pthread_t tid);

#endif /* TLS_H_ */

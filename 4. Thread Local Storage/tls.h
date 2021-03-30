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

#define MAX_THREADS 128

typedef struct Page{
    //unsigned long int address;
    uintptr_t address;	// Start address of a page
    int ref_count;		// Counter for the number of shared pages
}Page;

typedef struct ThreadLocalStorage{
	//pthread_t tid;
    unsigned int size;      // Size in bytes
    unsigned int page_num;  // Number of pages
    Page** pages;   // Array of pointers to Pages
}ThreadLocalStorage;

typedef struct Item{
    pthread_t key;
    ThreadLocalStorage* thread_local_storage;
    struct Item *next;
}Item;

Item* Table[127];
//Item* Table[MAX_THREADS];
int page_size = 0;
//unsigned long int page_size = 0;


unsigned int HashFunction(pthread_t tid){
    return (unsigned int) tid % 127;
}

Item* Find(pthread_t tid){
    int index = HashFunction(tid);

    if(Table[index] != NULL){
        Item* element = Table[index];
        
        while(Table[index]->key != tid && element != NULL){
            element = element->next;
        }

        return element;
    }

    return NULL;
    
}

bool Add(Item* newElement){
    pthread_t key = pthread_self();
    int index = HashFunction(key);

    if(Table[index] != NULL){
        Item* element = Table[index];
        Table[index] = newElement;
        Table[index]->next = element;

        return true;
    }
    else{
        Table[index] = newElement;
        Table[index]->next = NULL;
        
        return true;
    }

    return false;
}

Item* Remove(pthread_t tid){
    int index = HashFunction(tid);

    if(Table[index] != NULL){
        Item* prev = NULL;
        Item* current = Table[index];
		Item* next = Table[index]->next;
        
        while(Table[index]->key != tid && current != NULL){
        //while(current->key != tid && current != NULL){
            prev = current;
            current = next;
			next = current->next;
        }

		if(prev){
			prev->next = next;
		}

        return current;
    }
    else{
        return NULL;
    }
    
}

// Helper function to handle signals like SIGSEGV and SIGBUS, when they are caused by the TLS
void tls_handle_page_fault(int sig, siginfo_t *si, void *context){
    uintptr_t p_fault = ((uintptr_t) si->si_addr) & ~(page_size - 1);

    for(int i = 0; i < 127; i++){
        if(Table[i]){
            Item* element = Table[i];
            Page* page;

            // Scan through the linked list inside the element of the hash table, to find the matching address
            while(element){
                for(int j = 0; j < element->thread_local_storage->page_num; j++){
                    page = element->thread_local_storage->pages[j];
                    
                    // If the address matches the one where the fault was caused, exit the thread
                    if(page->address == p_fault){
                        pthread_exit(NULL);
                    }
                }
                element = element->next;
            }
        }
    }

    // Otherwise it was not a TLS related fault, so raise the signal
    signal(SIGSEGV, SIG_DFL);
    signal(SIGBUS, SIG_DFL);
    raise(sig);
}

// Helper function to initialise the needed parameters
void tls_init(){
    struct sigaction sigact;
    
    //Handle page faults (SIGSEGV, SIGBUS)
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_SIGINFO;
    
    //Give context to handler
    sigact.sa_sigaction = tls_handle_page_fault;
    sigaction(SIGBUS, &sigact, NULL);
    sigaction(SIGSEGV, &sigact, NULL);

	page_size = getpagesize();
}

// Creates a local storage area of a given size for the current thread
int tls_create(unsigned int size);

// Frees local storage area for current thread
int tls_destroy();

// Reads length bytes from the LSA of the currently executing thread, starting at 
// position offset and writes into memory location pointed to by buffer
int tls_read(unsigned int offset, unsigned int length, char *buffer);

// Reads length bytes from the memory location pointed to by buffer and writes them 
// into the local storage area of the currently executing thread, starting at position offset
int tls_write(unsigned int offset, unsigned int length, const char *buffer);

// Clones the LSA of the target thread tid as CoW
int tls_clone(pthread_t tid);

#endif /* TLS_H_ */

#include "tls.h"
#define _GNU_SOURCE
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

// Page struct
typedef struct Page{
	unsigned long int address;	// Start address of a page
    int ref_count;		// Counter for the number of shared pages
}Page;

// TLS struct
typedef struct ThreadLocalStorage{
    unsigned int size;      // Size in bytes
    unsigned int page_num;  // Number of pages
    Page** pages;   		// Array of pointers to Pages
}ThreadLocalStorage;

typedef struct Item{
    pthread_t tid;      // Thread ID for this specific element
    ThreadLocalStorage* tls;    // TLS
    struct Item *next;      // Pointer to the next TLS
}Item;

Item* Table[HASH];      // TLS table
unsigned long int page_size = 0;    // Page size
pthread_mutex_t mutex;      // Mutex

// Calculate the hash
unsigned int HashFunction(pthread_t tid){
    return (unsigned int) tid % HASH;
}

// Finds an element in the TLS table
Item* Find(pthread_t tid){
    int index = HashFunction(tid);

    if(Table[index] != NULL){
        Item* element = Table[index];

		while(element != NULL){
			if(element->tid == tid){
				return element;
			}
            element = element->next;
        }
    }

    return NULL;
}

// Inserts an element into the TLS table
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

// Removes an element from the TLS table
Item* Remove(pthread_t tid){
    int index = HashFunction(tid);

    if(Table[index] != NULL){
        Item* prev = NULL;
        Item* current = Table[index];
		Item* next = Table[index]->next;
        
		while(current != NULL){
			if(current->tid == tid){
				break;
			}
            prev = current;
            current = next;
			next = current->next;
		}

		if(prev != NULL){
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
    unsigned long int p_fault = ((unsigned long int) si->si_addr & ~(page_size - 1));

    for(int i = 0; i < HASH; i++){
        
        if(Table[i]){
            Item* element = Table[i];
        
            // Scan through the linked list inside the element of the hash table, to find the matching address
            while(element){
                for(int j = 0; j < element->tls->page_num; j++){
                    Page* page = element->tls->pages[j];
                    
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
    
    // Handle page faults (SIGSEGV, SIGBUS)
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_SIGINFO;
    
    // Give context to handler
    sigact.sa_sigaction = tls_handle_page_fault;
    sigaction(SIGBUS, &sigact, NULL);
    sigaction(SIGSEGV, &sigact, NULL);

    // Get the page size
	page_size = getpagesize();
}

int tls_create(unsigned int size){
	static bool is_first_call = true;

	// Initialise the TLS in the first call to this function
	if(is_first_call){
		tls_init();
		is_first_call = false;
		pthread_mutex_init(&mutex, NULL);
	}

	pthread_t TID = pthread_self();

	// Return an error if the given size is <= 0 or thread already has a TLS
	if(size <= 0 || Find(TID) != NULL){
		return -1;
	}

	// Creating a TLS for this thread
	ThreadLocalStorage* TLS = (ThreadLocalStorage*) malloc(sizeof(ThreadLocalStorage));
	TLS->size = size;
	TLS->page_num = size / page_size + (size % page_size != 0);	// Ceiling the page_num

	// Creating the pages array and then creating each page
	TLS->pages = (Page**) calloc(TLS->page_num, sizeof(Page*));
	for(int i = 0; i < TLS->page_num; i++){
		Page* page = (Page*) malloc(sizeof(Page));

		// Use mmap to map the pages to a specific place in memory
		page->address = (unsigned long int) mmap(0, page_size, PROT_NONE, (MAP_ANON | MAP_PRIVATE), 0, 0);
		
		if(page->address == (unsigned long int) MAP_FAILED){
			return -1;
		}

		page->ref_count = 1;
		TLS->pages[i] = page;
	}

	// Creating an element for the TLS hash table
	Item* element = (Item*) malloc(sizeof(Item));
	element->tls = TLS;
	element->tid = TID;

	// Check if the insertion of the element was successful
	if(Add(element)){
		return 0;
	}
	else{
		return -1;
	}
}

int tls_destroy(){
	pthread_t TID = pthread_self();
	Item* element = Remove(TID);

	// Throw an error if the TLS for this thread does not exist
	if(element == NULL){
		return -1;
	}
	
	// Iterate through the pages and free them if they are not shared, or if they are
	// decrement the count
	for(int i = 0; i < element->tls->page_num; i++){
		Page* page = element->tls->pages[i];
		if(element->tls->pages[i]->ref_count == 1){
			munmap((void *) page->address, page_size);
			free(page);
		}
		else{
			(element->tls->pages[i]->ref_count)--;
		}
	}
	// Free the calloced pages array
	free(element->tls->pages);

	// Freeing the TLS of the item of the hash table and the item itself
	free(element->tls);
	free(element);
	
	return 0;
}

// Protect the pages from Reading or Writing
static void tls_protect(Page* p){
    if(mprotect((void *) p->address, page_size, PROT_NONE)){
        fprintf(stderr, "tls_protect: could not protect page\n");
        exit(1);
    }
}

// Unprotecting the pages, so that each page can be used for Reading or Writing
static void tls_unprotect(Page* p, const int protect){ 
    // The variable protect defines what is allowed to be done to a page
    if(mprotect((void *) p->address, page_size, protect)){
        fprintf(stderr, "tls_unprotect: could not unprotect page\n");
        exit(1);
    }
}

int tls_write(unsigned int offset, unsigned int length, const char *buffer){
	pthread_mutex_lock(&mutex);

	pthread_t TID = pthread_self();
	Item* element = Find(TID);

	// Throw an error if the TLS for a particular thread does not exist
	if(element == NULL){
		pthread_mutex_unlock(&mutex);
		return -1;
	}
		
	ThreadLocalStorage* TLS = element->tls;

	// Throw an error if the function call wants to read more memory than the TLS can hold
	if((offset + length) > TLS->size){
		pthread_mutex_unlock(&mutex);
		return -1;
	}
	
	// pthread_mutex_lock(&mutex);
	// Unprotect the pages in the TLS, so that they are available for writting
	for(int i = 0; i < TLS->page_num; i++){
		tls_unprotect(TLS->pages[i], PROT_WRITE);
	}

	// Write from the buffer to the TLS
	for(int count = 0, index = offset; index < (offset + length); ++count, ++index){
		Page *page, *page_copy;

		// Calculate the page number in the TLS, its offset and the page itself
		unsigned int page_number = index / page_size;
		unsigned int page_offset = index % page_size;
		page = TLS->pages[page_number];

		// If the page is shared, create a private copy (COW)
		if(page->ref_count > 1){
			// Create a copy of the page
			page_copy = (Page *)malloc(sizeof(Page));
			page_copy->address = (unsigned long int) mmap(0, page_size, PROT_WRITE, (MAP_ANON | MAP_PRIVATE), 0, 0);
			memcpy((void*) page_copy->address, (void*)page->address, page_size);
			page_copy->ref_count = 1;
			TLS->pages[page_number] = page_copy;

			// Update the original page
			(page->ref_count)--;
			tls_protect(page);

			page = page_copy;
		}

		// Set the page_byte equal to the specific byte in the buffer
		char* page_byte = (char *) (page->address + page_offset);
		*page_byte = buffer[count];
	}	

	// Reprotect all of the pages in the TLS
	for(int i = 0; i < TLS->page_num; i++){
		tls_protect(TLS->pages[i]);
	}

	pthread_mutex_unlock(&mutex);

	return 0;
}

int tls_read(unsigned int offset, unsigned int length, char *buffer){
	pthread_mutex_lock(&mutex);

	pthread_t TID = pthread_self();
	Item* element = Find(TID);

	// Throw an error if the TLS for a particular thread does not exist
	if(element == NULL){
		pthread_mutex_unlock(&mutex);
		return -1;
	}
	
	ThreadLocalStorage* TLS = element->tls;
	
	// Throw an error if the function call wants to read more memory than the TLS can hold
	if((offset + length) > TLS->size){
		pthread_mutex_unlock(&mutex);
		return -1;
	}

	
	// Unprotect the pages in the TLS, so that they are available for reading
	for(int i = 0; i < TLS->page_num; i++){
		tls_unprotect(TLS->pages[i], PROT_READ);
	}
	
	// Read the pages and write them to the buffer
	for(int count = 0, index = offset; index < (offset + length); ++count, ++index){
		Page* page;
		// Calculate the page number in the TLS, its offset and the page itself
		unsigned int page_number = index / page_size;
		unsigned int page_offset = index % page_size;
		page = TLS->pages[page_number];
		
		// Store the byte in this page at this offset in the buffer
		char *page_byte = (char *) (page->address + page_offset);
		buffer[count] = *page_byte;
	}

	// Reprotect all of the pages in the TLS
	for(int i = 0; i < TLS->page_num; i++){
		tls_protect(TLS->pages[i]);
	}

	pthread_mutex_unlock(&mutex);

	return 0;
}

int tls_clone(pthread_t tid){
	// Find the element in the table which corresponds to the thread's where to clone the target TLS
	pthread_t clone_tid = pthread_self();
	Item* clone_element = Find(clone_tid);

	// Find the element in the table which corresponds to the target thread's TLS
	Item* target_element = Find(tid);

	// Throw an error if there is no TLS for the original or if the clone already has a TLS
	if(clone_element != NULL || target_element == NULL){
		return -1;
	}

	ThreadLocalStorage* targetTLS = target_element->tls;

	// Create a new item for the table
	clone_element = (Item*) malloc(sizeof(Item));
	clone_element->tid = clone_tid;

	// Clone the TLS
	clone_element->tls = (ThreadLocalStorage*) malloc(sizeof(ThreadLocalStorage));
	clone_element->tls->page_num = targetTLS->page_num;
	clone_element->tls->size = targetTLS->size;
	
	clone_element->tls->pages = (Page**) calloc(clone_element->tls->page_num, sizeof(Page*));
	
	for(int i = 0; i < clone_element->tls->page_num; i++){
		// (targetTLS->pages[i]->ref_count)++;
		clone_element->tls->pages[i] = targetTLS->pages[i];
		(clone_element->tls->pages[i]->ref_count)++;
	}

	// Check if the insert into the hash table worked, else throw an error
	if(Add(clone_element)){
		return 0;
	}
	else{
		return -1;
	}
}

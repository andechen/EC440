#include "tls.h"
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

typedef struct Page{
    //unsigned long int addr;
    uintptr_t addr;	// Start address of a page
    int count;		// Counter for the number of shared pages
}Page;

typedef struct ThreadLocalStorage{
    unsigned int size;      // Size in bytes
    unsigned int page_num;  // Number of pages
    Page** array_of_pages;   // Array of pointers to Pages
}ThreadLocalStorage;

typedef struct Item {
    pthread_t key;
    ThreadLocalStorage* thread_local_storage;
    struct Item* next;
}Item;

Item* Table[127];
int page_size = 0;

unsigned int HashFunction(pthread_t key){
    return (unsigned int) key % 127;
}

Item* Find(pthread_t key){
    int index = HashFunction(key);
    Item* element = Table[index];

    if(element != NULL){
        
        while(element->key != key){ //
            element = element->next;
        }

        return element;
    }
    else{
        return NULL;
    }
}

bool Add(Item* NewElement){
    pthread_t key = pthread_self();
    int index = HashFunction(key);
    Item* element = Table[index];

    if(element != NULL){
        element = NewElement;
        element->next = element;

        return true;
    }
    else{
        element = NewElement;
        element->next = NULL;
        
        return true;
    }

    return false;
}

Item* Remove(pthread_t key){
    int index = HashFunction(key);

    if(Table[index] != NULL){
        Item* prev = NULL;
        Item* current = Table[index];
		Item* next = Table[index]->next;
        
        while(Table[index]->key != key && current != NULL){
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
                    page = element->thread_local_storage->array_of_pages[j];
                    
                    // If the address matches the one where the fault was caused, exit the thread
                    if(page->addr == p_fault){
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



int tls_create(unsigned int size){
	static bool is_first_call = true;

	// Initialise the TLS in the first call to this function
	if(is_first_call){
		tls_init();
		is_first_call = false;
	}

	pthread_t TID = pthread_self();

	// Return an error if the given size is <=0 or thread already has a TLS
	if(size <= 0 || Find(TID) != NULL){
		return -1;
	}

	// Creating a TLS for this thread
	ThreadLocalStorage* TLS = (ThreadLocalStorage*) malloc(sizeof(ThreadLocalStorage));
	TLS->size = size;
	TLS->page_num = size / page_size + (size % page_size != 0);	// Ceiling the page_num

	// Creating the pages array and then creating each page
	TLS->array_of_pages = (Page**) calloc(TLS->page_num, sizeof(Page*));
	for(int i = 0; i < TLS->page_num; i++){
		Page* page = (Page*) malloc(sizeof(Page));

		// Use mmap to map the pages to a specific place in memory
		page->addr = (uintptr_t) mmap(0, page_size, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
		
		if(page->addr == (uintptr_t) MAP_FAILED){
			return -1;
		}

		page->count = 1;
		TLS->array_of_pages[i] = page;
	}

	// Creating an element for the TLS hash table
	Item* table_item = (Item*) malloc(sizeof(Item));
	table_item->thread_local_storage = TLS;
	table_item->key = TID;

	// Check if the insertion of the element was successful
	if(Add(table_item)){
		return 0;
	}
	else{
		return -1;
	}
	//return 0;
}

int tls_destroy(){
	pthread_t TID = pthread_self();
	Item* table_item = Remove(TID);

	// Throw an error if the TLS for this thread does not exist
	if(table_item == NULL){
		return -1;
	}
	
	// Iterate through the pages and free them if they are not shared, or if they are
	// decrement the count
	for(int i = 0; i < table_item->thread_local_storage->page_num; i++){

		if(table_item->thread_local_storage->array_of_pages[i]->count == 1){
			free(table_item->thread_local_storage->array_of_pages[i]);
		}
		else{
			(table_item->thread_local_storage->array_of_pages[i]->count)--;
		}
	}

	// Freeing the TLS of the item of the hash table and the item itself
	free(table_item->thread_local_storage);
	free(table_item);
	
	return 0;
}

// Protect the pages from Reading or Writing
static void tls_protect(Page* p){
    if(mprotect((void *) p->addr, page_size, PROT_NONE)){
        fprintf(stderr, "tls_protect: could not protect page\n");
        exit(1);
    }
}

// Unprotecting the pages, so that each page can be used for Reading or Writing
static void tls_unprotect(Page* p, const int protect){ 
    // The variable protect defines what is allowed to be done to a page
    if(mprotect((void *) p->addr, page_size, protect)){
        fprintf(stderr, "tls_unprotect: could not unprotect page\n");
        exit(1);
    }
}



int tls_write(unsigned int offset, unsigned int length, const char *buffer){
	pthread_t TID = pthread_self();
	Item* table_item = Find(TID);

	// Throw an error if the TLS for a particular thread does not exist
	if(table_item == NULL){
		return -1;
	}
		
	ThreadLocalStorage* TLS = table_item->thread_local_storage;

	// Throw an error if the function call wants to read more memory than the TLS can hold
	if(offset + length > TLS->size){
		return -1;
	}
	
	// Unprotect the pages in the TLS, so that they are available for writting
	for(int i = 0; i < TLS->page_num; i++){
		tls_unprotect(TLS->array_of_pages[i], PROT_WRITE);
	}

	int count, index;
	// Write from the buffer to the TLS
	for(count = 0, index = offset; index < (offset + length); ++count, ++index){
		Page *page, *page_copy;

		// Calculate the page number in the TLS, its offset and the page itself
		unsigned int page_number = index / page_size;
		unsigned int page_offset = index % page_size;
		page = TLS->array_of_pages[page_number];

		// If the page is shared, create a private copy (COW)
		if(page->count > 1){
			// Create a copy of the page
			page_copy = (Page *)malloc(sizeof(Page));
			page_copy->addr = (uintptr_t) mmap(0, page_size, PROT_WRITE, MAP_ANON | MAP_PRIVATE, 0, 0);
			memcpy((void *) page_copy->addr, (void *)page->addr, page_size);
			page_copy->count = 1;
			TLS->array_of_pages[page_number] = page_copy;

			// Update the original page
			(page->count)--;
			tls_protect(page);
			page = page_copy;
		}

		// Set the page_byte equal to the specific byte in the buffer
		char* page_byte = ((char*) page->addr) + page_offset;
		//*page_byte = buffer[index - offset];
		*page_byte = buffer[count];
				
	}	

	// Reprotect all of the pages in the TLS
	for(int i = 0; i < TLS->size; i++){
		tls_protect(TLS->array_of_pages[i]);
	}

	return 0;
}

int tls_read(unsigned int offset, unsigned int length, char *buffer){
	pthread_t TID = pthread_self();
	Item* element = Find(TID);

	// Throw an error if the TLS for a particular thread does not exist
	if(element == NULL){
		return -1;
	}
	
	ThreadLocalStorage* TLS = element->thread_local_storage;
	
	// Throw an error if the function call wants to read more memory than the TLS can hold
	if((offset + length) > TLS->size){
		return -1;
	}

	// Unprotect the pages in the TLS, so that they are available for reading
	for(int i = 0; i < TLS->page_num; i++){
		tls_unprotect(TLS->array_of_pages[i], PROT_READ);
	}

	int count, index;
	// Read the pages and write them to the buffer
	for(count = 0, index = offset; index < (offset + length); ++count, ++index){
		Page* page;

		// Calculate the page number in the TLS, its offset and the page itself
		unsigned int page_number = index / page_size;
		unsigned int page_offset = index % page_size;
		page = TLS->array_of_pages[page_number];

		// Store the byte in this page at this offset in the buffer
		char* page_byte = ((char*) page->addr) + page_offset;
		buffer[count] = *page_byte;
	}

	// Reprotect all of the pages in the TLS
	for(int i = 0; i < TLS->page_num; i++){
		tls_protect(TLS->array_of_pages[i]);
	}

	return 0;
}

int tls_clone(pthread_t tid){
	pthread_t clone = pthread_self();
	Item* clone_item = Find(clone);
	Item* original_item = Find(tid);

	// Throw an error if there is no TLS for the original or if the clone already has a TLS
	if(clone_item != NULL || original_item == NULL){
		return -1;
	}

	ThreadLocalStorage* originalTLS = original_item->thread_local_storage;

	// Clone the TLS
	clone_item->thread_local_storage = (ThreadLocalStorage *) malloc(sizeof(ThreadLocalStorage));
	clone_item->thread_local_storage->page_num = originalTLS->page_num;
	clone_item->thread_local_storage->size = originalTLS->size;
	clone_item->thread_local_storage->array_of_pages = (Page**) calloc(clone_item->thread_local_storage->page_num, sizeof(Page*));
	for(int i = 0; i < clone_item->thread_local_storage->page_num; i++){
		clone_item->thread_local_storage->array_of_pages[i] = originalTLS->array_of_pages[i];
		(clone_item->thread_local_storage->array_of_pages[i]->count)++;
	}

	// Check if the insert into the hash table worked, else throw an error
	if(Add(clone_item)){
		return 0;
	}
	else{
		return -1;
	}
}

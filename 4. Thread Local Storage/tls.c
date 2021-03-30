#include "tls.h"
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
	TLS->pages = (Page**) calloc(TLS->page_num, sizeof(Page*));
	for(int i = 0; i < TLS->page_num; i++){
		Page* page = (Page*) malloc(sizeof(Page));

		// Use mmap to map the pages to a specific place in memory
		page->address = (uintptr_t) mmap(0, page_size, PROT_NONE, (MAP_ANON | MAP_PRIVATE), -1, 0);
		
		if(page->address == (uintptr_t) MAP_FAILED){
			return -1;
		}

		page->ref_count = 1;
		TLS->pages[i] = page;
	}

	// Creating an element for the TLS hash table
	Item* element = (Item*) malloc(sizeof(Item));
	element->thread_local_storage = TLS;
	element->key = TID;

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
	for(int i = 0; i < element->thread_local_storage->page_num; i++){

		if(element->thread_local_storage->pages[i]->ref_count == 1){
			free(element->thread_local_storage->pages[i]);
		}
		else{
			(element->thread_local_storage->pages[i]->ref_count)--;
		}
	}

	// Freeing the TLS of the item of the hash table and the item itself
	free(element->thread_local_storage);
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
	
	// Unprotect the pages in the TLS, so that they are available for writting
	for(int i = 0; i < TLS->page_num; i++){
		tls_unprotect(TLS->pages[i], PROT_WRITE);
	}

	//int count, index;
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
			page_copy->address = (uintptr_t) mmap(0, page_size, PROT_WRITE, (MAP_ANON | MAP_PRIVATE), -1, 0);
			memcpy((void*) page_copy->address, (void*)page->address, page_size);
			page_copy->ref_count = 1;
			TLS->pages[page_number] = page_copy;

			// Update the original page
			(page->ref_count)--;
			tls_protect(page);
			page = page_copy;
		}

		// Set the page_byte equal to the specific byte in the buffer
		char* page_byte = ((char *) page->address) + page_offset;
		*page_byte = buffer[count];
				
	}	

	// Reprotect all of the pages in the TLS
	for(int i = 0; i < TLS->page_num; i++){
		tls_protect(TLS->pages[i]);
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
		tls_unprotect(TLS->pages[i], PROT_READ);
	}

	//int count, index;
	// Read the pages and write them to the buffer
	for(int count = 0, index = offset; index < (offset + length); ++count, ++index){
		Page* page;

		// Calculate the page number in the TLS, its offset and the page itself
		unsigned int page_number = index / page_size;
		unsigned int page_offset = index % page_size;
		page = TLS->pages[page_number];

		// Store the byte in this page at this offset in the buffer
		char* page_byte = ((char *) page->address) + page_offset;
		buffer[count] = *page_byte;
	}

	// Reprotect all of the pages in the TLS
	for(int i = 0; i < TLS->page_num; i++){
		tls_protect(TLS->pages[i]);
	}

	return 0;
}

int tls_clone(pthread_t tid){
	// Find the element in the table which corresponds to the target thread's TLS
	Item* target_element = Find(tid);

	// Find the element in the table which corresponds to the thread's where to clone the target TLS
	pthread_t clone_tid = pthread_self();
	Item* clone_element = Find(clone_tid);
	

	// Throw an error if there is no TLS for the original or if the clone already has a TLS
	if(clone_element != NULL || target_element == NULL){
		return -1;
	}

	ThreadLocalStorage* targetTLS = target_element->thread_local_storage;

	// Create a new item for the table
	clone_element = (Item*) malloc(sizeof(Item));
	clone_element->key = clone_tid;

	// Clone the TLS
	clone_element->thread_local_storage = (ThreadLocalStorage*) malloc(sizeof(ThreadLocalStorage));
	clone_element->thread_local_storage->page_num = targetTLS->page_num;
	clone_element->thread_local_storage->size = targetTLS->size;
	
	clone_element->thread_local_storage->pages = (Page**) calloc(clone_element->thread_local_storage->page_num, sizeof(Page*));
	
	for(int i = 0; i < clone_element->thread_local_storage->page_num; i++){
		clone_element->thread_local_storage->pages[i] = targetTLS->pages[i];
		
		(clone_element->thread_local_storage->pages[i]->ref_count)++;
	}

	// Check if the insert into the hash table worked, else throw an error
	if(Add(clone_element)){
		return 0;
	}
	else{
		return -1;
	}
}

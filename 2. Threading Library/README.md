# <ins>Project 2: Threading Library</ins>
The main deliverable for this project is a basic thread system for Linux. In the lectures, we learned that threads are independent units of execution that run (virtually) in parallel in the address space of a single process. As a result, they share the same heap memory, open files (file descriptors), process identifier, etc. Each thread has its own context, which consists of a set of CPU registers and a stack. The thread subsystem provides a set of library functions that applications may use to create, start and terminate threads, and manipulate them in various ways.

The most well-known and widespread standard that specifies a set of interfaces for multi-threaded programming on Unix-style operating systems is called POSIX threads (or pthreads). Note that pthreads merely prescribes the interface of the threading functionality. The implementation of that interface can exist in user-space, take advantage of kernel-mode threads (if provided by the operating system), or mix the two approaches. In this project, you will implement a small subset of the pthread API exclusively in user-mode. In particular, we aim to implement the following three functions from the pthread interface in user mode on Linux (prototypes and explanations partially taken from the respective man pages)

### Functions to Implement

    int pthread_create( 
        pthread_t *thread,
        const pthread_attr_t *attr, 
        void *(*start_routine) (void *), 
        void *arg);

The *pthread_create()* function creates a new thread within a process. Upon successful completion, *pthread_create()* stores the ID of the created thread in the location referenced by *thread*. In our implementation, the second argument (*attr*) shall always be NULL. The thread is created and executes *start_routine* with arg as its sole argument. If the *start_routine* returns, the effect shall be as if there was an implicit call to *pthread_exit()* using the return value of *start_routine* as the exit status. Note that the thread in which *main()* was originally invoked differs from this. When it returns from the effect shall be as if there was an implicit call to *exit()* using the return value of *main()* as the exit status.

    void pthread_exit(
        void *value_ptr);

The *pthread_exit()* function terminates the calling thread. In our current implementation, we ignore the value passed in as the first argument (*value_ptr*) and clean up all information related to the terminating thread. The process shall exit with an exit status of 0 after the last thread has been terminated. The behavior shall be as if the implementation called *exit()* with a zero argument at thread termination time.

    pthread_t pthread_self(void);

The *pthread_self()* function shall return the thread ID of the calling thread. For more details about error handling, please refer to the respective man pages.

### Example Library Usage
    
    static void* my_thread(void* my_arg) { 
        pthread_t my_tid = pthread_self(); // Do something in this thread
        
        if (i_need_to_exit_now) {
            pthread_exit(NULL); // Can exit with another value 
        }
        // Maybe do some more things in this thread
        // Suggestion: do something that takes a long time return NULL; 
        // Can return other values
    }

    int main(void) { 
        pthread_t tid;
        int error_number = pthread_create(
            &tid, NULL, my_thread, NULL /*my_arg*/);
        pthread_t my_tid = pthread_self();
        // my_tid must not equal tid (a different thread)
        // Suggestion: do something that takes a long time, and test that
        // both this and the other long work get to take turns. 
    }

Additionally, see the given *tests/busy_threads.c*. It is an <ins>incomplete test case</ins> (it may still pass when your code is broken). See its comments for suggested improvements.

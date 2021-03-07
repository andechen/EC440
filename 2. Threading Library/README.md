A README file for project 2, Threads.

References:
    https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html#Extended-Asm
    
* In this project, we are creating a library for threads, which implements pthreads API’s create, exit and self functions.

* Thread control block, is a struct that contains the information about the thread, such as its status, the pointer where it points to in the stack, a set of registers, its exit status and a variable indicating that it was created.

* The function schedule() can be called in one of the 3 places, from 
pthread_create(), from pthread_exit() or when a SIGALRM signal has 
been sent. It then loops through all the threads to find the next thread which can be executed, starting at the current thread. When it does
find one, it saves the state of the previous thread, using the setjump() function. To resume running the new thread, the function changes 
the state to TS_RUNNING and uses longjump() to continue the execution.

* The function pthread_create() can be called from the main program, to create a thread and execute it.

* The function scheduler_init() the function is called the first time in pthread_create(). It sets the state of the threat and then it 
sets up a signal using the ualarm() function which signals every turn for a period of time which is indicated in the define statement. This will proceed to execute in a round robin fashion

* The function pthread_exit() will terminate the thread by freeing the memory from the stack, it then checks if there are any other 
threads waiting to execute.

* The function pthread_self() returns the ID of the thread.
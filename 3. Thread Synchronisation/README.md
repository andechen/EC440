# <ins>Project 3: Thread Synchronisation</ins>
In the previous project, we implemented a part of pthreads that enables creation and execution of multiple threads. Now, we are going to add some more features to that library to support interaction between threads through synchronization.

The main features we need to add are:

1. Thread locking and unlocking, to prevent your scheduler from running at specific times.
2. An implementation for pthread barriers, to ensure one thread can wait for another thread
to finish, and to collect a completed thread’s exit status.
3. Support for mutexes in your threads, which will enable mutual exclusion from critical
regions of multithreaded code.

### New Functions to Implement
You will implement functions to support thread synchronization through mutexes and barriers. Each of these features includes init/destroy functions and one or more functions that control the synchronisation state.

### Mutex Functions

    int pthread_mutex_init(
        pthread_mutex_t *restrict ​mutex, 
        const pthread_mutexattr_t *restrict ​attr​);

The ​*pthread_mutex_init()​* function initializes a given ​*mutex​*. The ​*attr​* argument is unused in this assignment (we will always test it with NULL). Behavior is undefined when an already-initialized mutex is re-initialized. Always return 0.

    int pthread_mutex_destroy(
        pthread_mutex_t *​mutex​);

The ​*pthread_mutex_destroy()​* function destroys the referenced ​*mutex*​. Behavior is undefined when a mutex is destroyed while a thread is currently blocked on, or when destroying a mutex that has not been initialized. Behavior is undefined when locking or unlocking a destroyed mutex, unless it has been re-initialized by *​pthread_mutex_init*​. Return 0 on success.

    int pthread_mutex_lock(
        pthread_mutex_t *​mutex)​;

The ​*pthread_mutex_lock()​* function locks a referenced *​mutex​*. If the mutex is not already locked, the current thread acquires the lock and proceeds. If the mutex is already locked, the thread blocks until the mutex is available. If multiple threads are waiting on a mutex, the order that they are awoken is undefined. Return 0 on success, or an error code otherwise.

    int pthread_mutex_unlock(
        pthread_mutex_t *​mutex)​;

The ​*pthread_mutex_unlock()​* function unlocks a referenced *​mutex*​. If another thread is waiting on this mutex, it will be woken up so that it can continue running. Note that when that happens, the woken thread will finish acquiring the lock. Return 0 on success, or an error code otherwise.

### Barrier Functions

    int pthread_barrier_init(
        pthread_barrier_t *restrict ​barrier,​ 
        const pthread_barrierattr_t *restrict ​attr​, 
        unsigned ​count)​;

The ​*pthread_barrier_init()​* function initializes a given ​*barrier*​. The *​attr​* argument is unused in this assignment (we will always test it with NULL). The ​*count​* argument specifies how
many threads must enter the barrier before any threads can exit the barrier. Return 0 on success. It is an error if count is equal to zero (return EINVAL). Behavior is undefined when an already-initialized barrier is re-initialized.

    int pthread_barrier_destroy(
        pthread_barrier_t *​barrier​);

The ​*pthread_barrier_destroy()​* function destroys the referenced *​barrier​*. Behavior is undefined when a barrier is destroyed while a thread is waiting in the barrier or when destroying a barrier that has not been initialized. Behavior is undefined when attempting to wait in a destroyed barrier, unless it has been re-initialized by *​pthread_barrier_init*​. Return 0 on success.

    int pthread_barrier_wait(
        pthread_barrier_t *​barrier​);

The ​*pthread_barrier_wait()​* function enters the referenced *barrier*. The calling thread shall not proceed until the required number of threads (from ​*count*​ in *pthread_barrier_init*) have already entered the barrier. Other threads shall be allowed to proceed while this thread is in a barrier (unless they are also blocked for other reasons. Upon exiting a barrier, the order that the threads are awoken is undefined. Exactly one of the returned threads shall return PTHREAD_BARRIER_SERIAL_THREAD (it does not matter which one). The rest of the threads shall return 0.

We also recommend that you create new *​static void lock()​* and ​*static void unlock()* functions. Your lock function should disable the timer that calls your schedule routine, and unlock should re-enable the timer. You can use the ​*sigprocmask​* function to this end (one function using SIG_BLOCK, the other using SIG_UNBLOCK, with a mask on your alarm signal). Use these functions to prevent your scheduler from running when your threading library is internally​ in a critical section (users of your library will use barriers and mutexes for critical sections that are external to your library).

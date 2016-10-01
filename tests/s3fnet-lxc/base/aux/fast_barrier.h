#if !(defined(macintosh) || defined(Macintosh) || defined(__APPLE__) || defined(__MACH__))
#include <pthread.h>

#define USE_FUTEX

#if defined(_WIN32) || defined(_WIN64)

#include <sched.h>

/* sched_yield is absolutely necessary if dealing with more threads than
 * logical CPU cores. However, it does add some overhead in certain worst
 * cases, particularly when many other processes are running and a thread
 * calls sched_yield() immediately before the arrival of the last thread.
 */
#define INCREMENT_AND_FETCH(X)	_InterlockedIncrement(X)
#define MY_FUTEX_WAIT()	while(tdata[who].local_sense != global_sense) sched_yield();
#define MY_FUTEX_WAKE()

#else

#include <sys/time.h>
#include <unistd.h>

/* futex(int*,int,int,const struct timespec*) is a linux-specific system call
 * providing low-kernel-overhead user-level locking. In this case, a thread
 * sleeps until woken. A similar assembly-based API is used deep within the
 * pthread (NPTL) library.
 */

#define INCREMENT_AND_FETCH(X) __sync_add_and_fetch(X,1)

#ifdef USE_FUTEX
#ifndef __user
#define __user
#endif
#ifndef u32
#define u32 unsigned int
#endif

#include <linux/futex.h>
#include <sys/syscall.h>

#define MY_FUTEX_WAIT()	int gs_copy = global_sense; \
						while(tdata[who].local_sense != global_sense) { \
							syscall(SYS_futex,&global_sense,FUTEX_WAIT,gs_copy,NULL); \
						}
#define MY_FUTEX_WAKE() syscall(SYS_futex,&global_sense,FUTEX_WAKE,count-1,NULL);

#else

#include <sched.h>

#define MY_FUTEX_WAIT()	while(tdata[who].local_sense != global_sense) sched_yield();
#define MY_FUTEX_WAKE()

#endif /* USE_FUTEX */

#endif

#define BARRIER_VALID 1
#ifndef EINVAL
#define EINVAL 2
#endif
#ifndef EBUSY
#define EBUSY  3
#endif

/* Cache line size is important to allow spinning on 'local_sense'
 * (since thread-local storage is only allowed for static members)
 * It can be determined from /sys/devices/system/cpu/cpu0/cache/
 * under 'coherency_line_size'. May define LOCAL_SENSE_PADDING
 * as 0 to remove padding but deal with potential cache
 * coherence problems.
 */
#define CACHE_LINE_SIZE 64
//#define LOCAL_SENSE_PADDING CACHE_LINE_SIZE-sizeof(int)
#define LOCAL_SENSE_PADDING 0

/* Provides atomic access to min/max reduction values of barrier
 * 'B' where 'X' is some code that requires then and includes one
 * or more get_min_value() or get_max_value() calls.
 */
#define BARRIER_AUTOLOCK(B,X) \
    { \
		B.lock(); \
        X \
        B.unlock(); \
    }

/**
* Stores sense of each thread.
* This is a sense-reversing barrier, allowing it to be used an
* infinite number of times without ever getting stuck. On each
* iteration, threads spin (generally with sched_yield) on a
* comparison of their local 'sense' to the barrier's global 'sense'.
* The global sense then reverses on each iteration after all threads
* are released and the process repeats. This structure stores each class's
* local sense with the option of padding for cache performance.
*/
struct thread_data_t {
    /**
    * Thread's local sense.
    * Stores each thread's 'sense' as 0 or 1. Integer is used instead
    * of bool for better performance (machine word) and alignment.
    */
    int local_sense;

    /**
    * Adjustable structure padding.
    * Optional garbage variable to pad each thread's sense for performance
    * purposes (ie, make each thread_data_t the size of a cache line.
    */
    char padding[LOCAL_SENSE_PADDING];
};

/**
* Barrier for low-work synchronization.
* Barrier variant designed to get all threads through its lock as quickly
* as possible (fewest CPU cycles). Can suffer high system time due to
* sched_yield() for large amounts of work per-thread, particularly when
* threads are arriving at very different times.
*/
class fast_barrier_t {
  public:
    /**
    * Barrier constructor.
    * Creates a fast_barrier_t. Barrier still requires initialization
    * with the number of threads via init().
    * @see init()
    */
    fast_barrier_t() {};

    /**
    * Barrier destructor.
    * Destroys a fast_barrier_t. Calls destroy().
    * @see destroy()
    */
    ~fast_barrier_t();

    /**
    * Initializes the barrier to the given number of threads.
    * @param threads number of threads that will be synchronized.
    * @return error status (based on pthread_mutex_init's status)
    */
    int init(int threads);

    /**
    * Destroys the barrier.
    * Member function that acts as the destructor and frees
    * the barrier when we're done with it. It may be reinitialized
    * afterwards, for instance to change the number of threads.
    * @return error status (based on pthread_mutex_destroy's status)
    */
    int destroy();

    /**
    * Waits for all other threads to arrive at the barrier.
    * Wait for all members of a barrier to reach the barrier. Each entry to the
    * barrier offers an ltime_t value.  If negative, it is ignored.  If
    * non-negative it updates the "min_value", "max_value", and "sum_value"
    * entries of the barrier, replacing it if either the min_value is negative
    * (uninitialized) or greater than the offered time with similiar behaviors
    * for max_value and sum as expected.
    * @param tid the thread id
    * @param t a reduction value (timestamp)
    * @return 0 or -1, -1 indicates final thread
    */
    int wait(unsigned tid, ltime_t t);

    /**
    * Gets the minimum reduction value.
    * Returns the minimum time observed for use in calculating the
    * synchronzation window.
    * Warning: The min reduction value may be modified by
    * threads entering the barrier while the barrier is active. If
    * atomic access is needed, use the BARRIER_AUTOLOCK macro.
    * @return min reduction value
    */
    inline ltime_t get_min_value() { return min_reduction_value; }

    /**
    * Gets the maximum reduction value.
    * Returns the maximum time observed for use in calculating the
    * synchronzation window.
    * Warning: The max reduction value may be modified by
    * threads entering the barrier while the barrier is active. If
    * atomic access is needed, use the BARRIER_AUTOLOCK macro.
    * @return max reduction value
    */
    inline ltime_t get_max_value() { return max_reduction_value; }

    /**
    * Gets the sum of reduction values.
    * Returns the sum of all reduction values observed in the previous barrier
    * iteration for use in calculating the synchronzation window.
    * Does not have the same synchronization problems since it gets the sum
    * from the opposite global sense (last iteration).
    * @return previous reduction value sum
    */
    inline ltime_t get_sum_value() { return sum_reduction_value[!global_sense]; }

    /**
    * Locks the reduction value lock.
    * Acquires exclusive access to read/write reduction values.
    * Warning: lock() will freeze barrier progress while active.
    * Use of the BARRIER_AUTOLOCK macro is highly recommended.
    * @see unlock()
    */
    inline void lock() { pthread_mutex_lock(&reduction_lock); }

    /**
    * Unlocks the reduction value lock.
    * Releases exclusive access of read/write reduction values.
    * @see lock()
    */
    inline void unlock() { pthread_mutex_unlock(&reduction_lock); }

  protected:
    /**
    * Number of arrived threads.
    * Atomic variable that keeps track of the number of threads that have
    * arrived at the barrier.
    */
    int counter;

    /**
    * Minimum timestamp observed.
    */
    ltime_t min_reduction_value;

    /**
    * Maximum timestamp observed.
    */
    ltime_t max_reduction_value;

    /**
    * Sum of all reduction values for an iteration.
    * Indexed by global sense. Always returns previous iteration's sum to
    * avoid contention and unnecessary synchronization.
    */
    ltime_t sum_reduction_value[2];

    /**
    * Expected number of threads.
    * Number of threads the barrier will wait for. Set by init().
    * @see init()
    */
    int count;

    /**
    * Padding to keep 'counter' apart from thread data.
    * For best performance, the sense variables MUST NOT be on the same cache
    * line as the atomic barrier counter and reduction values since we spin
    * on them locally and atomic ops will cause a cache line invalidation.
    */
	char padding[CACHE_LINE_SIZE - sizeof(int)];

    /**
    * Barrier's global_sense.
    * Variable for threads to compare their sense to until barrier is released.
    */
	volatile int global_sense;

    /**
    * Dynamically created array of thread local senses.
    */ 
    thread_data_t* tdata;

    /**
    * Lock for synchronizing reduction values.
    * While a lock per variable might allow a slight parallel performance
    * gain in some cases, a single lock is easier and avoids the overhead of
    * acquiring and releasing three subsequent locks.
    */
    pthread_mutex_t reduction_lock;

    /**
    * Updates internal reduction values.
    * Called by wait() to update barrier's internal reduction values.
    * @param t a thread's reduction value, passed by wait()
    */
    void update_reduction_values(ltime_t);
};
#endif

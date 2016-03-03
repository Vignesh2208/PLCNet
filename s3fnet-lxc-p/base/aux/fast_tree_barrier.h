#if !(defined(macintosh) || defined(Macintosh) || defined(__APPLE__) || defined(__MACH__))
#include <pthread.h>

#if defined(_WIN32) || defined(_WIN64)

/* sched_yield is absolutely necessary if dealing with more threads than
 * logical CPU cores. However, it does add some overhead in certain worst
 * cases, particularly when many other processes are running and a thread
 * calls sched_yield() immediately before the arrival of the last thread.
 */
#define INCREMENT_AND_FETCH(X)    _InterlockedIncrement(X)
#define MY_FUTEX_WAIT()    while(tdata[who].local_sense != global_sense) sched_yield();
#define MY_FUTEX_WAKE()

#else

#include <linux/futex.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>

/* futex(int*,int,int,const struct timespec*) is a linux-specific system call providing
 * low-kernel-overhead user-level locking. In this case, a thread sleeps until woken.
 * A similar assembly-based API is used deep within the pthread (NPTL) library.
 */

#define INCREMENT_AND_FETCH(X) __sync_add_and_fetch(X,1)

/* RELEASE_FUTEX follows naturally from the futex concept.
 * ARRIVAL_FUTEX may add unnecessary overhead and be better as sched_yield.
 * WARNING: Using futexes may complicate things for a distributed system
 */
//#define USE_ARRIVAL_FUTEX
//#define USE_RELEASE_FUTEX

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
//#define LOCAL_SENSE_PADDING CACHE_LINE_SIZE-sizeof(int)-sizeof(char)
#define THREAD_LOCAL_PADDING 0

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

#define RELEASE_CHILDREN 2
#define ARRIVAL_CHILDREN 4

// allows for quick-changing type used for child data, ie for cache performance
typedef unsigned char child_data_t;

/**
* Struct representing a tree node.
* A single node for use in a software combining tree. Forms one piece of the
* fast_tree_barrier_t.
*/
struct tree_node_t {
    /**
    * The node's local sense.
    * Functions much more like global sense, with local sense thread-specific.
    * Used for releasing the barrier. It doubles as the release futex if
    * release futexes are enabled, thus it is an integer vs a bool. Children
    * are released by their parent's sense.
    */
    int sense;
#ifdef USE_RELEASE_FUTEX
    /**
    * Pointer to the release parent.
    * Reference to the parent that will release this node in the software
    * combining tree. Only used if release futexes are enabled.
    */
    tree_node_t* release_parent;

#else
    /**
    * Pointer to the children to be released.
    * Reference list of the children that this node needs to release when it
    * is released (or on arrival for root node). Only necessary if release
    * futexes are NOT enabled.
    */
    tree_node_t* release_children[RELEASE_CHILDREN];

#endif
    /**
    * Pointer to the arrival parent.
    * Reference to the parent that is waiting on this node. Used to alert the
    * parent that this node has arrived and is waiting to be released.
    */
    tree_node_t* arrival_parent;

    /**
    * Number of expected arrivals.
    * The number of arrival children this node has. This represents the number
    * of tree_node_t's that must signal this node before it is ready to release.
    */
    int arrivals_expected;

    /** Current number of arrivals.
    * The number of nodes that have already arrived and have signaled this node
    * (their parent) that they are waiting to be released. This is atomically
    * incremented by each child node and doubles as the arrival_futex if arrival
    * futexes are enabled, in which case it causes ARRIVAL_CHILDREN wakes
    * instead of one at the proper time. This can still be preferrable to
    * spinning, even with sched_yield.
    */
    int arrival_count; // incremented by each child; doubles as arrival futex - causes ARRIVAL_CHILDREN wakes
};

/**
* Structure to store each thread's local sense.
* Stores each thread's local sense with optional padding for cache performance.
* NOTE: It may be possible to include this structure in tree_node_t.
*/
struct tree_thread_data_t {
    /**
    * Thread's local sense,
    */
    int sense;

    /**
    * Optional structure padding
    * Padding used to (optionally) optimize cache line performance by preventing
    * threads from spinning on the sense of other threads.
    */
    char padding[THREAD_LOCAL_PADDING];
};

/**
* Class representing a software combining tree.
* Creates a software combining tree barrier useful for synchronizing very large
* numbers of threads, often in a distributed system. Excellent for extremely
* large amounts of work per thread and uneven arrival times. Staggers release
* of nodes to prevent cache invalidation floods.
*/
class fast_tree_barrier_t {
  public:
    /**
    * Barrier constructor.
    * Creates a fast_tree_barrier_t. The barrier still needs initialization
    * with init().
    * @see init()
    */
    fast_tree_barrier_t() {};

    /**
    * Barrier destructor.
    * Destroys a fast_tree_barrier_t. Calls destroy().
    * @see destroy()
    */
    ~fast_tree_barrier_t();

    /**
    * Initializes the barrier to the given number of threads.
    * @param threads number of threads that will be synchronized.
    * @return error status (based on pthread_mutex_init's status)
    */
    int init(int);

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
    int wait(unsigned tid, ltime_t t=-1);

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
    inline ltime_t get_sum_value() { return sum_reduction_value[(int)(!tdata[0].sense)]; }

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
    * Expected number of threads.
    * Number of threads the barrier will wait for. Set by init().
    * @see init()
    */
    int count;

    /**
    * Array of tree node pointers.
    * Dynamically allocated array of tree node references. Each node
    * represents a thread.
    */
    tree_node_t* shared_nodes;

    /**
    * Array of thread local-sense pointers.
    * Dynamically allocated array of local-sense references for each thread.
    * Separated from the actual nodes for better performance spinning. In a
    * distributed system, these would also stay local to the machine, while
    * shared_nodes would likely take on a slightly different form.
    */
    tree_thread_data_t* tdata;

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

    /**
    * Static wrapper function for checking if a node's childre are ready.
    * WARNING: May not be implemented; futex functionality largely obseletes
    */
    static child_data_t children_not_ready(tree_node_t node);
};
#endif

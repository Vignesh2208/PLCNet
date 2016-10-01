/*
 * barrier.cc 
 *  
 * A barrier causes threads to wait until a set of threads has
 * all "reached" the barrier. The number of threads required is
 * set when the barrier is initialized, and cannot be changed
 * except by reinitializing.
 *
 * The barrier_init() and barrier_destroy() functions,
 * respectively, allow you to initialize and destroy the
 * barrier.
 *
 * The barrier_wait() function allows a thread to wait for a
 * barrier to be completed. One thread (the one that happens to
 * arrive last) will return from barrier_wait() with the status
 * -1 on success -- others will return with 0. The special
 * status makes it easy for the calling code to cause one thread
 * to do something in a serial region before entering another
 * parallel section of code.
 *
 * min_value_reset initializes the min value to -1 so that the
 * reduction can be used for the next synchronization
 */
#if !(defined(macintosh) || defined(Macintosh) || defined(__APPLE__) || defined(__MACH__))
#include <s3f.h>
#include <cstdio>
#include <unistd.h>

fast_barrier_t::~fast_barrier_t() {
  destroy();
}

/*
 * Initialize a barrier for use.  Should make this a min reduction...?
 */
int fast_barrier_t::init (int threads)
{
	counter = 0;
    count = threads; 
    global_sense = 0;

    tdata = new thread_data_t[threads];
    if(!tdata) return -1;
    for(int i=0; i < threads; i++) {
        tdata[i].local_sense = 0;
    }

    min_reduction_value = -1;
    max_reduction_value = 0;
    sum_reduction_value[0] = 0;
    sum_reduction_value[1] = 0;
    int err = pthread_mutex_init(&reduction_lock,NULL);

    return err;
}

/*
 * Destroy a barrier when done using it.
 */
int fast_barrier_t::destroy()
{
    printf("asked to delete %lx \n", (unsigned long)tdata);
    if(tdata) delete [] tdata;
    tdata = NULL;
    int err = pthread_mutex_destroy(&reduction_lock);

    return err;
}

/*
 * Wait for all members of a barrier to reach the barrier. Each entry to the
 * barrier offers an ltime_t value.  If negative, it is ignored.  If non-negative
 * it updates the "min_value", "max_value", and "sum_value" entries of the barrier,
 * replacing it if either the min_value is negative (uninitialized) or greater than
 * the offered time with similiar behaviors for max_value and sum as expected.
 */
int fast_barrier_t::wait (unsigned tid, ltime_t t) {
    update_reduction_values(t);

    // safety mechanism to allow use with subsets of threads
	unsigned who = tid % count;
    tdata[who].local_sense ^= 1;
    if(INCREMENT_AND_FETCH(&counter) == count) {
        counter = 0; // reset barrier counter
        sum_reduction_value[tdata[who].local_sense] = 0; // reset alternate sum for next iteration
        global_sense = tdata[who].local_sense; // release other threads
		MY_FUTEX_WAKE()
        return -1;
    } else {
        MY_FUTEX_WAIT()
		//while(tdata[who].local_sense != global_sense);
        return 0;
    }
}

inline void fast_barrier_t::update_reduction_values(ltime_t t) {
    // if t is non-negative then use it to update min, max, sum
    if( t >= 0 ) {
        pthread_mutex_lock(&reduction_lock);
        if( min_reduction_value == -1 || t < min_reduction_value ) {
            min_reduction_value = t;
        }

        if( t > max_reduction_value ){ 
            max_reduction_value = t;
        }

        sum_reduction_value[global_sense] += t;
        pthread_mutex_unlock(&reduction_lock);
    }
}
#endif

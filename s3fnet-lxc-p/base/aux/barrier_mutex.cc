/**
 * \file barrier_mutex.cc
 * Source file for the barrier_mutex_t class
 */

#include <s3f.h>

barrier_mutex_t::~barrier_mutex_t() {
  destroy();
}

/*
 * Initialize a barrier for use.  Should make this a min reduction...?
 */
int barrier_mutex_t::init (int count)
{
    int status;

    threshold = counter = count;
    cycle = 0;
    status = pthread_mutex_init (&mutex, NULL);
    if (status != 0)
        return status;
    status = pthread_cond_init (&cv, NULL);
    if (status != 0) {
        pthread_mutex_destroy (&mutex);
        return status;
    }
    valid = BARRIER_VALID;
    min_value = -1;
    reduction_value = -1; 
    return 0;
}

/*
 * Destroy a barrier when done using it.
 */
int barrier_mutex_t::destroy()
{
    int status, status2;

    if (valid != BARRIER_VALID)
        return EINVAL;

    status = pthread_mutex_lock (&mutex);
    if (status != 0)
        return status;

    /*
     * Check whether any threads are known to be waiting; report
     * "BUSY" if so.
     */
    if (counter != threshold) {
        pthread_mutex_unlock (&mutex);
        return EBUSY;
    }

    valid = 0;
    status = pthread_mutex_unlock (&mutex);
    if (status != 0)
        return status;

    /*
     * If unable to destroy either 1003.1c synchronization
     * object, return the error status.
     */
    status = pthread_mutex_destroy (&mutex);
    status2 = pthread_cond_destroy (&cv);
    return (status == 0 ? status : status2);
}

/*
 * Wait for all members of a barrier to reach the barrier. Each entry to the
 * barrier offers an ltime_t value.  If negative, it is ignored.  If positive,
 * it updates the "min_value" entry of the barrier, replacing it if either the
 * min_value is negative (initialized) or greater than the offered time.  When
 * the count (of remaining members) reaches 0, broadcast to wake
 * all threads waiting.
 */
int barrier_mutex_t::wait (ltime_t t)
{
    int status, cancel, tmp, my_cycle;
    if (valid != BARRIER_VALID)
        return EINVAL;

    status = pthread_mutex_lock (&mutex);
    if (status != 0)
        return status;

    my_cycle = cycle;   /* Remember which cycle we're on */

    // update min_time
    if( !(t<0) && (min_value < 0 || min_value > t )) min_value = t;

    if (--counter == 0) {
        cycle = !cycle;
        counter = threshold;
	
	// before signaling all the threads, copy the reduced
	// value into the reduction_value variable. 
	reduction_value = min_value;
	
	// initialize min_value for the next use of the barrier
	min_value = -1;

        status = pthread_cond_broadcast (&cv);
        /*
         * The last thread into the barrier will return status
         * -1 rather than 0, so that it can be used to perform
         * some special serial code following the barrier.
         */
        if (status == 0)
            status = -1;
    } else {
        /*
         * Wait with cancellation disabled, because barrier_wait
         * should not be a cancellation point.
         */
        pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, &cancel);

        /*
         * Wait until the barrier's cycle changes, which means
         * that it has been broadcast, and we don't want to wait
         * anymore.
         */
        while (my_cycle == cycle) {
            status = pthread_cond_wait (
                    &cv, &mutex);
            if (status != 0) break;
        }

        pthread_setcancelstate (cancel, &tmp);
    }
    /*
     * Ignore an error in unlocking. It shouldn't happen, and
     * reporting it here would be misleading -- the barrier wait
     * completed, after all, whereas returning, for example,
     * EINVAL would imply the wait had failed. The next attempt
     * to use the barrier *will* return an error, or hang, due
     * to whatever happened to the mutex.
     */
    pthread_mutex_unlock (&mutex);
    return status;          /* error, -1 for waker, or 0 */
}



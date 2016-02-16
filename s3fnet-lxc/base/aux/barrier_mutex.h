/**
 * \file barrier_mutex.h
 *
 * \brief A barrier implementation using pthread_mutex.
 */

#define BARRIER_VALID 1
#ifndef EINVAL
#define EINVAL 2
#endif
#ifndef EBUSY
#define EBUSY  3
#endif

/**
 * A barrier implementation using pthread_mutex.
 * A barrier causes threads to wait until a set of threads has all "reached" the barrier.
 * The number of threads required is set when the barrier is initialized, and cannot be changed
 * except by reinitializing.
 *
 * The barrier_init() and barrier_destroy() functions,
 * respectively, allow you to initialize and destroy the
 * barrier.
 *
 * The barrier_wait() function allows a thread to wait for a
 * barrier to be completed. One thread (the one that happens to
 * arrive last) will return from barrier_wait() with the status
 * -1 on success and others will return with 0. The special
 * status makes it easy for the calling code to cause one thread
 * to do something in a serial region before entering another
 * parallel section of code.
 *
 * min_value_reset initializes the min value to -1 so that the
 * reduction can be used for the next synchronization.
 *
 *  The barrier_mutex_t class is derived from code obtained at
 *     http://ptgmedia.pearsoncmg.com/images/0201633922/sourcecode/barrier.c
 *  authorship is not given in the file, and we were not able to deduce it from the link
 */
class barrier_mutex_t {
public:
	barrier_mutex_t( ) {};
	~barrier_mutex_t();
	/** Initialize a barrier for use. */
	int init(int);
	/** Destroy a barrier when done using it. */
	int destroy();
	/**
	 * Wait for all members of a barrier to reach the barrier. Each entry to the
	 * barrier offers an ltime_t value.  If negative, it is ignored.  If positive,
	 * it updates the "min_value" entry of the barrier, replacing it if either the
	 * min_value is negative (initialized) or greater than the offered time.  When
	 * the count (of remaining members) reaches 0, broadcast to wake
	 * all threads waiting.
	 */
	int wait(ltime_t);
	ltime_t get_min_value() { return reduction_value; }
private:
	int threshold;
	int cycle;
	int counter;
	int valid;
	ltime_t min_value;
	ltime_t reduction_value;
	pthread_mutex_t mutex;
	pthread_cond_t cv;
};


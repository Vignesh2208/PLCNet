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
#include <sched.h>
#include <unistd.h>
#include <cstring>

fast_tree_barrier_t::~fast_tree_barrier_t() {
  destroy();
}

/*
 * Initialize a barrier for use.  Should make this a min reduction...?
 */
int fast_tree_barrier_t::init (int threads)
{
	int err = 0;
	count = threads; 
	shared_nodes = new tree_node_t[threads];
	if(shared_nodes) {
		int i,j,index;
		for(i = 0;i < threads;i++) {
			shared_nodes[i].sense = 0;
			shared_nodes[i].arrival_count = 0;
			shared_nodes[i].arrivals_expected = 0;
			if(i == 0) {
#ifdef USE_RELEASE_FUTEX
				shared_nodes[0].release_parent = NULL;
#endif /* USE_RELEASE_FUTEX */
				shared_nodes[0].arrival_parent = NULL;
			} else {
#ifdef USE_RELEASE_FUTEX
				shared_nodes[i].release_parent = &shared_nodes[(i-1)/RELEASE_CHILDREN];
#endif /* USE_RELEASE_FUTEX */
				shared_nodes[i].arrival_parent = &shared_nodes[(i-1)/ARRIVAL_CHILDREN];
			}

#ifndef USE_RELEASE_FUTEX
			for(j = 0;j < RELEASE_CHILDREN;j++) {
				index = RELEASE_CHILDREN*i+j+1;
				if(index < threads) {
					shared_nodes[i].release_children[j] = &shared_nodes[index];
				} else {
					shared_nodes[i].release_children[j] = NULL;
				}
			}
#endif /* NOT USE_RELEASE_FUTEX */

			for(j = 0;j < ARRIVAL_CHILDREN;j++) {
                /* Original document appears to be incorrect, should be:
                 * ARRIVAL_CHILDREN*i+j+1 (consider p0 with 1 thread) */
				//shared_nodes[i].have_child[j] = (ARRIVAL_CHILDREN*i+j < threads) ? true : false;
                shared_nodes[i].arrivals_expected += (ARRIVAL_CHILDREN*i+j+1 < threads) ? 1 : 0;
			}
		}
	} else {
		err |= 1;
	}

	tdata = new tree_thread_data_t[threads];
	if(!tdata) err |= 2;
    for(int i=0; i < threads; i++) {
        tdata[i].sense = 1;
    }

    min_reduction_value = -1;
    max_reduction_value = 0;
    sum_reduction_value[0] = 0;
    sum_reduction_value[1] = 0;
    err |= pthread_mutex_init(&reduction_lock,NULL);

    return err;
}

/*
 * Destroy a barrier when done using it.
 */
int fast_tree_barrier_t::destroy()
{
    int err = 0;

    printf("asked to delete %lx \n", (unsigned long)tdata);
	if(shared_nodes) delete [] shared_nodes;
	shared_nodes = NULL;
    if(tdata) {
        delete [] tdata;
        err = pthread_mutex_destroy(&reduction_lock); // bundled to prevent dup free
    }
    tdata = NULL;

    return err;
}

/*
 * Wait for all members of a barrier to reach the barrier. Each entry to the
 * barrier offers an ltime_t value.  If negative, it is ignored.  If non-negative
 * it updates the "min_value", "max_value", and "sum_value" entries of the barrier,
 * replacing it if either the min_value is negative (uninitialized) or greater than
 * the offered time with similiar behaviors for max_value and sum as expected.
 */
int fast_tree_barrier_t::wait (unsigned tid, ltime_t t) {
    update_reduction_values(t);

    // safety mechanism to allow use with subsets of threads
	unsigned who = tid % count;

	//printf("WHO: %u arrived (count=%d)\n",who,count); // DEBUG

#ifdef USE_ARRIVAL_FUTEX
    int arrivals;
#endif

	while(shared_nodes[who].arrival_count < shared_nodes[who].arrivals_expected) {
#ifdef USE_ARRIVAL_FUTEX
        arrivals = shared_nodes[who].arrival_count;
	    syscall(SYS_futex,&shared_nodes[who].arrival_count,FUTEX_WAIT,arrivals,NULL);
#else
		sched_yield();
#endif
	}

	shared_nodes[who].arrival_count = 0;

	if(who != 0) {
		INCREMENT_AND_FETCH(&shared_nodes[who].arrival_parent->arrival_count);
#ifdef USE_ARRIVAL_FUTEX
		syscall(SYS_futex,&shared_nodes[who].arrival_parent->arrival_count,FUTEX_WAKE,1,NULL);
#endif

#ifdef USE_RELEASE_FUTEX
        int sense_copy;
#endif

#ifdef USE_RELEASE_FUTEX
		while(shared_nodes[who].release_parent->sense != tdata[who].sense) {
            sense_copy = shared_nodes[who].release_parent->sense;
            syscall(SYS_futex,&shared_nodes[who].release_parent->sense,FUTEX_WAIT,sense_copy,NULL);
#else
		while(shared_nodes[who].sense != tdata[who].sense) {
			sched_yield();
#endif
		}
	} else {
		sum_reduction_value[(int)(!tdata[0].sense)] = 0; // reset alternate sum for next iteration
	}

	//printf("WHO: %u was released (count=%d)\n",who,count); // DEBUG

#ifdef USE_RELEASE_FUTEX
	shared_nodes[who].sense = !shared_nodes[who].sense;
	syscall(SYS_futex,&shared_nodes[who].sense,FUTEX_WAKE,RELEASE_CHILDREN,NULL);
#else
	tree_node_t* rchild;
	for(int i=0;i<RELEASE_CHILDREN;i++) {
		rchild = shared_nodes[who].release_children[i];
		if(rchild) {
			rchild->sense = tdata[who].sense;
		}
	}
#endif

	tdata[who].sense = !tdata[who].sense;

	//printf("WHO: %u left (count=%d)\n",who,count); // DEBUG

	return 0;
}

inline void fast_tree_barrier_t::update_reduction_values(ltime_t t) {
    // if t is non-negative then use it to update min, max, sum
    if( t >= 0 ) {
        pthread_mutex_lock(&reduction_lock);
        if( min_reduction_value == -1 || t < min_reduction_value ) {
            min_reduction_value = t;
        }

        if( t > max_reduction_value ){ 
            max_reduction_value = t;
        }

        sum_reduction_value[tdata[0].sense] += t;
        pthread_mutex_unlock(&reduction_lock);
    }
}
#endif

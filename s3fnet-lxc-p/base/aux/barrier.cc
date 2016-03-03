/**
 * \file barrier.cc
 * Source file for the barrier_t class
 */

#include <s3f.h>
#include <sched.h>
#include <assert.h>

barrier_t::~barrier_t() {
  destroy();
}

/*
 * Initialize a barrier for use.  Should make this a min reduction...?
 */
int barrier_t::init (int threads)
{
    int i;
    count = threads; 
    cycle = 0;

    calls = new long[threads];
    for(i=0; i< threads; i++) 
        calls[i] = 0;

    values = new ltime_t [threads];
    for(i=0; i< threads; i++) 
            values[i] = 0;
    return 0;
}

/*
 * Destroy a barrier when done using it.
 */
int barrier_t::destroy()
{
  //printf("asked to delete %lx and %lx\n", (unsigned long)calls, (unsigned long)values);
  if(calls)  delete [] calls;
  if(values) delete [] values;
  calls = NULL;
  values = NULL;
  return 0;
}

/*
 * Wait for all members of a barrier to reach the barrier. Each entry to the
 * barrier offers an ltime_t value.  If negative, it is ignored.  If non-negative
 * it updates the "min_value", "max_value", and "sum_value" entries of the barrier, replacing it if either the
 * min_value is negative (initialized) or greater than the offered time.  When
 * the count (of remaining members) reaches 0, broadcast to wake
 * all threads waiting.
 */
int barrier_t::wait (unsigned who, ltime_t t)
{
    ltime_t lt_min_value, lt_max_value, lt_sum_value;
    lt_min_value = lt_max_value = t;
    lt_sum_value = 0;

    // store the passed in value
    // assert( !(count < 0) && (count < 16) );
    // assert( !(who<0) && (who < count) );
    values[who] = t;

    // increment caller's call count
    long mycalls = ++calls[who];

    // compute the minimum call count
    for(int i=1; i< count; i++)  {
      // wait for caller (i+who)%count to reach as far as caller 'who' has come 
      while( calls[(i+who)%count] < mycalls ) {
		sched_yield();
      }

      // if values[i] is non-negative then use it to update min, max, sum
      if( !(values[i] < 0 ) ) {
        if( lt_min_value<0 || values[i] < lt_min_value ) lt_min_value = values[i];
        if( lt_max_value<0 || values[i] > lt_max_value ) lt_max_value = values[i];
        lt_sum_value += values[i];
      }
    }  

    // at this point every caller has checked in. Caller 0 copies its local
    // min, max, sum into the barrier's data structure, and increments the "calls"
    // variable, the key for all to release and go
    if( !who ) {

       min_reduction_value = lt_min_value;
       max_reduction_value = lt_max_value;
       sum_reduction_value = lt_sum_value;

       cycle++; 
       return -1;
    } else 
       while( cycle < mycalls ) sched_yield();
       return 0;
}


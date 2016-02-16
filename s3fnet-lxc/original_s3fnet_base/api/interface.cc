#include <s3f.h>
using namespace std;
using namespace s3f;
/**
 * \file interface.cc
 *
 * \brief S3F Interface class methods
 *
 * authors : David Nicol, Dong (Kevin) Jin
 */

// time measurement functions
#if defined(HAVE_GETHRTIME)
/* Wall time clock is in nanoseconds. It should be good enough! */

unsigned long get_wall_time()
{
	return (unsigned long)gethrtime();
}

double wall_time_to_sec(unsigned long t)
{
	return t/1e9;
}

#else

/**  Wall time clock is in microseconds. It's the best precision we can get from gettimeofday function. */
unsigned long get_wall_time()
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return (unsigned long)(tv.tv_sec*1e6+tv.tv_usec);
}
#endif

void * proxy_function(void * timeline_ptr) {
	return static_cast<Timeline *>(timeline_ptr)->thread_function();
}

TimelineInterface::~TimelineInterface() {
#ifdef PTHREAD_BARRIER
	pthread_barrier_destroy(&window_barrier);
	pthread_barrier_destroy(&top_barrier);
	pthread_barrier_destroy(&bottom_barrier);
#else
	window_barrier.destroy();
	top_barrier.destroy();
	bottom_barrier.destroy();
#endif
}

Interface::Interface(int num_timelines, int ltps) : 
			__num_timelines(num_timelines), __clock(0), __log_ticks_per_sec(ltps)
{

	// set the time-scale in the interface
	__tli.put_log_ticks_per_sec( ltps );
	__tli.__num_Timelines = num_timelines;
	//
	// for each timeline create a thread whose body is the "thread_function"
	// method of Timeline.  Save here both the pointer to the Timeline and
	// the handle for the thread
	//
	// we use a barrier synchronization method to construct
	// alternating bursts where either the simulation threads are
	// running and the main thread waits, or vice-versa
	//   Important to create this before the Timelines are created,
	// because they will immediately synch on it while the main thread runs
	//
#ifdef PTHREAD_BARRIER
	pthread_barrier_init(&__tli.window_barrier, NULL, num_timelines+1);
	pthread_barrier_init(&__tli.top_barrier, NULL, num_timelines);
	pthread_barrier_init(&__tli.bottom_barrier, NULL, num_timelines);
#else
	__tli.window_barrier.init(num_timelines+1);    // only barrier control interacts with
	__tli.top_barrier.init(num_timelines);         // top_ and bottom_ used by simulation threads
	__tli.bottom_barrier.init(num_timelines);
#endif
	// initialized the class mutexes for S3F object classes

	pthread_mutex_init(&Process::process_class_mutex, NULL);
	pthread_mutex_init(&Entity::entity_class_mutex, NULL);
	pthread_mutex_init(&OutChannel::outchannel_class_mutex, NULL);
	pthread_mutex_init(&InChannel::inchannel_class_mutex, NULL);
	pthread_mutex_init(&Timeline::timeline_class_mutex, NULL);
#ifdef PTHREAD_BARRIER
	pthread_mutex_init(&Timeline::bottom_barrier_min_value_mutex, NULL);
#endif
	__start_build_time = get_wall_time();
	__start_run_time = 0;
	__end_run_time = 0;
	__acc_utime = 0;
	__acc_stime = 0;

	for(int i=0; i<num_timelines; i++) {
		pthread_t t1;
		Timeline* tline = new Timeline(&__tli);

		//
		// note that proxy_function above is just a cut-out
		// to get around fact that class method cannot _directly_ be
		// used as thread body
		//
		pthread_create(&t1, NULL, proxy_function, tline);

		// create a structure that holds the pointer to the Timeline and
		// to its associated pthread. Store it
		//
		TimelineThread tlt(tline, t1);
		__timeline_threads.push_back( tlt );
	}
}

/* call the init function of every entity.  Serialize this by stepping through the timelines, one by one */
void Interface::InitModel() {
	unsigned int num_timelines = __timeline_threads.size();
	for(unsigned int t=0; t<num_timelines; t++) {
		Timeline* tl = __timeline_threads[t].get_timeline();
		for(unsigned int e = 0;  e< tl->__entity_list.size(); e++ ) {
			tl->__entity_list[e]->init();
		}
		// tl->__in_appt.resize(num_timelines);
		// tl->__out_appt.resize(num_timelines);
	}

	// This initialization process will have identified on each timeline
	// the value of the smallest cross-timeline minimum write time value.
	//  Find the smallest such
	//
	ltime_t mxtd = -1;
	ltime_t thrs;

	for(unsigned int t=0; t<__timeline_threads.size(); t++ ) {
		Timeline* tl = __timeline_threads[t].get_timeline();
		ltime_t thrs  = tl->min_sync_cross_timeline_delay(-1);
		if( thrs != -1 && (mxtd < 0 || thrs < mxtd) ) mxtd = thrs;
	}
	// now mxtd has the minimum cross timeline delay among all connections.
	// as a heuristic, set the window threshold equal to the number
	// of timelines times this minimum
#ifdef COMPOSITE_SYNC
	thrs = Timeline::__num_timelines*mxtd; //**** Why ?
#else
	thrs = 0;
#endif

	// printf("true min x timeline delay is %ld, window size is %ld\n", mxtd, thrs);

	// for each timeline recompute it's minimum synchronous cross-timeline delay
	for(unsigned int t=0; t<__timeline_threads.size(); t++ ) {
		Timeline* tl = __timeline_threads[t].get_timeline();
		tl->__min_sync_cross_timeline_delay  = tl->min_sync_cross_timeline_delay(thrs);
		tl->__window_size = thrs;

		// initialize the composite synchronization structure using this threshold
		//
		tl->initialize_appt(true, thrs);
		for(unsigned int e = 0;  e< tl->__entity_list.size(); e++ ) {
			tl->__channels_at_minimum
			= tl->count_min_delay_crossings(thrs);
		}
	}
}

Timeline* Interface::get_Timeline(unsigned int tl) {
	if( __num_timelines <= tl ) return (Timeline*)NULL;
	return __timeline_threads[tl].get_timeline();
}


// we do have to kill off the pthreads created by the Interface
Interface::~Interface() {
#ifdef SCHED_YIELD_BARRIER
	//wait for all the simulation thread to quit
	__tli.window_barrier.wait(__num_timelines,(ltime_t)(-1));
#endif
	for(unsigned int i=0; i< __timeline_threads.size(); i++) {
		pthread_cancel( __timeline_threads[i].get_pthread() );
	}
}

void Interface::BuildModel( vector<string> vs ) {}


ltime_t Interface::advance(ltime_t t) {
	return advance_body(STOP_BEFORE_TIME, t, STOP_ON_ANY);
}

ltime_t Interface::advance(next_action na, ltime_t t) {
	return advance_body(STOP_BEFORE_TIME, t, STOP_ON_ANY);
}

ltime_t Interface::advance(stop_cond sc, ltime_t t) {
	return advance_body(STOP_FUNCTION, t, sc);
}


// direct the simulation to advance another t units of simulation time.
// When more than one simulation thread is involved,
// Cross-timeline constraints may inhibit this however,
// so the return value is how the simulation actually advanced
//   nxt_act = STOP_BEFORE_TIME allows for time to advance as far
// as t when there is one simulation thread, 
// nxt_act = SYNCH_WINDOW explicitly advances only as far
// as the window allows.  nxt_act == GLOBAL advances
//
ltime_t Interface::advance_body(next_action nxt_act, ltime_t t , stop_cond sc) { 

	if( !__start_run_time ) {
		__start_run_time = get_wall_time();
	}

	// measure system use now
	struct rusage use_data;
	getrusage(0, &use_data);
	timeval tim = use_data.ru_utime;
	__srt_utime = tim.tv_sec*1000000 + tim.tv_usec;

	tim = use_data.ru_stime;
	__srt_stime = tim.tv_sec*1000000 + tim.tv_usec;


	// set the interface up for the threads
	__tli.put_next_action( nxt_act );
	__tli.put_stop_before( clock()+t );
	__tli.put_stop_cond( sc );

	// Now release the threads to do the simulation
	// a synchronization window size
#ifdef MUTEX_BARRIER
	__tli.window_barrier.wait((ltime_t)(-1));
#endif
#ifdef SCHED_YIELD_BARRIER
	__tli.window_barrier.wait(__num_timelines,(ltime_t)(-1));
#endif	
#ifdef PTHREAD_BARRIER
	pthread_barrier_wait( &(__tli.window_barrier) );
#endif

	// and wait for them to finish the window
#ifdef MUTEX_BARRIER
	__tli.window_barrier.wait((ltime_t)(-1));
#endif
#ifdef SCHED_YIELD_BARRIER
	__tli.window_barrier.wait(__num_timelines,(ltime_t)(-1));
#endif
#ifdef PTHREAD_BARRIER
	pthread_barrier_wait( &(__tli.window_barrier) );
#endif

#ifndef PTHREAD_BARRIER
	__clock = __tli.window_barrier.get_min_value();	
#else
	//just set clock to epoch value as for now, do not want to use mutex here
	__clock = __tli.get_stop_before();
#endif
	__end_run_time = get_wall_time();

	getrusage(0, &use_data);
	tim = use_data.ru_utime;
	unsigned long utime = tim.tv_sec*1000000 + tim.tv_usec;

	tim = use_data.ru_stime;
	unsigned long stime = tim.tv_sec*1000000 + tim.tv_usec;

	tim = use_data.ru_stime;
	__acc_utime += (utime - __srt_utime);
	__acc_stime += (stime - __srt_stime);
	return __clock;
} 	

unsigned long Interface::sim_exc_time() {
	return( __end_run_time - __start_run_time );
}

unsigned long Interface::full_exc_time() {
	return( __end_run_time - __start_build_time );
}

void Interface::runtime_measurements() {
	unsigned long sum_executed = 0;
	unsigned long work_executed = 0;
	unsigned long sync_executed = 0;
	for(unsigned int i=0; i<__num_timelines; i++) {
		Timeline *tl = get_Timeline(i);
		sum_executed  += tl->get_executed();
		work_executed += tl->get_work_executed();
		sync_executed += tl->get_sync_executed();
	}
	printf("-------------- runtime measurements ----------------\n");
	printf("Simulation run of %g sim seconds, with %d timelines\n",
			((double)__clock)/pow(10.0,__tli.get_log_ticks_per_sec()),
			__num_timelines);

#ifdef SAFE_EVT_PTR
	printf("\tconfigured with smart pointer events\n");
#else
	printf("\tconfigured without smart pointer events\n");
#endif

#ifdef SAFE_MSG_PTR
	printf("\tconfigured with smart pointer activations\n");
#else
	printf("\tconfigured without smart pointer activations\n");
#endif

	printf("total run time is %g seconds\n", full_exc_time()/1e6);
	printf("simulation run time is %g seconds\n", sim_exc_time()/1e6);
	printf("accumulated usr time is %g seconds\n", 
			__acc_utime/(1e6*get_numTimelines()));
	printf("accumulated sys time is %g seconds\n", __acc_stime/1e6);
	printf("total evts %ld, work evts %ld, sync evts %ld\n",
			sum_executed, work_executed, sync_executed);

	printf("total exec evt rate %g, work evt rate %g\n",
			sum_executed*1e6/sim_exc_time(),
			work_executed*1e6/sim_exc_time());
	printf("----------------------------------------------------\n");
}


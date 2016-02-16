/**
 * \file interface.h
 *
 * \brief S3F Interface
 *
 * authors : David Nicol, Dong (Kevin) Jin
 */

#ifndef __INTERFACE_H__
#define __INTERFACE_H__

#ifndef __S3F_H__
#error "interface.h can only be included by s3f.h"
#endif


/**
 * TimelineThread is a helper class that sorts an associated Timeline pointer and pthread.
 * Seems like a natural for the pair<> macro but the compiler had a fit about that.
 */

class TimelineThread {
public:
	TimelineThread(Timeline* tl, pthread_t p) :  __tl(tl), __p(p) {};
	~TimelineThread() { };

	Timeline* get_timeline() { return __tl; }
	pthread_t get_pthread() { return __p; }

protected:
	Timeline*	__tl;
	pthread_t     __p;
};

/* typedef helps simplify some expressions later */
// typedef bool(*ptr2StopFunc)(TimelineInterface*);

/* right now the only thing the Interface can request is a block of simulation
   time.  That could change, e.g., provide a pointer to a function that gives
   a termination condition.
 */
enum next_action { STOP_BEFORE_TIME, STOP_FUNCTION };
enum stop_cond   { STOP_ON_ANY, STOP_ON_ALL };

/**
 * TimelineInterface is another helper class, that carries information from the Interface class instance to be read by Timelines.
 * In particular it holds
 * <BR> -- the simulation time up to which the Timelines ought next simulate
 * <BR> -- the "type" of the next epoch of simulation
 * <BR> -- a function called to determine whether to continue the simulation epoch
 * <BR> -- the time-scale of the simulation clock ticks
 *
 * One of these is created, and when the Timelines are created each is given a pointer to it
 */

class TimelineInterface {
public:
	friend class Timeline;

	TimelineInterface() : __next_action(STOP_FUNCTION), __stop_cond(STOP_ON_ANY) {}
	TimelineInterface( stop_cond sc ) : __next_action(STOP_FUNCTION), __stop_cond(sc) {}

	virtual ~TimelineInterface();

	next_action __next_action; ///< type of next synchronization
	stop_cond   __stop_cond;

	ltime_t   __stop_before; ///< time up to which the Timelines ought to simulate
	int       __num_Timelines;

	// barrier synchronizations used by the control thread and
	// Timeline threads for synchronization and epoch windows
	//
#ifdef MUTEX_BARRIER
	barrier_mutex_t window_barrier;        ///< synchronize control and simulation threads
	barrier_mutex_t top_barrier;           ///< top_barrier synchronize only simulation threads
	barrier_mutex_t bottom_barrier;        ///< bottom_barrier synchronize only simulation threads
#endif

#ifdef PTHREAD_BARRIER
	pthread_barrier_t window_barrier; ///< synchronize control and simulation threads
	pthread_barrier_t top_barrier; ///< top_barrier synchronize only simulation threads
	pthread_barrier_t bottom_barrier;  ///< bottom_barrier synchronize only simulation threads
#endif
#ifdef SCHED_YIELD_BARRIER
	barrier_t window_barrier; ///< synchronize control and simulation threads
	barrier_t top_barrier; ///< top_barrier synchronize only simulation threads
	barrier_t bottom_barrier;  ///< bottom_barrier synchronize only simulation threads
#endif /* SCHED_YIELD_BARRIER */

#ifdef FAST_SCHED_YIELD_BARRIER
	fast_barrier_t window_barrier; ///< synchronize control and simulation threads
	fast_barrier_t top_barrier; ///< top_barrier synchronize only simulation threads
	fast_barrier_t bottom_barrier;  ///< bottom_barrier synchronize only simulation threads
#endif /* FAST_SCHED_YIELD_BARRIER */

#ifdef TREE_BARRIER
	fast_tree_barrier_t window_barrier; ///< synchronize control and simulation threads
	fast_tree_barrier_t top_barrier; ///< top_barrier synchronize only simulation threads
	fast_tree_barrier_t bottom_barrier;  ///< bottom_barrier synchronize only simulation threads
#endif /* TREE_BARRIER */

	// time-scale information
	int   __log_ticks_per_sec;
	int  get_log_ticks_per_sec()         { return __log_ticks_per_sec; }
	void put_log_ticks_per_sec(int ltps) { __log_ticks_per_sec = ltps; }
	int  get_numTimelines()              { return __num_Timelines; }

	TimelineInterface( ltime_t t ) : __next_action(STOP_BEFORE_TIME),
			__stop_before(t) {};


	/** default stop_condition function is not to stop */
	virtual bool stop_condition()     { return false; }


	/** access to epoch state variable */
	ltime_t get_stop_before()               { return __stop_before;    }
	void   put_stop_before(ltime_t stop_before) { __stop_before = stop_before; }

	/** access to type of synchronization */
	next_action get_next_action()        { return __next_action; }
	stop_cond   get_stop_cond()          { return __stop_cond; }
	void put_next_action(next_action na) { __next_action = na;   }
	void put_stop_cond(stop_cond sc) { __stop_cond = sc;   }
};

/**
 *  A C++ program can build an S3F simulation and interact with it
 *   through an instance of some derivation of the Interface class
 *   (which is virtual).  Interface methods are called to build the model,
 *   initialize it, and then direct it to simulate epochs of simulation time.
 *   Between epochs the simulation threads are blocked, and the "other side"
 *   of the system can do whatever, e.g., update wireless field values
 */
class Interface
{
public:
	/* ****************************************************
		PUBLIC INTERFACE FOR CLASS INTERFACE
  ltime_t now
  Time to which the simulation has completely advanced 
	 **************************************************/

	/**
	 *  constructor.
	 *  @param num_timelines Indicate the number of simulation threads to create (pthreads)
	 *  @param ltps Give the time scale of the clock ticks in terms of log base 10 clock ticks per second.
	 */
	Interface(int,int);

	Interface();

	/**  destructor. No thought has gone into it. */
	virtual ~Interface();

	/** There is no base class instantiation for BuildModel.
	 * One must be crafted for the application or application domain.
	 * We imagine that the instantiated class will want access to the
	 * command line arguments, and so give a vector of strings as a way
	 * of passing these (or anything else).
	 */
	virtual void BuildModel( vector<string> );

	/**
	 * It is assumed that the call to BuildModel will create the objects needed in the simulation.
	 * After these have been constructed the InitModel method is called.
	 * This visits every Timeline and calls the init() method for every Entity on every Timeline.
	 * These init()s can finish establishing connections between objects.
	 */
	void InitModel();

	/**
	 *   Method advance is called to all the simulation to advance to the simulation
	 *   time passed as an argument, using the method indicated by next_action.
	 *   The return value describes how far in simulation time the simulation has advanced
	 *   after processing the advance directive.
	 */
	ltime_t advance(ltime_t );
	ltime_t advance(next_action, ltime_t );
	ltime_t advance(stop_cond, ltime_t );

	/**
	 *  Direct the simulation to advance another t units of simulation time.
	 *  When more than one simulation thread is involved,
	 *  Cross-timeline constraints may inhibit this however,
	 *  so the return value is how the simulation actually advanced
	 *  nxt_act = STOP_BEFORE_TIME allows for time to advance as far
	 *  as t when there is one simulation thread,
	 *  nxt_act = SYNCH_WINDOW explicitly advances only as far
	 *  as the window allows.  nxt_act == GLOBAL advances
	 */
	ltime_t advance_body(next_action, ltime_t, stop_cond);

	/** Retrieve the simulation clock timescale provided */
	inline int get_log_ticks_per_sec()       { return __log_ticks_per_sec; }

	/** Place the simulation clock timescale provided */
	void put_log_ticks_per_sec(int ltps)     { __log_ticks_per_sec = ltps; }

	/** Fetch the address of the Timeline whose index is provided as an argument. */
	Timeline* get_Timeline(unsigned int tl);

	/** Return the number of Timelines created for this simulation */
	unsigned int get_numTimelines()     { return __num_timelines; }
	ltime_t clock()                     { return __clock; }
	/** Return measurement including simulation time, total time, total events, etc */
	void runtime_measurements();

	/* ****************************************************
         PROTECTED DATA ELEMENTS
	 *******************************************************/
protected:
	vector<TimelineThread>	__timeline_threads; ///<  keep a vector of every Timeline created
	unsigned int __num_timelines; ///<  Total number of Timelines created
	ltime_t __clock; ///< time to which the simulation has advanced
	int __log_ticks_per_sec; ///< Time-scale of clock tickc, log base 10 of number of clock ticks in a second
	TimelineInterface __tli; ///< Pointer to the data structure created to interface with simulation Timelines.
	unsigned long __start_build_time; ///< time when BuildModel is called;
	unsigned long __start_run_time;   ///< time when advance first called
	unsigned long __end_run_time;     ///< time when advance finishes
	unsigned long full_exc_time(); ///< return the entire execution time (simulation + initialization/model-building)
	unsigned long sim_exc_time(); ///< return the simulation time

	unsigned long __srt_utime;
	unsigned long __srt_stime;
	unsigned long __acc_utime;
	unsigned long __acc_stime;
};

#endif /*__INTERFACE_H__*/

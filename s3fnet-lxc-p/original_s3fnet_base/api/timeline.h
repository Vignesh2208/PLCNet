/**
 * \file timeline.h
 *
 * \brief S3F Timeline
 *
 * authors : David Nicol, Dong (Kevin) Jin
 */

#ifndef __TIMELINE_H__
#define __TIMELINE_H__

#ifndef __S3F_H__
#error "timeline.h must be included by s3f.h"
#endif

/* forward references */
class pq;                    ///< used for priority queue interactions
class TimelineInterface;     ///< holds directives from the S3F interface object
class mappedChannel;         ///< holds description of a mapto directive

/**
 * Data structure of the incoming appointment from other timeline, used for composite synchronization
 */
struct InAppointment_block {
	ltime_t appointment;
	bool waiting;
	pthread_mutex_t appt_mutex;
};

typedef struct InAppointment_block InAppointment;

/** Data structure of the outgoing appointment to other timeline, used for composite synchronization */
struct OutAppointment_block {
	ltime_t lookahead;
	pthread_mutex_t appt_mutex;
	vector<EventPtr> events;
};

typedef struct OutAppointment_block OutAppointment;

/** actions sometimes depend on whether the timeline is running some process,
 *  is being initialized, or during the midst of a run but the simulation is not
 *  active. TimelineState reflects these conditions.
 */
enum TimelineState { INITIALIZING, BLOCKED, RUNNING, WAITING };

/**
 * Timeline is a logical process in concept, and a pthread by implementation.
 * Multiple S3F entities can be assigned to a timeline and thus being processed by a hardware core.
 */
class Timeline {
public:
	friend void*  proxy_function(void *);

	/**
	 *   At creation a Timeline is given a pointer to a data structure
	 *   that is the interface between the Timeline and Interface objects.
	 */
	Timeline       (TimelineInterface*);
	virtual        ~Timeline();

	/**
	 *  A timeline can be queried for the simulation time, either the time
	 *  stamp of the last event executed, or (if the state is not RUNNING)
	 *  the largest time in the synchronization window last completed.
	 */
	ltime_t               now()   { return __time; }

	/** The synchronization horizon, the upper edge of the current synchronization window */
	ltime_t           horizon()   { return __stop_before; }

	/** Reports the state of the Timeline. */
	TimelineState   get_state()   { return __state; }

	/** Returns the id for this timeline, between 0 and the number of created timelines minus 1. */
	unsigned            s3fid()   { return __s3fid; }

	/**
	 * The simulation clock is a long integer.  When the Interface to the simulation is created,
	 * a scale factor describing the units is given in terms of the logarithm (base 10) of the
	 * number of integer ticks per second of virtual time. This methods reports that value.
	 */
	int get_log_ticks_per_sec()   { return __log_ticks_per_sec; }

	/* ****************************************************************
          INTERFACE FOR ACCESS BY FRIEND CLASSES AND
          TIMELINE METHODS
	 ******************************************************************/

	/**
	 * This is the code body of the Timeline thread.
	 * It synchronizes with other Timelines, it interacts with the event data structures,
	 * it calls the Process functions associated with waitFor timeouts and waitOn deliveries.
	 *
	 * A Timeline is a pthread, and thread_function is the code body declared for it.
	 * All of Timeline processing happens within this function or calls made from it.
	 *
	 * The interface specifies an epoch of simulation time to cover, by specifying the
	 * simulation time to and beyond no Timeline may advance. The timeline awaits that
	 * specification by engaging in a barrier synchronization with all other Timelines
	 * and the control thread on barrier variable "window_barrier".
	 *
	 * The call starts by synchronizing with all other Timelines and the control thread
	 * through the "window_barrier" barrier construct. It blocks there until all Timeline
	 * threads and the control thread have reached it. In particular, the control thread
	 * does not enter that barrier until it has put information about the first synchronization
	 * epoch in the the interface it provides.
	 *
	 * The epoch is divided into a number of synchronization windows, constructed
	 * in such a way that during a synchronization window all Timelines may execute
	 * events asynchronously.  After the upper edge of a synchronization window is
	 * computed, it is compared with the epoch stopping time. If the latter is smaller,
	 * the stopping point for that cycle becomes the epoch stopping time.
	 *
	 * Execution of a synchronization window may cause events to be created on
	 * one Timeline, to be executed on a different one.  Such events are buffered
	 * in a list associated with the target Timeline. Therefore, each synchronization
	 * window begins calling method get_cross_timeline_events() to transfer those events
	 * into the Timeline's event list.  After this is done the Timeline's event list has
	 * all the events it will do in the coming window, and the next step is to compute the
	 * upper edge of the synchronization window.
	 *
	 * Each Timeline offers to a global min-reduction the sum of
	 * <BR> (i)  The earliest time of any event in its event list, and
	 * <BR> (ii) The smallest minimum delay associated with any connection that crosses timelines and has the outchannel on this Timeline.
	 *
	 * The minimum computed defines a time before which it is impossible for
	 * an event created on one timeline to have to be executed on another.
	 * That minimum is the upper edge of the synchronization window.
	 *
	 * Once the window is defined, method sync_window() is called to do all the
	 * processing associated with the window.
	 *
	 * Different barrier variables are used to control the synchronization window,
	 * ones that only the Timelines use (as the control thread is waiting on the
	 * window_barrier instance for the epoch to finish.
	 *
	 * On return from sync_window() it may be necessary to implement mapto calls that
	 * could not be executed dynamically, and changes of minimum delays on connections
	 * that likewise could not be executed dynamically.  As a result of these it may
	 * be necessary to recompute this Timeline's smallest cross timeline minimum delay.
	 * Following this, the Timeline repeats the cycle for another synchronization window,
	 * or enters the window barrier again which synchronizes all Timeline threads and the
	 * control thread, and releases the control thread when the epoch is completed.
	 */
	void*     thread_function();
protected:
	friend class Entity;
	friend class Interface;
	friend class OutChannel;
	friend class InChannel;

	/**
	 *  Schedule the given process activation, at the given time,
	 *  using the provided Activation record.  Return a handle
	 *  for cancelation.  Necessarily the process belongs
	 *  to an entity aligned to this timeline, but this has not
	 *  yet been checked.  Throw an assertion if violated
	 *
	 *  A timeline can directly schedule the execution of a given Process
	 *  (if the Process is owned by an Entity aligned to this Timeline).
	 *  An "Activation" is given as an argument to any Process whose execution results.
	 *  An Activation is a std::tr1::shared_ptr protecting a pointer to a message derived
	 *  from base class Message.  Its use alleviates issues of copying and deleting messages.
	 *  The unsigned int is a priority used in tie-breaking activations scheduled at the same time.
	 *  It return a "HandleCode" that can be used to cancel the action scheduled.
	 *  @param proc the process whose activation is scheduled
     *  @param t the time of the activation
     *  @param pri a priority used as a tie-breaker when events have the same base time
     *  @param act a smart pointer wrapper around an instance of a class derived from class Message, the payload.
	 */
	HandleCode    schedule(Process*,   ltime_t, unsigned int, Activation);

	/**
	 * 	Schedule the given arrival, at the given time, delivering the provided Activation record.
	 *  Return a handle for cancelation.
	 *
	 *  A timeline can directly schedule the arrival of a message at an InChannel owned
	 *  by an Entity aligned to this Timeline at a given time.
	 *  An "Activation" is given as an argument to any Process whose execution results.
	 *  An Activation is a std::tr1::shared_ptr protecting a pointer to a message derived
	 *  from base class Message.  Its use alleviates issues of copying and deleting messages.
	 *  The unsigned int is a priority used in tie-breaking activations scheduled at the same time.
	 *  It return a "HandleCode" that can be used to cancel the action scheduled.
	 *
	 *  @param ic the InChannel where the Activation will be delivered
	 *  @param t is the time of the delivery
	 *  @param pri is a priority used as a tie-breaker when events have the same base time
	 *  @param act is a smart pointer wrapper around an instance of a class derived from class Message, the payload.
	 *
	 *  If the InChannel is owned by an Entity not aligned with this timeline,
	 *  and the target time is at least as large as the horizon,
	 *  put the Event in a list for the target timeline.  Otherwise
	 *  place in a vector of Events associated with the OutAppointment
	 */
	HandleCode    schedule(InChannel*, ltime_t, unsigned int, Activation);

	/**
	 *  In order to avoid an infinite loop where an process that was activated
	 *  by an arrival on an inchannel calls waitOn for that inchannel AGAIN
	 *  and just gets picked again to execute, we _schedule_ attachments
	 *  to the inchannel's waiting list to occur after all the user events
	 *  at the current execution time have taken place. This version of schedule
	 *  implements that.
	 */
	void      schedule(InChannel*, Process*, bool, unsigned int);

	/**
	 *   Timeline variable __this points to the Process currently executing
	 *   (NULL if the Timeline state is not RUNNING).
	 *   This method returns that pointer.
	 */
	Process*  get_running_proc()                      { return __this; }

	/** Returns the time of the least-time event in the Timeline's event list. */
	ltime_t   next_time();

	/**
	 * As mapto calls are made, each one that crosses timeline boundaries
	 * has its minimum delay value reported to the timeline, using this method.
	 *
	 * During initialization, mapto calls that cross timeline boundaries will
	 * call this method so that at the end of all the init() calls we know what
	 * __min_sync_cross_timeline_delay is.
	 * Recall that __min_sync_cross_timeline_delay is initialized to -1.
	 */
	void         cross_timeline_delay(ltime_t d);

	/**
	 * This computation finds among all OutChannels owned by Entities
	 * aligned to this Timeline, the smallest minimum delay no smaller than
	 * thrs among mappings to InChannels on other timelines. The result of this
	 * computation is put into Timeline variable __min_sync_cross_timeline_delay.
	 *
	 * The thresholding is used to support the eventual inclusion of "Composite
	 * Synchronization" described in
	 *
	 * Nicol, David M., and Jason Liu. "Composite synchronization in parallel discrete-event simulation."
	 * Parallel and Distributed Systems, IEEE Transactions on 13.5 (2002): 433-446.
	 *
	 * Using thrs <=0 ensures that the overall minimum is found.
	 */
	ltime_t      min_sync_cross_timeline_delay(ltime_t thrs);

	/**
	 * Count the number of connections whose minimum delay values are no smaller
	 * than the argument.  Result is stored in variable __channels_at_minimum.
	 */
	unsigned int count_min_delay_crossings(ltime_t);

	/** Returns the value of __min_sync_cross_timeline_delay */
	ltime_t      get_min_sync_cross_timeline_delay()       { return __min_sync_cross_timeline_delay; }
	void         initialize_appt(bool, ltime_t);
	unsigned long get_nxt_handle();

	/* *****************************************************************
         METHODS USED INTERNALLY
   bool logical_advance()
    supports stopping when a logical condition is meet. A boolean indicates
    whether a pointer to a function should be called. If not, true is
    return, else the complement of the function return
	 ******************************************************************/

	/**
	 * Writes that cross timelines are buffered until the beginning of the
	 * next synchronization window; when buffered, a description of the write
	 * is placed in a list associated with the Timeline where the InChannel
	 * endpoint resides.
	 *
	 * In the previous synchronization window, other Timelines may have done
	 * outchannel writes that through maptos should deliver activations to
	 * inchannels on this Timeline. These were buffered in the __window_list vector.
	 * Reading from __window_list should be isolated by barrier synchronizations
	 * from any writes into it, so unlike the writes (which had to have mutex protection),
	 * the reads need none.
	 */
	void  get_cross_timeline_events();

	/**
	 * Changes Timeline state variable __state to s.
	 * Just before a Process body is executed the Timeline calls change_state(RUNNING);
	 * Immediately afterward it calls change_state(BLOCKED).
	 */
	void  change_state(TimelineState s)     { __state = s; }

	InChannel* get_activeChannel()          { return __activeChannel; }
	void  put_activeChannel(InChannel* inc) { __activeChannel = inc; }
	unsigned long get_executed()            { return __executed;     }
	unsigned long get_work_executed()       { return __work_executed; }
	unsigned long get_sync_executed()       { return __sync_executed; }

	/**
	 * The heart of the thread_body is code that establishes a synchronization
	 * window, and then all Timelines execute asynchronously up to the time
	 * established as an upper bound.  This method holds the code that executes
	 * all events within a synchronization window.
	 */
	void  sync_window();

	/**
	 * Called after a window synchronization to see if the conditions for continuing
	 * execution are meet. Checks to see if the timeline interface specification
	 * for advancing is to call an interface function or not to determine whether
	 * to advance.  The decision to check or not is cached in private bool variable
	 * __no_global_check (set to true if the configuration is NOT to call such a
	 * function.  Otherwise, TimelineInterface function stop_condition() is called.
	 */
	bool global_continue();

	/* *******************************************************************
  					INTERNAL VARIABLES
  pthread_mutex_t __appointment_mutex
  the mutex used in conjunction with __appointment_cond_var
	 */

private:
	TimelineState __state; ///< The current state of the Timeline, (one of INITIALIZING, RUNNING, BLOCKED).
	unsigned int  __s3fid; ///<   This Timeline's own id.

	/**
	 *   Just before a Process body is executed, __this is set to point to the instance
	 *   of the Process.  This is used as a default Process for calls to waitFor that do not
	 *   directly specify the Process body to execute at the expiration of the waitFor timer.
	 */
	Process*      __this;

	/**
	 *   When the Timeline is created an run, an s3f::Interface object has filled
	 *   out information describing how it wishes the simulation to advance during
	 *   the next epoch of time.  __interface_control holds a pointer to that (shared)
	 *   structure.
	 */
	TimelineInterface* __interface_control;

	/** This list holds a pointer for every Entity aligned to this Timeline. */
	vector<Entity*> __entity_list;

	/** Computed by method min_crosstime_delay() */
	ltime_t          __min_sync_cross_timeline_delay;

	ltime_t          __window_size;

	/**
	 *   After __min_sync_cross_timeline_delay is computed, we count the number of
	 *   connections whose minimum delay values are exactly this.  Such a count
	 *   helps to minimize calls to the expensive routine that recalculates
	 *   __min_sync_cross_timeline_delay after some mapping change.  If a connection that
	 *   has this minimum is removed or the minimum delay increases, so long as
	 *   __channels_at_minimum is still non-zero there is no need to recompute.
	 */
	unsigned int     __channels_at_minimum;

	/**
	 * Set to false at the beginning of a synchronization window, __recompute_min_delay
	 * is set to true if a dynamically called mapto or an unmap call remove the last
	 * known connection with minimum delay value __min_sync_cross_timeline_delay.  The
	 * flag is checked at the end of the window and a re-computation triggered if needed.
	 */
	bool		   __recompute_min_delay;

	/**
	 *   Scale factor for virtual time clock, gives the logarithm base 10 of the
	 *   number of clock ticks (increment of 1 on the clock variable) in each virtual time second.
	 *   Used when converting double precision floating point variables from or to native clock ticks.
	 */
	int              __log_ticks_per_sec;

	/**
	 * The virtual time up to which the timeline has advanced. __time is set to the time of
	 * a Process body activation when that is made, and then skips up to the end of the synchronization
	 * window after all events within the window have been executed.
	 */
	ltime_t          __time;

	/**
	 *   This holds the result of a window calculation by all simulation threads, establishing
	 *   an upper bound on how far they may simulate this window. Any activation with a timestamp
	 *   strictly less than __stop_before may be executed.
	 */
	ltime_t          __stop_before;

	/** This integer keeps track of the number of synchronization windows that have been completed. */
	unsigned int     __window;

	InChannel*       __activeChannel;

	/**
	 * Vector holding description of cross timeline writes.
	 * These have been formatted already into the same data
	 * structure as is used for the priority list.
	 */
	vector<EventPtr>  __window_list;

	/** Access to __window_list is guarded by mutex __events_mutex */
	pthread_mutex_t  __events_mutex;
	pthread_mutex_t  timeline_inst_mutex;

	/**
	 * When a timeline encounters an incoming appointment time from a timeline,
	 * whose time has not yet reached the appointment, it waits on this condition variable.
	 * The timeline it waits for must signal the variable.
	 */
	pthread_cond_t   __appointment_cond_var;

	/**
	 * When a timeline reaches an incoming appointment and finds that the timeline
	 * it awaits has not yet reached the synchronization point, it puts that Timeline's
	 * address in this variable and does a wait on the condition variable
	 */
	Timeline *       __waiting_on_timeline;

	/**
	 *  When mapto is called during execution of the simulation it may be necessary
	 *  to delay its implementation until the end of the current simulation window.
	 *  When that happens, a description of the mapping is created and a pointer to
	 *  it is put in the __channels_to_map vector.  At the end of a synchronization
	 *  window the Timeline checks whether this list is empty, and if not implements
	 *  the new maptos. If in addition the variable __recompute_min_delay is true,
	 *  then the new local minimum is computed before entering the next synchronization
	 *  window (at which point that value is needed).
	 */
	vector<mappedChannel*> __channels_to_map;

	/**
	 * The s3f interface to OutChannels allows for dynamic adjustment to the minimum delay
	 * on some specified connection.  Delays that cannot immediately be dealt with are buffered
	 * in vector __delays_to_change.  As with __channels_to_map, at the end of the synchronization
	 * window the size of this list is checked. If not empty the new minimum delays are imposed,
	 * and any need for recomputation of __min_sync_cross_timeline_delay is noted.
	 */
	vector<mappedChannel*> __delays_to_change;
	vector<InAppointment>  __in_appt;
	vector<OutAppointment> __out_appt;

	/** The event list.  An STL priority list structure is used. */
	STL_EventList    __events;

	unsigned long    __handle_idx;
	unsigned long    __executed;
	unsigned long    __work_executed;
	unsigned long    __sync_executed;
	unsigned long    __executed_one_window;
	unsigned long    __work_executed_one_window;
	unsigned long    __sync_executed_one_window;

	unsigned int     __evtnum;

	bool __no_global_check;

	/* ********************************************************
          STATIC VARIABLES
	 ************************************************************/
public:
	/**
	 * Synchronizes access to __entity_list.  Needed when the implementation
	 * advances to dynamic alignment of Entities to Timelines.
	 */
	static pthread_mutex_t  timeline_class_mutex;

	/**
	 * Enumerates the number of Timeline instances created.
	 * Value at instance of a Timeline's creation becomes that Timeline's id.
	 */
	static unsigned int     __num_timelines;

	/**
	 * Calls to schedule that could provide a priority for same-time
	 * tie-breking but do not are given this default_sched_pri.
	 * The range of values  permitted a user is 0- (2^{16}-1).  default_sched_pri
	 * is set to this largest value (like time, smaller means higher priority).
	 */
	static int              default_sched_pri;

	/** minimum possible scheduling priority, used only by a timeline, not a user priority */
	static int              min_sched_pri;
	static vector<Timeline*> timeline_adrs;
#ifdef PTHREAD_BARRIER
	static pthread_mutex_t bottom_barrier_min_value_mutex;
	static ltime_t bottom_barrier_min_value;
	static ltime_t prev_bottom_barrier_min_value;
#endif
};

/* macro for calling process bodies */
#define CALL_MEMBER_FN(object,ptrToMember)  ((object).*(ptrToMember))
#define MIN(a,b) (a<b?a:b)
#define MAX(a,b) (a>b?a:b)

#endif /*__TIMELINE_H__*/

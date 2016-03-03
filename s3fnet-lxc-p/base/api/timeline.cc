#include <s3f.h>
using namespace std;
using namespace s3f;

//#define DEBUG
/**
 * \file timeline.cc
 *
 * \brief S3F Timeline methods
 *
 * authors : David Nicol, Dong (Kevin) Jin
 */

/* ******************************************

  Timeline::Timeline (TimelineInterface* tli) 

 ********************************************/
Timeline::Timeline (TimelineInterface* tli) :
			 __interface_control(tli), __evtnum(0)  {

	// mutex for Timeline state must be initialized
	pthread_mutex_init(&timeline_inst_mutex, NULL);

	// mutex guarding __window_list must be initialized
	pthread_mutex_init(&__events_mutex,NULL); 

	pthread_mutex_init(&timekeeperTimelineTimeMutex,NULL);

	pthread_cond_init(&__appointment_cond_var, NULL);

	// always start at time 0.  Change this?
	pthread_mutex_lock(&timekeeperTimelineTimeMutex);
	__time = 0;
	pthread_mutex_unlock(&timekeeperTimelineTimeMutex);

	__window = 0;

	// interface tells us what the timescale is, remember it                                  
	__log_ticks_per_sec = tli->get_log_ticks_per_sec();

	// -1 means no meaningful value
	__min_sync_cross_timeline_delay = -1;              
	__channels_at_minimum = 0;	

	// we always start in the initialization state, no process running
	__state = INITIALIZING;                         
	__this  = NULL;
	__activeChannel = NULL;
	__executed = __work_executed = __sync_executed = 0;
	__executed_one_window = __work_executed_one_window = __sync_executed_one_window = 0;

	// the default scheduling priority from the user's perspective
	// is the largest unsigned integer you can represent in 16 bits
	default_sched_pri = (1<<16)-1;

	// minimum scheduling priority from the system's perspective
	min_sched_pri = INT_MAX;	

	__handle_idx = 0;

	// get a unique id for this timeline.  Just in case there
	// are multiple threads building timelines, protect the access
	// with the static Timeline class mutex
	//
	pthread_mutex_lock(&timeline_class_mutex);
	__s3fid = __num_timelines++;
	timeline_adrs.push_back(this);
	pthread_mutex_unlock(&timeline_class_mutex);

	// initialize appointment vectors
	int num_timelines = (int)tli->get_numTimelines();
	__in_appt.resize(num_timelines);
	__out_appt.resize(num_timelines);	

	// initialize flag indicating whether to call a timeline interface
	// function to see if a global termination condition has been met
	if( tli->get_next_action() == STOP_FUNCTION ) __no_global_check = false;
	else __no_global_check = true;
}



/* ************************************************

  HandleCode Timeline::schedule(Process *proc, ltime_t t, 
  	unsigned int pri, Activation act)
        -- Process* proc is the process whose activation is scheduled
        -- ltime_t t is the time of the activation
        -- unsigned int pri is a priority used as a tie-breaker
               when events have the same base time 
        -- Activation act is a smart pointer wrapper around an
            instance of a class derived from class Message, the
            payload.

  schedule the given process activation, at the given time,
     using the provided Activation record.  Return a handle
     for cancelation.  Necessarily the process belongs
     to an entity aligned to this timeline, but this has not 
     yet been checked.  Throw an assertion if violated

 *************************************************/
HandleCode Timeline::schedule(Process *proc, ltime_t t,
		unsigned int pri, Activation act) {

	// can only directly schedule a Process if its owner
	//  is on this timeline
	assert( proc->owner()->alignment() == this );

	// create a timeout event to be scheduled on this timeline,
	Event* e = new Event(t, pri, proc, act, this, __evtnum++);	
	HandleCode h = (void *)e;
	EventPtr eptr(e);

	// push onto the priority queue
	__events.push( eptr );

	// return the handle.  The fact that it is a smart pointer
	// to an event is not part of the API, but is a useful
	//  implementation detail
	//
	return h;
}

/************************************************************

  HandleCode Timeline::schedule(InChannel *ic, ltime_t t, 
  		unsigned int pri, Activation act)

        -- InChannel* ic is the InChannel where the Activation
             will be delivered
        -- ltime_t t is the time of the delivery
        -- unsigned int pri is a priority used as a tie-breaker
               when events have the same base time 
        -- Activation act is a smart pointer wrapper around an
            instance of a class derived from class Message, the
            payload.

  schedule the given arrival, at the given time,
     delivering the provided Activation record.  Return a handle
     for cancelation.  

  If the InChannel is owned by an Entity not aligned with this timeline,
    and the target time is at least as large as the horizon,   
    put the Event in a list for the target timeline.  Otherwise
    place in a vector of Events associated with the OutAppointment

 ***************************************************************/
HandleCode Timeline::schedule(InChannel *ic, ltime_t t,
		unsigned int pri, Activation act) {


	// if the activation is crossing timelines, do a reset to avoid
	// thread unsafety problems
	Timeline* tgt = ic->owner()->alignment();
	Event* e =new Event(EVTYPE_ACTIVATE,t, 0, ic, act, NULL, tgt, __evtnum++);

	// return a smart pointer to the event to be scheduled. That's the 
	// handle for a cancelation.

	HandleCode h = (void *)e;
	EventPtr eptr(e);

	// see if the target InChannel is on this timeline or not
	if( tgt != this && horizon() <= t) {
		// going to a different timeline in a future window,
		// so save it to be exchanged
		//  after the window synchronization has taken place
		// Access to this list must be serialized

		pthread_mutex_lock( &(tgt->__events_mutex));

		// Put the event in the buffer
		tgt->__window_list.push_back( eptr );

		// release the mutex
		pthread_mutex_unlock( &(tgt->__events_mutex) );

		// we're done
		return h;
	} 

	if( tgt != this && t < horizon()) {
		// exchange this at the appointment structure. Get the target timeline
		// index
		{ int tgt_tl = ic->alignment()->s3fid();

		pthread_mutex_lock( &__out_appt[tgt_tl].appt_mutex );
#ifdef DEBUG
		printf("at %ld timeline %d pushes event (time %ld) on __out_appt[%d]\n",
				now(), s3fid(), t, tgt_tl);
#endif
		__out_appt[tgt_tl].events.push_back( eptr );

		pthread_mutex_unlock( &__out_appt[tgt_tl].appt_mutex );
		return h;
		}
	}

	// the InChannel is owned by an entity aligned to this
	// timeline, so the event just goes onto the event list
	//
	__events.push ( eptr );

	// we're done
	return h;
}

void  Timeline::schedule(InChannel* ic, Process* proc, bool bind, unsigned int pri ) {

	Event* e; 
	// create an event to happen immediately to implement the attachment 
	e=new Event(now(), pri, ic,proc,bind,pri,this, __evtnum++);
	EventPtr eptr(e);
	__events.push (eptr);
}

/* **************************************************************

    ltime_t Timeline::next_time()

    Return -1 if the __events list is empty, otherwise return
    the timestamp of the event with least timestamp.

 *****************************************************************/

ltime_t Timeline::next_time() {

	// return -1 if there are no events
	if( __events.empty() ) return -1;
	return (__events.top())->get_time();
}

/* ***************************************************************

 void* Timeline::thread_function()

 A Timeline is a pthread, and thread_function is the code body
 declared for it.  All of Timeline processing happens within
 this function or calls made from it.

 The interface specifies an epoch of simulation time to cover, by
 specifying the simulation time to and beyond no Timeline may advance.
 The timeline awaits that specification by engaging in a barrier synchronization
 with all other Timelines and the control thread on barrier variable
 "window_barrier".  

 The call starts by synchronizing with all other Timelines and the
 control thread through the "window_barrier" barrier construct.  It
 blocks there until all Timeline threads and the control thread have 
 reached it.  In particular, the control thread does not enter that barrier 
 until it has put information about the first synchronization epoch
 in the the interface it provides.

 The epoch is divided into a number of synchronization windows, constructed
 in such a way that during a synchronization window all Timelines may execute
 events asynchronously.  After the upper edge of a synchronization window is
 computed, it is compared with the epoch stopping time. If the latter is smaller,
 the stopping point for that cycle becomes the epoch stopping time.

 Execution of a synchronization window may cause events to be created on
 one Timeline, to be executed on a different one.  Such events are buffered
 in a list associated with the target Timeline.  Therefore,
 each synchronization window begins calling method get_cross_timeline_events() 
 to transfer those events into the Timeline's event list.  After this is done
 the Timeline's event list has all the events it will do in the coming 
 window, and the next step is to compute the upper edge of the synchronization window.

 Each Timeline offers to a global min-reduction the sum of
   (i)  The earliest time of any event in its event list, and
   (ii) The smallest minimum delay associated with any connection 
     that crosses timelines and has the outchannel on this Timeline.

 The minimum computed defines a time before which it is impossible for
 an event created on one timeline to have to be executed on another.
 That minimum is the upper edge of the synchronization window.

 Once the window is defined, method sync_window() is called to do all the
 processing associated with the window.

 Different barrier variables are used to control the synchronization window,
 ones that only the Timelines use (as the control thread is waiting on the
 window_barrier instance for the epoch to finish.

 On return from sync_window() it may be necessary to implement mapto calls that
 could not be executed dynamically, and changes of minimum delays on connections
 that likewise could not be executed dynamically.  As a result of these it may
 be necessary to recompute this Timeline's smallest cross timeline minimum delay.
 Following this, the Timeline repeats the cycle for another synchronization window,
 or enters the window barrier again which synchronizes all Timeline threads and the
 control thread, and releases the control thread when the epoch is completed.

 ******************************************************************/

void* Timeline::thread_function() {

	// variables used in the loop below
	ltime_t epoch_end;
	ltime_t nxt_time;

	while(1) {
		// wait for all Timelines to reach this, and the control thread
		// to put epoch information in the interface
		//
#ifdef MUTEX_BARRIER
		__interface_control->window_barrier.wait( -1 );
#endif
#ifdef SCHED_YIELD_BARRIER
		__interface_control->window_barrier.wait( s3fid(), -1 );
#endif
#ifdef PTHREAD_BARRIER
		pthread_barrier_wait( &(__interface_control->window_barrier) );
#endif
		//
		// Before entering the top_barrier to release
		// the simulation threads, the control thread filled
		// out the interface with parameters describing
		// the first window
		//
		epoch_end   = __interface_control->get_stop_before();

		printf("### Epoch %lu  ### Time %lu ### StopBefore %lu ### windowsize %lu \n", epoch_end, now(), __stop_before, __window_size);

		if( __interface_control->get_next_action() == STOP_BEFORE_TIME )
		{
			if(epoch_end <= now()+1){
				printf("!!!! Breaking out of timeline thread !!!!\n");
				exit(0);
				break;
			}
		}

		if( __interface_control->get_next_action() == STOP_FUNCTION )
			__no_global_check = false;
		else __no_global_check = true;


		// enter the loop of doing synchronization windows until the
		// full simulation epoch is covered.
		//
		do {
			get_cross_timeline_events();

			// compute upper edge of synchronization window---equal to
			// min_{all timelines}{ time_of_first_event + minimum_cross_timeline_delay }
			//
			change_state(BLOCKED);
			nxt_time = next_time();

			if( __interface_control->get_numTimelines() > 1 ) {
				// if either there is no event on the event list, or no outbound connections
				// that span Timelines, offer __window_size to the barrier
				if( nxt_time < 0 || __min_sync_cross_timeline_delay < 0  )  {
					ltime_t this_window = MAX(now(), nxt_time) + __window_size;
					if(this_window > now())
					{
#ifdef MUTEX_BARRIER
						__interface_control->bottom_barrier.wait( this_window );
#endif
#ifdef SCHED_YIELD_BARRIER
						__interface_control->bottom_barrier.wait( s3fid(), this_window );
#endif
#ifdef PTHREAD_BARRIER
						int bottom_barrier_status = 0;
						pthread_mutex_lock(&bottom_barrier_min_value_mutex);
						if(bottom_barrier_min_value == prev_bottom_barrier_min_value || bottom_barrier_min_value > this_window) bottom_barrier_min_value = this_window;
						pthread_mutex_unlock(&bottom_barrier_min_value_mutex);
						bottom_barrier_status = pthread_barrier_wait( &(__interface_control->bottom_barrier) );
						if(bottom_barrier_status == -1) prev_bottom_barrier_min_value = bottom_barrier_min_value;
#endif
					}
					else //mean nothing to execute, next_time should be infinitedly large
					{
#ifdef MUTEX_BARRIER
						__interface_control->bottom_barrier.wait(-1);
#endif
#ifdef SCHED_YIELD_BARRIER
						__interface_control->bottom_barrier.wait(s3fid(), -1);
#endif
#ifdef PTHREAD_BARRIER
						pthread_barrier_wait( &(__interface_control->bottom_barrier) );
#endif
					}

				} else {
					// either nxt_time or __min_sync_cross_timeline_delay may be -1,
					// so ensure in both cases that the event time offered is at
					// least now(), and that the minimum cross timeline delay is at
					// least 0.  Compute the sum and offer to the min-reduction.
					//
#ifdef MUTEX_BARRIER
					__interface_control->bottom_barrier.wait( MAX(now(),nxt_time) + MAX(0,__min_sync_cross_timeline_delay));
#endif
#ifdef SCHED_YIELD_BARRIER
					__interface_control->bottom_barrier.wait( s3fid(), MAX(now(),nxt_time) + MAX(0,__min_sync_cross_timeline_delay));
#endif
#ifdef PTHREAD_BARRIER
					int bottom_barrier_status = 0;
					ltime_t offered_min_value = MAX(now(),nxt_time) + MAX(0,__min_sync_cross_timeline_delay);
					pthread_mutex_lock(&bottom_barrier_min_value_mutex);
					if(bottom_barrier_min_value == prev_bottom_barrier_min_value || bottom_barrier_min_value > offered_min_value) bottom_barrier_min_value = offered_min_value;
					pthread_mutex_unlock(&bottom_barrier_min_value_mutex);
					bottom_barrier_status = pthread_barrier_wait( &(__interface_control->bottom_barrier) );
					if(bottom_barrier_status == -1) prev_bottom_barrier_min_value = bottom_barrier_min_value;
#endif
				}

				// get the upper window edge computed by the min-reduction.
				// __stop_before holds that value.
				//
#ifdef PTHREAD_BARRIER
				__stop_before = bottom_barrier_min_value;
#else
				__stop_before = __interface_control->bottom_barrier.get_min_value();
#endif
				// we cannot go farther than the upper edge of the epoch window though.
				if(__stop_before <= 0) //nothing in the eventlist, jump directly to the epoch_end;
				{
					__stop_before = epoch_end;
					printf("Should NEVER EVER GET HERE\n");
					assert(false);
				}
				else
					__stop_before = min(__stop_before, epoch_end);

			} else __stop_before = epoch_end;

			// do the simulation work for the window.
			//
			change_state(RUNNING);
#ifdef DEBUG
			printf("timeline %d enters window [%ld,%ld)\n",
					s3fid(), now(), __stop_before);
#endif

			sync_window();
			change_state(BLOCKED);

			// synchronize at a "end of cycle" barrier, but the one
			// to join depends on whether we've finished an epoch.
			// Can tell if __stop_before < stop_before (means not done)
			//   While this is a min-reduction, we aren't using that capability
			// here, so just offer -1 (nothing)
			//

			if( __stop_before < epoch_end)
			{
#ifdef MUTEX_BARRIER
				__interface_control->top_barrier.wait(-1);
#endif
#ifdef SCHED_YIELD_BARRIER
				__interface_control->top_barrier.wait( s3fid(),  global_continue() );
#endif
#ifdef PTHREAD_BARRIER
				// PTHREAD_BARRIER_SERIAL_THREAD is -1
				pthread_barrier_wait( &(__interface_control->top_barrier) );
#endif
			}
			else
			{
#ifdef MUTEX_BARRIER
				__interface_control->window_barrier.wait(__stop_before);
#endif
#ifdef SCHED_YIELD_BARRIER
				__interface_control->window_barrier.wait(s3fid(), __stop_before);
#endif
#ifdef PTHREAD_BARRIER
				pthread_barrier_wait( &(__interface_control->window_barrier) );
#endif
			}
			// do another synchronization window if the last one was not the epoch end
			//
		} while( __stop_before < epoch_end );
	}
	return (void *)NULL;
}

/* *******************************************************************

 void Timeline::sync_window()

 sync_window does the work of one synchronization window.			

 **********************************************************************/
void Timeline::sync_window() {

	// local variables, mostly to buffer individual elements
	// read more than one time from somewhat-complicated to access
	// data structures
	EventPtr   nxt_evt;
	Entity*  ent;
	OutChannel* oc;
	InChannel* ic;
	ltime_t delay;
	unsigned int tl;
	Timeline* dest_tl;
	InAppointment* in_appt;
	bool released;
	// flag set to true if at the end of the window we need to recompute
	// the minimum cross timeline delay
	//
	__recompute_min_delay = false;

	// knowing the window index helps with some double buffering schemes
	__window++;
	/*
	 * The timeline logic had to slightly be changed in order to process emulated events. Variable __time and
	 * certain functions of the event_list were equipped with locks in order to prevent any unpredictable behavior
	 * as the outgoing thread in the LXC Manager injects events into various timelines, that may or may not be
	 * executing. (This avoids certain cases of injecting an event into a timeline (via scheduling)
	 * with time __time when that variable
	 * is also at the same time modified by the timeline logic.
	 *
	 * As a result, before any event gets executed, all LXCs on that timeline must advance to that virtual time.
	 *
	 */
	while(1)
	{
		// lock
		pthread_mutex_lock(&__events.MUTEX);

		if (__events.empty_lockless())
		{
			pthread_mutex_unlock(&__events.MUTEX);
			break;
		}

		nxt_evt = __events.top_lockless();
		pthread_mutex_unlock(&__events.MUTEX);

		// check added to make sure that LXCs dont advance past a timelines virtual time
		// this could happen if the next event is the next epoch but the simulation does not
		// enter the next epoch.
		if (nxt_evt->get_time() <= __stop_before )
		{
			if (simCtrl->advanceLXCsOnTimeline(s3fid(), nxt_evt->get_time()))
			{
				continue;
			}
		}

		nxt_evt= __events.top();
		if (nxt_evt->get_time() < __stop_before )
		{
			__events.pop();
		}
		else
		{
			//pthread_mutex_unlock(&__events.MUTEX);
			break;
		}

	//}*/

	// event list loop.  Do all events with times strictly less than __stop_before
	//
	//while ( !__events.empty() &&
	//		(nxt_evt=__events.top())->get_time() < __stop_before ) {

		// advance simulation time this far
		// __time  = nxt_node->get_time();
		pthread_mutex_lock(&timekeeperTimelineTimeMutex);
		__time  = nxt_evt->get_time();
		pthread_mutex_unlock(&timekeeperTimelineTimeMutex);

		// get a pointer to the event
		// nxt_evt = nxt_node->get_evt();

		// process the event, depending on event type
		//

#ifdef DEBUG
		printf("At %ld on timeline %u event type %d executed\n",
				now(), s3fid(), nxt_evt->get_evtype() );
#endif

		__executed++;
		__executed_one_window++;
		switch (nxt_evt->get_evtype()) {

		// on a timeout we call the function whose pointer is carried
		// with the event, passing along the activation that is also carried
		// with the event
		//
		case EVTYPE_TIMEOUT :
			__work_executed++;
			__work_executed_one_window++;
			// set __this for reference to process that is running
			__this = nxt_evt->get_proc();

			// make process' __active NULL to flag that it was triggered by
			// a waitFor
			//
			__this->put_activeChannel( NULL );

			// Macro to call the Process body associated with the Process pointed to
			// by __this, passing to it the Activation carried along in the event
			//
			CALL_MEMBER_FN( *__this->owner(), __this->__func )(nxt_evt->get_act());
			__this = NULL;

			break;

			// EVTYPE_ACTIVATE  identifies all activations of Processes attached to
			// the named in-channel.  It schedules individual EVTYPE_EXEC_ACTIVATE
			// events to do the work.  Since the EVTYPE_ACTIVATE is scheduled at the highest
			// priority order, the priorities on these activations "fit" with the priorities
			// of any other scheduled events at the same time.
			//
			//
		case EVTYPE_ACTIVATE :
			// The InChannel has an STL list of Processes that should be
			// activated if an Activation appears on the InChannel.  
			// Iterate over that list, for each member call the associated
			// Process
			// 
			ic = nxt_evt->get_inc();

			// for each process create an EXEC_ACTIVATION event and schedule it at
			// the priority given in the process priority list
			//
			for(std::list<ProcessPri>::iterator 
					list_iter = ic->__waitingList.begin();
					list_iter != ic->__waitingList.end(); list_iter++) {

				Event* e; 
				if( (*list_iter).get_pri() > 0 )
					e = new Event(EVTYPE_EXEC_ACTIVATE, now(), (*list_iter).get_pri(), ic,
							nxt_evt->get_act(), (*list_iter).get_proc(),this, __evtnum++);
				else
					e = new Event(EVTYPE_EXEC_ACTIVATE, now(), (*list_iter).get_proc()->pri(), ic,
							nxt_evt->get_act(), (*list_iter).get_proc(),this, __evtnum++);

				__events.push(e);

				// Depending on whether the Process was attached to the InChannel by bind()
				// which survives this call) or by waitOn (which does not), we may need
				// to remove the Process from the InChannel list.  The get_bound() method
				// returns true if the Process went on via a bind() call.  Remove before
				// the call in case of queries against the binding state during the call

				if( !(*list_iter).get_bound() )
					list_iter = nxt_evt->get_inc()->__waitingList.erase(list_iter);
			}
			break;

		case EVTYPE_EXEC_ACTIVATE:
			__work_executed++;
			__work_executed_one_window++;
			__this = nxt_evt->get_proc();

			// Need to know what Entity owns the Process
			ent = __this->owner();		

			// running process needs to know what inchannel
			// triggered it
			//
			__this->put_activeChannel( nxt_evt->get_inc() );
			put_activeChannel( nxt_evt->get_inc() );


			// Macro to call the Process body associated with the Process pointed to
			// by __this, passing to it the Activation carried along in the event
			//
			CALL_MEMBER_FN( *ent, __this->__func)(nxt_evt->get_act());

			put_activeChannel( NULL );
			__this->put_activeChannel( NULL );

			// not processing this body anymore
			__this = NULL;
			break;



		case EVTYPE_MAKE_APPT:
			__sync_executed++;
			__sync_executed_one_window++;
			// get the identity of the timeline needing this appointment
			tl = nxt_evt->get_tl();

			// turn that into a pointer to the timeline
			dest_tl = timeline_adrs[tl];

			// recover pointer to the InAppointment structure on the destination,
			// using own timeline identity index into destination's incoming appt array
			in_appt = &dest_tl->__in_appt[__s3fid];

			// gain access to the data in the appointment structure
			pthread_mutex_lock( &in_appt->appt_mutex );

			// write the new appointment into it
			in_appt->appointment = now() + __out_appt[tl].lookahead;

			// if the target timeline is waiting it needs a signal to its
			// condition variable
			if( in_appt->waiting ) {
#ifdef DEBUG
				printf("at %ld timeline %d signals timeline %d\n",
						now(), s3fid(), tl);
#endif
				pthread_cond_signal(&dest_tl->__appointment_cond_var);
				// &in_appt->appt_mutex);
			}
			// release the appointment data
			pthread_mutex_unlock( &in_appt->appt_mutex );

			// schedule the next appointment write at a scheduling priority
			// higher than the appointment wait
			{
				Event *e =new Event(in_appt->appointment, min_sched_pri-1, tl,EVTYPE_MAKE_APPT,this, __evtnum++);
				EventPtr eptr(e);
#ifdef DEBUG
				printf("at %ld timeline %d schedules next update to timeline %d for %ld\n",
						now(), s3fid(), tl, now() + __out_appt[tl].lookahead);
#endif
				// __events.push( pqn );
				__events.push( eptr );
			}
			break;

		case EVTYPE_WAIT_APPT:
			__sync_executed++;
			__sync_executed_one_window++;
			// get the identity of the timeline we're waiting for
			tl = nxt_evt->get_tl();

#ifdef DEBUG
			printf("at %ld timeline %d checks appointment from %d, sees %ld\n",
					now(), s3fid(), tl, __in_appt[tl].appointment);
#endif

			// continue if the appointment time is larger than the clock
			released = false;
			if( now() < __in_appt[tl].appointment) released = true;

			// acquire the appointment lock and have another look
			if(! released ) {
				pthread_mutex_lock( &__in_appt[tl].appt_mutex );
				if( now() < __in_appt[tl].appointment) {
					// got lucky
					pthread_mutex_unlock( &__in_appt[tl].appt_mutex );
				} else {
					// flag waiting state, mark state of timeline as waiting,
					// and wait for a signal from the timeline it waits for
					__in_appt[tl].waiting = true;

					change_state(WAITING);
#ifdef DEBUG
					printf("at %ld timeline %d to wait on timeline %d\n",
							now(), s3fid(), tl);
#endif
					pthread_cond_wait(&__appointment_cond_var, &__in_appt[tl].appt_mutex);

					// the appointment time has been updated. Adjust state
					// and schedule another EVTYPE_WAIT_APPT at the given time
					//
					change_state(RUNNING);
					__in_appt[tl].waiting = false;

#ifdef DEBUG
					printf("at %ld timeline %d released by timeline %d\n",
							now(), s3fid(), tl);
#endif

					// free up mutex
					pthread_mutex_unlock( &__in_appt[tl].appt_mutex );
				}}
			// schedule the next incoming appointment wait time
			{
				Event* e =new Event(__in_appt[tl].appointment, min_sched_pri, tl,EVTYPE_WAIT_APPT,this, __evtnum++);

				EventPtr eptr(e);
#ifdef DEBUG
				printf("at %ld timeline %d schedules wait check from timeline %d at time %ld\n",
						now(), s3fid(), tl, __in_appt[tl].appointment);
#endif
				__events.push( eptr );
			}

			// move any events left here by the appointment source
			// into own event list
			{
				OutAppointment *out_appt = &(timeline_adrs[tl]->__out_appt[s3fid()]);
				pthread_mutex_lock( &out_appt->appt_mutex);
				for(int i = out_appt->events.size()-1; i>= 0; i--) {
					__events.push( out_appt->events[i] );
				}
				out_appt->events.clear();

				pthread_mutex_unlock( &out_appt->appt_mutex);
			}

			break;

		case EVTYPE_BIND:
			__work_executed++;
			__work_executed_one_window++;
			// safe now to implement delayed binding
			nxt_evt->get_inc()->insert_process_pri( nxt_evt->get_proc(),
					nxt_evt->get_bind(), user_pri(nxt_evt->get_pri()) );
			break;

			// simply ignore cancelled events
		case EVTYPE_CANCEL :
			__executed--;
			__executed_one_window--;
			break;
		}

		nxt_evt->release();
		// delete nxt_node;
	}

	// All events in this window now processed. Check for any needed
	// dynamic updating of mappings or minimum delays
	// that were called for during the last burst
	//
	// any maptos needing doing?
	//
	if( __channels_to_map.size() > 0 ) {
		// yes. map the channels
		//
		for(unsigned int i=0; i<__channels_to_map.size(); i++) {
			// which outchannel is doing the mapto?
			//
			oc = __channels_to_map[i]->oc();

			// the __channels_to_map structure has the complete mapping
			// information, and so all that needs to be done is to put
			// it in the outchannel's list of mapped channels.
			//
			oc->__mapped_to.push_back( *__channels_to_map[i] );

			// if minimum cross timeline delay for this is equal
			// to the timeline's minimum, increment the count of
			// such.  If it is less than the existing minimum,
			// throw a flag that indicates a new minimum must be
			// computed.
			//
			delay = oc->min_write_delay() +
					__channels_to_map[i]->transfer_delay();

			if( delay == __min_sync_cross_timeline_delay )
				__channels_at_minimum++;
			else {
				if( delay < __min_sync_cross_timeline_delay )
					__recompute_min_delay = true;
			}
		}
		// empty the list, call the element's destructors
		__channels_to_map.clear();
	}

	// check whether there are cross timeline minimum delays that
	// with updates buffered
	//
	if( __delays_to_change.size() > 0 ) {

		// temporary variable used to compute the complete crossing time
		ltime_t crossing;

		// change the minimum delay of each channel in this list,
		// and determine whether that triggers a recalculation of the
		// minimum cross timeline channel delay
		//
		for(unsigned int i=0; i<__delays_to_change.size(); i++) {
			oc    = __delays_to_change[i]->oc();
			ic    = __delays_to_change[i]->ic();

			// Different logic for changing an OutChannel's minimum write delay
			if( __delays_to_change[i]->select() ) {
				ltime_t xtransfer = oc->min_xtransfer_delay();
				ltime_t mwd = __delays_to_change[i]->min_write_delay();

				// if xtransfer is not -1 there is a cross timeline mapping, which means
				// we have to check that there's a known positive lower bound on the
				// transfer time.  If not, at this point we can only ignore the change
				// request...
				//
				crossing = mwd+xtransfer;
				if( xtransfer >= -1 && crossing == 0 ) continue;

				// implement the change
				oc->__min_write_delay = mwd;

				// flag if the change impacts the minimum cross timeline delay
				if( crossing == __min_sync_cross_timeline_delay ) __channels_at_minimum++;
				if( crossing < __min_sync_cross_timeline_delay)  __recompute_min_delay = true;
			}
			// if we get here the delay is a tranfer delay, so find the corresponding InChannel
			delay = __delays_to_change[i]->transfer_delay();

			// search the OutChannel's table of mappings for this one
			unsigned int end = oc->__mapped_to.size();
			for(unsigned int i=0; i<end; i++) {
				if( oc->__mapped_to[i].ic() == ic ) {
					// found the named InChannel, so change the transfer delay
					(oc->__mapped_to[i]).set_transfer_delay(delay);

					// if this just lowers the delay to the existing minimum
					// we just have one more such connection
					//
					ltime_t crossing = oc->min_write_delay() +
							oc->__mapped_to[i].transfer_delay();
					if( crossing == __min_sync_cross_timeline_delay )
						__channels_at_minimum++;
					else if ( crossing < __min_sync_cross_timeline_delay)
						// It got smaller than the Timeline's smallest, so
						// we'll have to redo the minimum cross timeline delay
						// calculation
						//
						__recompute_min_delay = true;
					break;
				}
			}
		}
		__delays_to_change.clear();
	}	
	// haven't done anything with appointments yet 

	// if in the course of doing an unmap during processing or 
	// a mapto or min delay change above we changed the minimum
	// cross timeline delay, recompute it and then recompute
	// the number of times a cross-timeline channel with that minimum
	// delay appears	
	//
	if(__recompute_min_delay ) {

		// method min_sync_cross_timeline_delay goes through all entities
		__min_sync_cross_timeline_delay = min_sync_cross_timeline_delay(__window_size);

		// count the number of connections whose minimum delays are
		// exactly the Timeline's overall minimum
		//
		__channels_at_minimum = count_min_delay_crossings( __min_sync_cross_timeline_delay );
	}	

	// we're done with this synchronization window.  Time advances to the very end of it.
	pthread_mutex_lock(&timekeeperTimelineTimeMutex);
	__time = __stop_before-1;
	pthread_mutex_unlock(&timekeeperTimelineTimeMutex);

#ifdef SYNC_WIN_EVENT_COUNT
	printf("now = %ld, timeline = %d, window# = %d, total events = %ld, work events = %ld, sync events = %ld\n", 
			now(), s3fid(), __window, __executed_one_window, __work_executed_one_window, __sync_executed_one_window);
#endif
	__executed_one_window = __work_executed_one_window = __sync_executed_one_window = 0;

}

/* *****************************************************

  void Timeline::get_cross_timeline_events()

  In the previous synchronization window, other Timelines
  may have done outchannel writes that through maptos 
  should deliver activations to inchannels on this Timeline.
  These were buffered in the __window_list vector.
    Reading from __window_list should be isolated by
  barrier synchronizations from any writes into it, so unlike
  the writes (which had to have mutex protection), the reads
  need none.

 ********************************************************/

void Timeline::get_cross_timeline_events() {

	// local copy of the list size, since it will not change
	// each pass through the loop, an optimization
	//
	int num_to_do = __window_list.size();

	// For each buffered event, all that is needed is a copy
	// into __events
	//

	for(int i=0; i<num_to_do; i++) {
		__events.push( __window_list[i] );
	}

	// if the list was not empty, it can be emptied now
	if( num_to_do > 0 ) __window_list.clear();
}

/* *******************************************************

Timeline:: ~Timeline()

 ***********************************************************/

Timeline:: ~Timeline() {  
	while(!__events.empty()) {
		__events.pop();
	}
	// destroy mutexes and cond_var
	//
	if( !s3fid() ) pthread_mutex_destroy(&timeline_class_mutex);
#ifdef PTHREAD_BARRIER
	if( !s3fid() ) pthread_mutex_destroy(&bottom_barrier_min_value_mutex);
#endif
	pthread_mutex_destroy(&__events_mutex);
	pthread_mutex_destroy(&timeline_inst_mutex);
	pthread_cond_destroy(&__appointment_cond_var);

	for(unsigned int i=0; i<__in_appt.size(); i++) {
		pthread_mutex_destroy(&__in_appt[i].appt_mutex);
	}
	for(unsigned int i=0; i<__out_appt.size(); i++) {
		pthread_mutex_destroy(&__out_appt[i].appt_mutex);
	}
} 


/* *******************************************************

Timeline::cross_timeline_delay(ltime_t d)

During initialization, mapto calls that cross timeline
boundaries will call this method so that at the end
of all the init() calls we know what __min_sync_cross_timeline_delay
is.  Recall that __min_sync_cross_timeline_delay is initialized to -1.

 **********************************************************/

void Timeline::cross_timeline_delay(ltime_t delay) {

	// change __min_sync_cross_timeline_delay if either this is
	// the first call to the method, or the offered delay
	// is smaller than the smallest seen so far
	//
	if( (__min_sync_cross_timeline_delay == -1) ||
			(delay < __min_sync_cross_timeline_delay) ) {
		__min_sync_cross_timeline_delay = delay;
	}
}

/* ********************************************************

  ltime_t Timeline::min_sync_cross_timeline_delay(ltime_t thrs)

 among all entities mapped to this timeline, get the
 minimum cross-timeline delay of all outbound
 cross timeline connections that are at least as large as thrs.

 ***********************************************************/

ltime_t Timeline::min_sync_cross_timeline_delay(ltime_t thrs) {

	// initialize answer variable to "nothing here"
	ltime_t ans = -1;

	// buffer entity delay once computed
	ltime_t ent_delay = 0;

	// number of entities to visit
	unsigned int end = __entity_list.size();

	// For each entity call its own min_sync_cross_timeline_delay method,
	// passing the same threshold.  Save it, and adjust ans if
	//   -- ans is initialized to -1, or
	//   -- the entity's minimum delay is at least as large as thrs, AND
	//       is smaller than the working minimum
	//
	for(unsigned int i=0; i<end; i++) {
		ent_delay = __entity_list[i]->min_sync_cross_timeline_delay(thrs);
		if(ent_delay != -1) // -1 means everything is on the same timeline
		{
			if( thrs<= ent_delay && ((ans<0) || (ent_delay<ans))) ans = ent_delay;
		}
	}
	return ans;
}

/* ***************************************************************

unsigned int Timeline::count_min_delay_crossings(ltime_t thrs)

count the number of cross-timeline channels whose minimum
delay is less than or equal to thrs

 *******************************************************************/

unsigned int Timeline::count_min_delay_crossings(ltime_t thrs) {

	// initialize running total
	unsigned int ans = 0;

	// number of entities to visit
	unsigned int end = __entity_list.size();

	// each entity will count the number of such channels it has
	// and return the values
	//
	for(unsigned int i=0; i<end; i++) {
		ans += __entity_list[i]->count_min_delay_crossings(thrs);
	}

	// return summation
	return ans;
}

void Timeline::initialize_appt(bool first, ltime_t thrs) {

	__window_size = thrs;

	if(first) pthread_cond_init(&__appointment_cond_var, NULL);

	for(unsigned int i=0; i<__num_timelines; i++) {
		if(first) pthread_mutex_init(&__in_appt[i].appt_mutex, NULL);
		if(first) pthread_mutex_init(&__out_appt[i].appt_mutex, NULL);

		__in_appt[i].waiting = false;
		__in_appt[i].appointment = -1;   // flag for uninitialized
		__out_appt[i].lookahead  = -1;   // flag for uninitialized
	}

	// work through all cross-timeline channels, to determine
	// the lookahead between this Timeline and others
	//
	for(unsigned int i=0; i< __entity_list.size(); i++) {
		Entity* e = __entity_list[i];
		for(unsigned int j=0; j<e->__outchannel_list.size(); j++) {
			OutChannel *oc = e->__outchannel_list[j];
			for(unsigned int k=0; k<oc->__mapped_to.size(); k++) {
				if( oc->__mapped_to[k].xtimeline() ) {
					// this channel crosses timelines.  Figure the
					// lookahead as the sum of the channel per-write minimum and the
					// transfer time
					ltime_t lookahead = oc->min_write_delay()
  							+ oc->__mapped_to[k].transfer_delay();

					// if the lookeahd is less than thrs this is an asynchronous channel
					if( lookahead < thrs) oc->__mapped_to[k].set_asynchronous(true);
					else oc->__mapped_to[k].set_asynchronous(false);

					unsigned int tgt_tl = oc->__mapped_to[k].ic()->alignment()->s3fid();
					if( __out_appt[tgt_tl].lookahead < 0 ||
							lookahead < __out_appt[tgt_tl].lookahead ) {
						__out_appt[tgt_tl].lookahead = lookahead;
					}
				}
			}
		}
	}

	// for every Timeline whose lookahead is not -1 and is less than
	// the threshold,  schedule an event on the target
	// and one on the source, at the appointment time
	for(unsigned int i=0; i<__num_timelines; i++)	{
		if( __out_appt[i].lookahead != -1 && __out_appt[i].lookahead < thrs ) {

			// event for the source
			pthread_mutex_lock(&__events_mutex);
			{
				Event* e = new Event(__out_appt[i].lookahead, min_sched_pri-1,i,EVTYPE_MAKE_APPT, this, __evtnum++ );
				EventPtr eptr(e);

#ifdef DEBUG
				printf("timeline %d creates appt-write to %d for %ld\n",
						s3fid(), i, __out_appt[i].lookahead);
#endif
				__events.push( eptr );
			}
			pthread_mutex_unlock(&__events_mutex);

			// event for the destination
			pthread_mutex_lock(&timeline_adrs[i]->__events_mutex);
			{
				Event *e = new Event(__out_appt[i].lookahead, min_sched_pri,s3fid(),EVTYPE_WAIT_APPT, timeline_adrs[i], __evtnum++);
				EventPtr eptr(e);
#ifdef DEBUG
				printf("timeline %d schedules appt-read from %d for %ld\n",
						i, s3fid(), __out_appt[i].lookahead);
#endif
				timeline_adrs[i]->__events.push( eptr );
			}
			pthread_mutex_unlock(&timeline_adrs[i]->__events_mutex);
		}}
}

unsigned long Timeline::get_nxt_handle() {
	return __handle_idx++;
}

bool Timeline::global_continue() {
	if( __no_global_check ) return true;
	return !__interface_control->stop_condition();
}

/* ******************************************************

	static Timeline data elements

 ********************************************************/

// tie-breaking priority to use if none specified
int Timeline::default_sched_pri;
int Timeline::min_sched_pri;
vector<Timeline*> Timeline::timeline_adrs;


// total number of timelines created, used when assigning ids
unsigned int Timeline::__num_timelines = 0;

// mutex that protects access to __num_timelines
pthread_mutex_t Timeline::timeline_class_mutex;

#ifdef PTHREAD_BARRIER
pthread_mutex_t Timeline::bottom_barrier_min_value_mutex;
ltime_t Timeline::bottom_barrier_min_value = -1;
ltime_t Timeline::prev_bottom_barrier_min_value = -1;
#endif

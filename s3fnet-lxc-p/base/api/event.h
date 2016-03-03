/**
 * \file event.h
 * \brief S3F definition of Events that are execute to advance an S3F-based simulation
 */

#ifndef __EVENT_H__
#define __EVENT_H__

#ifndef __S3F_H__
#error "event.h can only be included by s3f.h"
#endif

const unsigned int EVTYPE_NULL=0;
const unsigned int EVTYPE_TIMEOUT=1;
const unsigned int EVTYPE_ACTIVATE=2;
const unsigned int EVTYPE_EXEC_ACTIVATE=3;
const unsigned int EVTYPE_WAIT_APPT=4;
const unsigned int EVTYPE_MAKE_APPT=5;
const unsigned int EVTYPE_BIND=6;
const unsigned int EVTYPE_CANCEL=7;

#define user_pri(p) (((p+1)<<8) & 0xffffff)

/**
  * Class Event holds all the information needed to execute an s3f event.
  * Fields are describe below:
  *
  * __EventType : an enumerated type that flags the type of the event
  *         to be done.  The possibilities are
  *         <BR> EVTYPE_TIMEOUT -- used when the event results from a waitFor
  *           call from a Process. 
  *         <BR> EVTYPE_ACTIVATE -- used when the event results from the
  *           presentation of an Activation instance to an InChannel
  *         <BR> EVTYPE_WAIT_APPT -- handle incoming appointment time from named timeline
  *         <BR> EVTYPE_MAKE_APPT -- create outgoing appointment time for named timeline
  *         <BR> EVTYPE_BIND      -- bind a process to an inchannel
  *         <BR> EVTYPE_CANCEL -- used to flag that this event should not
  *           be executed, is filtered before presenting to the simulation
  * 
  * __proc : Pointer to a Process, the one called when executing an 
  *            EVTYPE_TIMEOUT event
  *
  * __inc  : Pointer to an InChannel, the one to which an Activation is
  *            directed at the simulation time
  *
  * __act  : Pointer to an Activation, presented to Processes that are
  *            bound to or waiting on the InChannel in __inc, on a
  *            EVTYPE_ACTIVATE event
  *
  * __tl   : Pointer to a timeline involved in the appointment
  *
  * Time is not in this event, that is carried in the priority_queue
  * data structure that contains this one
  *
  **/
class Event {
 public:
   friend class STL_EventList;
   Event();
   Event(ltime_t,int, Process* proc, Activation act, Timeline* home_tl, unsigned int); 
   Event(unsigned int, ltime_t, int,InChannel* inc, Activation act, Process*, Timeline* home_tl, unsigned int);  
   Event(ltime_t,int,unsigned int tl, int type , Timeline* home_tl, unsigned int);
   Event(ltime_t,int, InChannel* inc, Process* proc, bool bind, unsigned int pri, Timeline* home_tl, unsigned int);

 ~Event(); 
  /* getor functions for all class state variables */

  Process*    get_proc();  
  int         get_evtype();
  Activation  get_act();   
  InChannel*  get_inc();  
  unsigned int get_tl(); 
  unsigned int get_pri();
  unsigned int get_evtnum();
  bool get_bind();      
  void cancel();
  void release();
  void* adrs();
  Event* get();
  ltime_t get_time()       { return __time; }
  Timeline* get_home_tl()  { return __home_tl; }  
  ltime_t     __time;     ///< not used in priority queue but placed here for information
  int         __key2;     ///< priority
  unsigned int  __evtnum; ///< inserted by timeline on scheduling
 protected:
  unsigned int  __EventType; ///< what to do with this event
  Process*   __proc;      ///< process to exec. on TIMEOUT
  InChannel*  __inc;      ///< InChannel on which Activation arrives
  unsigned int __tl;      ///< used for the appointment events
  unsigned int __pri;     ///< priority used for ordering
  bool        __bind;
  Activation  __act;      ///< Activation to deliver in case of ACTIVATE
  Timeline*   __home_tl;  ///< timeline on which this is scheduled
 };

#ifndef SAFE_EVT_PTR
typedef Event* EventPtr;
#else
typedef std::tr1::shared_ptr<Event> EventPtr;
#endif

class Handle {
 public:
   Handle(void *);
   Handle() {};
   ~Handle();
   Event*  __eptr;
   ltime_t __time;
   Timeline* __home_tl;
   bool cancel();
};

typedef std::tr1::shared_ptr<Handle> HandlePtr;
typedef void* HandleCode;

#endif /* __EVENT_H__ */

/**
 * \file event.cc
 * \brief Source file for the Event Class
*/

#include <s3f.h>

#ifndef SAFE_MSG_PTR
 Event::Event(ltime_t t, int key2, Process* proc, Activation act, Timeline* home_tl, unsigned int evtnum) : 
	__evtnum(evtnum), __EventType(EVTYPE_TIMEOUT),  __proc(proc), __act(act), __home_tl(home_tl)  
		{ if(act) act->inc_evts();
		  __time = t; __key2 = key2;
		};
 Event::Event(unsigned int etype, ltime_t t, int key2, InChannel* inc, Activation act, Process* proc, Timeline* home_tl, unsigned int evtnum) : 
	__evtnum(evtnum), __EventType(etype), __proc(proc),
	__inc(inc), __act(act),  __home_tl(home_tl) 
		{ if(act) act->inc_evts();
		  __time = t; __key2 = key2;
		} ;

#else
 Event::Event() : __EventType(EVTYPE_NULL) {};
 Event::Event(ltime_t t, int key2, Process* proc, Activation act, Timeline* home_tl, unsigned int evtnum) : 
	__evtnum(evtnum), __EventType(EVTYPE_TIMEOUT), __proc(proc), __act(act),  __home_tl(home_tl)  
		{ __time = t; __key2 = key2; };

 Event::Event(unsigned int etype, ltime_t t, int key2, InChannel* inc, Activation act, Process* proc, Timeline* home_tl, unsigned int evtnum) : 
	__evtnum(evtnum), __EventType(etype), __proc(proc), 
	__inc(inc), __act(act),  __home_tl(home_tl)   
		{ __time = t; __key2 = key2; };
#endif

Event::Event(ltime_t t, int key2, unsigned int tl, int type, Timeline* home_tl, unsigned int evtnum ) : 
	__evtnum(evtnum), __EventType(type), __tl(tl),  __home_tl(home_tl)  
		{ __time = t; __key2 = key2; };
Event::Event(ltime_t t, int key2, InChannel* inc, Process* proc, bool bind, unsigned int pri, Timeline* home_tl, unsigned int evtnum) :
	__evtnum(evtnum), __EventType(EVTYPE_BIND), __proc(proc), __inc(inc), __pri(pri), __bind(bind),  __home_tl(home_tl)  
		{ __time = t; __key2 = key2; };

Event* Event::get() { return this; }

#ifndef SAFE_EVT_PTR
 void Event::release() { delete this; } 
 void* Event::adrs()  { return (void *)this; }
#else 
 void  Event::release() {}
 void* Event::adrs()    { return (void *)this->get(); }
#endif

Event:: ~Event() { 
 // printf("called event %p dtor\n", this);
 #ifndef SAFE_MSG_PTR
 if( __EventType == EVTYPE_TIMEOUT || __EventType == EVTYPE_ACTIVATE ) {
	if( __act && __act->dec_evts() == 0) delete __act;
 } 
 #endif
}; 

Process*    Event::get_proc()         { return __proc; }
int         Event::get_evtype()       { return __EventType; }
Activation  Event::get_act()          { return __act; }
InChannel*  Event::get_inc()          { return __inc; }
unsigned int   Event::get_tl()       { return __tl; }
unsigned int   Event::get_pri()      { return __pri; }
unsigned int   Event::get_evtnum()   { return __evtnum; }
bool        Event::get_bind()         { return __bind; }
 
void  Event::cancel() { __EventType = EVTYPE_CANCEL; }

Handle::Handle(void * eptr) {
  __eptr = (Event*)eptr;
  __time = __eptr->get_time();
  __home_tl = __eptr->get_home_tl();
}

bool Handle::cancel() {
 if( __time <= __home_tl->now()) return false;
 __eptr->cancel();
 if( __time <= __home_tl->now()) return false;
 return true; 
}

Handle::~Handle() {};


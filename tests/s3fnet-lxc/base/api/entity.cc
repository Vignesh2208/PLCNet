/**
 * \file entity.cc
 * \brief Source file of the S3F Entity class
 *
 * authors : David Nicol, Dong (Kevin) Jin
 */

#include <s3f.h>
using namespace s3f;

/* ***************************************************************

  Entity::Entity(Timeline *tl)
  Entity::Entity(Timeline *tl, string name)
  
  An entity is always assigned to a Timeline upon construction.
  Optionally it may be given a text name (put into a directory
  for general access)
  
  For both constructors the heavy lifting is done by a call to
  method BuildEntity
  
 *******************************************************************/
 
  // no name version, initialize name to null, call the core constructing method
  Entity::Entity(Timeline *tl) : __name("") {
	BuildEntity(tl);
  }

  // Named version, put the string name in the directory,
  // and then call BuildEntity
  //
  Entity::Entity(Timeline *tl, string name) : __name(name) {

	// name directory is globally accessible by all threads so
	// protect it by a mutex for good measure
	pthread_mutex_lock(&entity_class_mutex);
	
	// EntityName is an STL map<string, Entity*>.  Make
	// sure the offered string name is not in the map already
	//
	if( EntityName.find(name) != EntityName.end() ) {
	  cout << "Error--duplicate declaration of Entity named " 
		<< name << endl;
	}
	else {
	   // add it to the Entity directory
	   EntityName[name] = this;
	}
	// release access to the Entity Directory
	pthread_mutex_unlock(&entity_class_mutex);

    // do the rest of the construction work
	BuildEntity(tl);
  } 

 Entity::~Entity() {
	if( !s3fid() ) pthread_mutex_destroy(&entity_inst_mutex);
 }

  /* **********************************************************
  
  void Entity::BuildEntity(Timeline *tl )
  
  Finish constructing the Entity. 
    -- save the Timeline pointer
    -- build the tables used to support tick - double precision conversion
    -- give the entity a unique name
    -- put the entity onto its Timeline's list of entities.
  
  **************************************************************/
  
  void Entity::BuildEntity(Timeline *tl ) {
  	
  	// remember alignment, often used to check for cross-timeline connections
	__timeline = tl;

    // initialize the mutex used to serialize access to the individual
    // Entity state (not the static class variables).
    pthread_mutex_init(&entity_inst_mutex,NULL);
    
    // get unique Entity id. Global access so protect by mutex
	pthread_mutex_lock(&entity_class_mutex);
	__s3fid = __num_entities++;
	pthread_mutex_unlock(&entity_class_mutex);

	// first entity builds the pow[] table, used in tick - double precision conversion
	if( __s3fid == 0 ) {
		for(int i=0; i<36; i++) __pow[i] = pow(10.0, (double)(i-18));
 	}


	// copy time-scale information from the Timeline
	__log_ticks_per_sec = tl->get_log_ticks_per_sec();

	// record entity on timeline's list of entities
	pthread_mutex_lock(&tl->timeline_inst_mutex);
	tl->__entity_list.push_back(this);
	pthread_mutex_unlock(&tl->timeline_inst_mutex);
   }
	
  /* **********************************************************
	
	
	ltime_t Entity::now()
	
	return the time of the Entity's Timeline---the time of the current
	event execution or end of the synchronization window
	
	**************************************************************/
  ltime_t Entity::now() { return __timeline->now(); };

	/* **********************************************************
	
	Timeline* Entity::alignment()
	
	return a pointer to the Timeline on which the Entity is aligned
	
	**************************************************************/ 
  Timeline* Entity::alignment() { return __timeline; } 

	/* **********************************************************
	
	HandleCode Entity::waitFor(Process* proc, Activation act, ltime_t delay)
	HandleCode Entity::waitFor(Process* proc, Activation act, ltime_t delay, unsigned int pri )
	HandleCode Entity::waitFor(Process* proc, ltime_t delay, unsigned int pri ) 
	HandleCode Entity::waitFor(Process* proc, ltime_t delay) 
	
    A waitFor method tells the Timeline scheduler to schedule an activation of
      the named process, at a time equal to the current time plus
      the delay. The named process must be owned by an entity
      aligned to this timeline---checked by the Timeline::schedule
      method. 

    Four variations exist, working through all combinations of including
    an Activation or not, and including a priority or not.
      If no Activation is included, one is created with NULL as the
    protected pointer.  If no priority is included, Timeline::default_schedule_pri
    is used as a basis.  All priorities are passed through a transformation
    user_pri  that creates a block of priorities higher than any user can
    specify, and a block that are lower than any user can specify, in order
    to give the system the ability to schedule events assured to occur before
    any user event, or after any user event
	
	**************************************************************/ 

 
   HandleCode Entity::waitFor(Process* proc, Activation act, ltime_t delay) {
      return alignment()->schedule( proc, now()+delay, user_pri(Timeline::default_sched_pri), act);
   } 

   HandleCode Entity::waitFor(Process* proc, Activation act, ltime_t delay, unsigned int pri ) {
      return alignment()->schedule( proc, now()+delay, user_pri(pri), act);
   }

   HandleCode Entity::waitFor(Process* proc, ltime_t delay, unsigned int pri ) {
     Message* m = NULL;
     Activation act(m);
     return alignment()->schedule( proc, now()+delay, user_pri(pri), act);
   }

   HandleCode Entity::waitFor(Process* proc, ltime_t delay) {
     Message* m = NULL;
     Activation act(m);
     return alignment()->schedule( proc, now()+delay, user_pri(Timeline::default_sched_pri), act);
   }

   HandleCode Entity::waitFor(Activation act, ltime_t delay) {
      return alignment()->schedule( alignment()->__this, now()+delay, user_pri(Timeline::default_sched_pri), act);
   } 

   HandleCode Entity::waitFor(Activation act, ltime_t delay, unsigned int pri ) {
      return alignment()->schedule( alignment()->__this, now()+delay, user_pri(pri), act);
   }

   HandleCode Entity::waitFor(ltime_t delay, unsigned int pri ) {
     Message* m = NULL;
     Activation act(m);
     return alignment()->schedule( alignment()->__this, now()+delay, user_pri(pri), act);
   }

   HandleCode Entity::waitFor(ltime_t delay) {
     Message* m = NULL;
     Activation act(m);
     return alignment()->schedule( alignment()->__this, now()+delay, user_pri(Timeline::default_sched_pri), act);
   }


/* **********************************************************
	
	Entity* Entity::getEntity(string name) 
		
	Entities whose constructors were given string names are listed
	in an Entity class directory.   Given the name, this method
	looks up a pointer to the Entity (if present in the directory),
	otherwise it returns NULL
		
 **************************************************************/ 

 Entity* Entity::getEntity(string name) {
	
	// protect global directory using entity class mutex
	pthread_mutex_lock(&entity_class_mutex);
	
	// look for the name in the directory. 
	map<string,Entity*>::const_iterator itr 
		= EntityName.find(name);
	
	// if the iterator points to the end of the map we didn't find it
	if( itr == EntityName.end()) {
			// make list available to others and return NULL
			pthread_mutex_unlock(&entity_class_mutex);	
 	 		return (Entity*) NULL;
	}
	// Entity found.  Make directory available to others and return the
	// pointer
	pthread_mutex_unlock(&entity_class_mutex);	
	return (*itr).second;
 }

/* *********************************************************
	
	 double  Entity::t2d(ltime_t t)
	 double  Entity::t2d(ltime_t t, int s)
	 ltime_t Entity::d2t(double d)
	 ltime_t Entity::d2t(double d, int s)
	 
 Conversion routines for ticks and doubles.
 
 S3F's clock is discrete, fixed point binary where
    the position of the binary point is declared when
    the simulation is constructed  (units in 
    Timeline::__log_ticks_per_sec).
      Often we need to
    representing floating point numbers in this discrete
    format, and vice-versa.  Methods t2d and d2t are provided
    for this translation. The version version assumes that the 
    double is expressed in the same units as the clock ticks.  

     t2d transforms ltime_t -> double
     d2t transforms double -> ltime_t
 
    The second version includes a scale factor s describing the double.
    s is likewise in log_10(clock ticks per sec). 
    
    That is, given clock tick variable C and
    scale factor s, t2d will transform C into a double length
    floating point representation of C x 10^{-s} virtual seconds. 
    d2t with scale argument s means that the units of the input
    are 10^{s} per unit second, and will create a ltime_t value
    at the scale that all clock values share.

 **************************************************************/ 


  // units of double are identical to units of clock ticks
  double  Entity::t2d(ltime_t t) { return (double)t; }

  // units of double are s log_10(clock ticks per second), so 
  // transformation is required
  //
  double  Entity::t2d(ltime_t t, int s) { 
	int scale = 18+(s-__log_ticks_per_sec);
	if(scale < 36 && scale>=0 ) return t*__pow[scale]; 
	return t*pow(10.0, scale-18);
  }

 // units of double are identical to units of clock ticks
  ltime_t Entity::d2t(double d)  { return (ltime_t)d; }

  // units of double are s log_10(clock ticks per second), so 
  // transformation is required
  //
  ltime_t Entity::d2t(double d, int s) {
	int scale = 18+(__log_ticks_per_sec-s);
	if(scale < 36 && scale>=0 ) return (ltime_t)round(d*__pow[scale]);
	else return (ltime_t)round(d*pow(10.0, scale-18));
  }

 /* ****************************************************************

    ltime_t Entity::min_sync_cross_timeline_delay(ltime_t thrs)
    
    examine all outbound connections that cross timelines and have
    a minimum delay of at least thrs.  Return the least min_delay
    value among these.
 
 ***************************************************************/
 ltime_t Entity::min_sync_cross_timeline_delay(ltime_t thrs) {
   // initialize search answer to "nothing here"
   ltime_t ans = -1;
   
   // buffer the minimum read up from a channel connection
   ltime_t ch_min;
   
   // length of the list of this entity's outchannels
   unsigned int end = __outchannel_list.size();
   
   // visit every one of the outchannels this entity owns
   for(unsigned int i=0; i<end; i++) {
   	
   	// ask it to provide the minimum cross timeline delay at least as
   	// large as thrs, among all its mappings
   	//
	ch_min = __outchannel_list[i]->min_sync_cross_timeline_delay(thrs);
	
	// save that value if it is no larger than thrs, AND either this is
	// first one we've seen, or its value is less than the smallest one
	// seen to date.
	//
	if( (-1 < ch_min) && thrs<=ch_min && ((ans<0) || (ch_min<ans))) ans = ch_min;
   }
   
   // final answer is in ans
   return ans;
 }
 
 
  /* ****************************************************************

   unsigned int Entity::count_min_delay_crossings(ltime_t thrs)  
   
   Count the number of outbound cross timeline connections with minimum
   delay at least as large as thrs.  
 
 ***************************************************************/
 unsigned int Entity::count_min_delay_crossings(ltime_t thrs) {

  // initialize running sum
  unsigned int ans = 0;
  
  // number of OutChannels owned by this entity
  unsigned int end = __outchannel_list.size();
  
  // For each OutChannel as it to count the number of outbound
  // cross timeline connections with minimum delay less than or equal
  // to thrs
  // 
  for(unsigned int i=0; i<end; i++) {
    ans += __outchannel_list[i]->count_min_delay_crossings(thrs);
  }
  
  // return accumulated sum
  return ans;
}


  /* ****************************************************************

   InChannel* Entity::getInChannel(string ic_name) 
   
   At time of construction and InChannel can declare a string name for
  itself in a name space local to the Entity (i.e., it must be unique
  to all InChannels on that Entity, but may be duplicated on other 
  Entities.
  
  getInChannel(string) takes a name thought to be that of an InChannel
  owned by this entity, and looks up its address.  NULL is returned
  if the string is not found, otherwise a pointer to the InChannel is 
  returned.
 
 ***************************************************************/
 InChannel* Entity::getInChannel(string ic_name) {

    // off timeline entities may query this directory, so protect
    // access with a per-entity mutex.
	pthread_mutex_lock(&entity_inst_mutex);
	
	// see if we can find the named string
	map<string,InChannel*>::const_iterator itr = InChannelName.find(ic_name);
	
	// nope.  release the mutex and return NULL
	if( itr == InChannelName.end()) {
			pthread_mutex_unlock(&entity_inst_mutex);	
 	 		return (InChannel*) NULL;
	}

    // release the mutex and return the InChannel pointer
	pthread_mutex_unlock(&entity_inst_mutex);	
	return (*itr).second; 
}

  /* ****************************************************************

   Process* Entity::getProcess(string p_name) 
   
   At time of construction a Process can declare a string name for
  itself in a name space local to the Entity (i.e., it must be unique
  to all Processes on that Entity, but may be duplicated on other 
  Entities.
  
  getProcess(string) takes a name thought to be that of an Process
  owned by this entity, and looks up its address.  NULL is returned
  if the string is not found, otherwise a pointer to the Process is 
  returned.
 
 ***************************************************************/
 Process* Entity::getProcess(string p_name) {

    // off timeline entities may query this directory, so protect
    // access with a per-entity mutex.
	pthread_mutex_lock(&entity_inst_mutex);
	
	// see if we can find the named string
	map<string,Process*>::const_iterator itr = ProcessName.find(p_name);
	
	// nope.  release the mutex and return NULL
	if( itr == ProcessName.end()) {
			pthread_mutex_unlock(&entity_inst_mutex);	
 	 		return (Process*) NULL;
	}

    // release the mutex and return the Process pointer
	pthread_mutex_unlock(&entity_inst_mutex);	
	return (*itr).second; 
}

  /* ****************************************************************

     Static data members of class Entity
     
  ***************************************************************/
 map<string,Entity*> Entity::EntityName; 
 pthread_mutex_t Entity::entity_class_mutex;
 unsigned int Entity::__num_entities = 0;
 double Entity::__pow[36];  



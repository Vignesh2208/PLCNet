/**
 * \file inchannel.cc
 * \brief Source file for the S3F InChannel
 *
 * authors : David Nicol, Dong (Kevin) Jin
 */

#include <s3f.h>
using namespace s3f;

/* ***************************************************************

	InChannel::InChannel(Entity* theowner)
  	InChannel::InChannel(Entity* theowner, string icname)

  	InChannel constructors must declare an Entity that owns the InChannel.
  	The constructor acquire a unique id for the instance. Inclusion of a
  	string name adds that name to the directory of the Entity that owns it.

 **************************************************************/

InChannel::InChannel(Entity* theowner) : __extname(string("")),
		__owner(theowner), __alignment(theowner->alignment())  {

	// the initialization above saves the identity of the owner, and
	// the timeline on which the owner is aligned.  It initializes the
	// string name to "" because none other was provided.

	// get unique id number
	pthread_mutex_lock(&inchannel_class_mutex);
	__s3fid = __num_inchannels++;
	pthread_mutex_unlock(&inchannel_class_mutex);

	// register inchannel with entity that owns it	
	pthread_mutex_lock(&theowner->entity_inst_mutex);
	theowner->__inchannel_list.push_back(this);	
	pthread_mutex_unlock(&theowner->entity_inst_mutex);

}

InChannel::InChannel(Entity* theowner, string icname) :
    		__extname(icname), __owner(theowner), __alignment(theowner->alignment()) {

	// the initialization above saves the identity of the owner, and
	// the timeline on which the owner is aligned.  It initializes the
	// string name to that which was given.

	// get a unique id
	pthread_mutex_lock(&inchannel_class_mutex);
	__s3fid = __num_inchannels++;
	pthread_mutex_unlock(&inchannel_class_mutex);

	// put the string-inchannel pair into the owning Entity's
	// InChannelName directory
	pthread_mutex_lock(&theowner->entity_inst_mutex);

	// make sure the offered string name is not in the map already
	if( theowner->InChannelName.find(icname) != theowner->InChannelName.end() ) {
		cout << "Error--duplicate declaration of InChannel named "
				<< icname << endl;
	}
	else {
		// add it to the InChannel names list
		//
		theowner->InChannelName[icname] = this;
	}

	// add a pointer to this InChannel to a list the Entity maintains
	// of all InChannels it owns.
	theowner->__inchannel_list.push_back(this);	
	pthread_mutex_unlock(&theowner->entity_inst_mutex);
}

InChannel::~InChannel() {
	if(! s3fid() ) pthread_mutex_destroy(&inchannel_class_mutex);
}

/* *********************************************************

	void InChannel::waitOn()
    void InChannel::waitOn(Process* proc)

    Attach a Process to the InChannel's list of Processes waiting
    for execution when an Activation arrives.  One form uses the
    named Process, the other form attaches the currently executing 
    Process.

    Unlike the original SSF, there are no assumed or implied 
    suspension semantics.  Control is returned from the waitOn
    call and the calling process may go on to do other things.

 **************************************************************/

void InChannel::waitOn() {
	// timeline has pointer to the currently running process
	//  Register that Process on the InChannel's waiting list.  The priority
	//  for activation is the processes' priority
	assert( alignment()->get_running_proc() != NULL);
	add_process( alignment()->get_running_proc() );
}

void InChannel::waitOn(Process* proc) {
	//  Register *proc on the InChannel's waiting list.  The priority
	//  for activation is *proc's own priority
	add_process( proc );
}

/* *********************************************************

	void InChannel::insert_process_pri( ProcessPri pp )

	Called to put a description of a Process to activate in the
	InChannel's list of such.  The ProcessPri structure identifies
	the Process's owner (needed because the Process code body is 
	an Entity method), the Process code body, whether the association
	persists (bound) after an activation, and the priority on the list.

	The method does a linear insertion into the list, placing the
	element immediately before the first entry it encounters
	(scanning by increasing priority) with a priority that is strictly
	larger.  This implies that insertions with the same priority are
	ordered FCFS among themselves.

 ********************************************************/



// check alignment, and then see if the timeline's active channel
// is in fact this one.
void InChannel::insertion_gateway(Process* proc, bool b, unsigned int pri) {
	assert( alignment() == proc->alignment() );

	// if the timeline's activeChannel is not this inChannel we can go ahead
	// and implement it
	if( alignment()->get_activeChannel() != this ) {
		insert_process_pri( proc, b, user_pri(pri) );
	} else {
		// we have to avoid an infinite loop, so instead schedule an event
		// to do this insertion, to occur next after the present event processing finishes
		alignment()->schedule(this, proc, b, user_pri(pri) );
	}
}


void InChannel::insert_process_pri( Process* proc, bool b, unsigned int pri ) {

	ProcessPri pp(proc, b, pri);

	for(std::list<ProcessPri>::iterator list_iter = __waitingList.begin();
			list_iter != __waitingList.end(); list_iter++) {

		// insert pp if the element currently viewed has a priority
		// that is strictly larger than pp's
		//
		if( (*list_iter).get_pri() > pp.get_pri()) {
			__waitingList.insert(list_iter, pp);
			return;
		}
	}
	// hit the end of the list, which means there is no existing element
	// with a strictly larger priority, so that this one goes at the end
	__waitingList.push_back( pp );
	return;
}

/* *********************************************************

	void InChannel::add_process_pri( ProcessPri pp )

	This method creates a ProcessPri structure to pass to insert_process_pri,
	is called from a waitOn call, the two versions of which select
	the Process argument passed to add_process 

 ********************************************************/

void InChannel::add_process( Process * proc) {
	insertion_gateway( proc, false, proc->pri() );
}

/* *********************************************************

   void InChannel::bind( Process * proc )
   void InChannel::bind( )

   bind is like waitOn, save that the attachment persists across 
   activations.  Just as with waitOn, the version called picks
   the correct process to pass to insert_process_pri.

 ********************************************************/


void InChannel::bind( Process * proc ) {
	insertion_gateway( proc, true, proc->pri());
}

void InChannel::bind( ) {
	Process* proc = alignment()->get_running_proc();

	// have to check that there is actually a running process now,
	// throw a fatal error if there is not
	assert( proc != NULL);

	insertion_gateway( proc, true, proc->pri());
}

/* *********************************************************

   bool InChannel::unbind( Process * proc )
   bool InChannel::unbind( )

   Unattach the named process from the InChannel's list of attached processes.
   Return true if found and is bound.  return false otherwise.

 ********************************************************/

bool InChannel::unbind( Process * proc ) {

	// loop over the InChannel's waiting list
	for(std::list<ProcessPri>::iterator list_iter = __waitingList.begin(); 
			list_iter != __waitingList.end(); list_iter++) {

		// is the current element associated with the named Process, and is it
		// bound?
		if( proc == (*list_iter).get_proc() && (*list_iter).get_bound() ) {

			// yes, remove it from the list and return true
			__waitingList.erase(list_iter);
			return true;
		}
	}
	// didn't find it so return false
	return false;
}

bool InChannel::unbind( ) {
	Process *proc = alignment()->get_running_proc();
	assert( proc != NULL);
	return unbind( proc );
}


/* *********************************************************

   bool InChannel::unwaitOn( Process * proc )
   bool InChannel::unwaitOn(  )

   Unattach the named process from the InChannel's list of attached processes.
   Return true if found and unbound, return false otherwise.

 ********************************************************/

bool InChannel::unwaitOn( Process * proc ) {

	// loop over the InChannel's waiting list
	for(std::list<ProcessPri>::iterator list_iter = __waitingList.begin(); 
			list_iter != __waitingList.end(); list_iter++) {

		// is the current element associated with the named Process, and is it
		// bound?
		if( proc == (*list_iter).get_proc() && !(*list_iter).get_bound() ) {

			// yes, remove it from the list and return true
			__waitingList.erase(list_iter);
			return true;
		}
	}
	return false;
}

bool InChannel::unwaitOn( ) {
	Process *proc = alignment()->get_running_proc();
	assert( proc != NULL);
	return unwaitOn( proc );
}



/* *****************************************************

   bool InChannel::is_bound( Process *proc )
   bool InChannel::is_bound( )

   Return true if named process is attached to InChannel
   waiting list and is bound.  False otherwise

 ********************************************************/

bool InChannel::is_bound( Process *proc ) {

	bool ans = false;
	// scan entire list
	for(std::list<ProcessPri>::iterator list_iter = __waitingList.begin();
			list_iter != __waitingList.end(); list_iter++) {

		// return true if we found the same process, and it is bound
		if ( proc == (*list_iter).get_proc()
				&& (*list_iter).get_bound() ) {
			ans = true;
			break;
		}
	}
	return ans;
}


bool InChannel::is_bound( ) {
	Process *proc = alignment()->get_running_proc();
	assert( proc != NULL);
	return is_bound( proc );
}

/* *****************************************************

   bool InChannel::is_waiting( Process *proc )
   bool InChannel::is_waiting( )

   Return true if named process is attached to InChannel
   waiting list and is not bound.  False otherwise

 ********************************************************/

bool InChannel::is_waiting( Process * proc ) {
	bool ans = false;

	// scan entire list
	for(std::list<ProcessPri>::iterator list_iter = __waitingList.begin(); 
			list_iter != __waitingList.end(); list_iter++) {

		// return true if we match the process and the entry is not bound
		if( proc == (*list_iter).get_proc() && !(*list_iter).get_bound() )
		{ ans = true;
		break;
		};
	}

	return ans;
}

bool InChannel::is_waiting( ) {
	Process *proc = alignment()->get_running_proc();
	assert( proc != NULL);
	return is_waiting( proc );
}


/* *****************************************************

       Static member variables

 ****************************************************/

// protects access to count of inchannels
pthread_mutex_t InChannel::inchannel_class_mutex;

// enumerates inchannels
unsigned int InChannel::__num_inchannels = 0;



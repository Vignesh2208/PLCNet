/**
 * \file process.cc
 *
 * \brief S3F Process Methods
 *
 * authors : David Nicol, Dong (Kevin) Jin
 */

#include <s3f.h>

/* ***************************************************************

  Process::Process(Entity* owner, void (Entity::*func)(Activation))
  Process::Process(Entity* owner, string name, void (Entity::*func)(Activation))

  A Process always has an Entity owner and a pointer to a method
   associated with that owner.  Without reflection there is no easy way
   to check this relationship.  Optionally a string name may be given
   to identify the Process and look up its address.

 *******************************************************************/

Process::Process(Entity* owner, void (Entity::*func)(Activation)) : 
__owner(owner), __func(func),
__active(0), __pri(Timeline::default_sched_pri) {

	__alignment = owner->alignment();

	// initialization above ensures that it does not appear as though
	// the Process was activated, and stores a default scheduling
	// priority

	// get the unique id, serialized by mutux
	pthread_mutex_lock(&process_class_mutex);
	__s3fid = __num_processes++;
	pthread_mutex_unlock(&process_class_mutex);

	// add this process to the list of processes maintained by the owning
	// entity, serialized by entity mutex
	pthread_mutex_lock(&owner->entity_inst_mutex);
	owner->__process_list.push_back(this);
	pthread_mutex_unlock(&owner->entity_inst_mutex);
};

Process::Process(Entity* owner, string name, void (Entity::*func)(Activation)) :
			__owner(owner), __func(func),
			__active(0), __pri(Timeline::default_sched_pri) {

	// initialization above ensures that it does not appear as though
	// the Process was activated, and stores a default scheduling
	// priority

	// get the unique id, serialized by mutux
	pthread_mutex_lock(&process_class_mutex);
	__s3fid = __num_processes++;
	pthread_mutex_unlock(&process_class_mutex);

	// add this process to the owning entity ProcessName map
	// and put it on the owning Entity's list of processes
	//    protect Entity state by serializing on owning Entity's
	// mutex
	pthread_mutex_lock(&owner->entity_inst_mutex);
	owner->__process_list.push_back(this);

	if( owner->ProcessName.find(name) != owner->ProcessName.end() ) {
		cout << "Error--duplicate declaration of Process " << name
				<< " at Entity id " << owner->s3fid() << endl;
	} else {
		owner->ProcessName[name] = this;
	}
	pthread_mutex_unlock(&owner->entity_inst_mutex);
};


/* ***************************************************************
		STATIC CLASS VARIABLES
 *****************************************************************/

// protect access to __num_processes counter
pthread_mutex_t Process::process_class_mutex;

// enumerates number of created processes globally
unsigned int Process::__num_processes = 0;

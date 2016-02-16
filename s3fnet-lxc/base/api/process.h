/**
 * \file process.h
 *
 * \brief Header files of the S3F Process class
 *
 * authors : David Nicol, Dong (Kevin) Jin
 */

#ifndef __PROCESS_H__
#define __PROCESS_H__

#ifndef __S3F_H__
#error "process.h can only be included by s3f.h"
#endif

/**
 * 	The code that executes should be thought of as embedded in an instant of time.
 * 	No time advances while it executes.  When the routine exits via normal C+
 * 	runtime semantics, the Timeline scheduler may choose another Process to execute,
 * 	with an associated timestamp that may be farther in the future.  Simulation time
 * 	advances with discrete leaps from the times of these various Process executions.
 * 	Process code bodies may make calls to waitFor or waitOn.  In the original version
 * 	of SSF these cause the process code body to suspend.  S3F does not.  These are
 * 	calls that schedule a future activation (waitFor) or attach Process to an InChannel
 * 	to be activated when a message arrives.  Normal C++ runtime semantics hold throughout.
 * 	From the point of runtime behavior here is nothing special about calls to waitFor,
 * 	or waitOn, and no assumptions or restrictions about where they may appear in the
 * 	code (e.g. in SSF, they had to the last statements executed if the Process was "simple")
 */
class Process 
{
public:
	/**
	 * constructor passes pointer to the Entity that "owns" this Process
	 * and a pointer to one of its methods, that will be used as a code body for the Process.
	 * S3F does assume (and cannot check, as far as I can tell) that the method is part
	 * of the Entity named as owner.
	 *
	 * A Process always has an Entity owner and a pointer to a method associated with that owner.
	 * Without reflection there is no easy way to check this relationship.
	 * Optionally a string name may be given to identify the Process and look up its address.
	 *
	 */
	Process(Entity*, void (Entity::*)(Activation));

	/** Null constructor */
	Process() {};

	/** Returns a pointer to the Entity owning this Process */
	inline Entity* 	owner() 			{ return __owner;  }

	/**
	 * When the Process body is executing, the Process can obtain using this method
	 * a pointer to the InChannel where an Activation arrival trigger the execution (if any).
	 * A return of NULL indicates that a waitFor triggered the execution, not a waitFor.
	 */
	inline InChannel* activeChannel()		{ return __active; }

	/** Returns a unique Process identifier, indexed between 0 and the total number of Processes created. */
	unsigned int 		s3fid()				{ return __s3fid; }

	/** Returns a pointer to the Timeline of the Process owner */
	Timeline*         alignment()         { return __alignment; }

	/* ******************************************************
           	Public Auxiliary Methods
	 ****************************************************/
	/**
	 * A Process can be constructed and give a text name identifier to
	 * be used to look up the address of the Process, given the name.
	 * The name is unique to the Entity, but may be duplicated on other
	 * Entities.
	 */
	Process(Entity*, string, void (Entity::*)(Activation));

	/**
	 *  Processes may be given priorities used to order the sequence of
	 *  activations when a number of Processes are waiting on the same InChannel.
	 *  These priorities are the same as the scheduling priorities.
	 *  pri() returns the priority.
	 */
	inline int pri()				{ return __pri; }

	/** set_pri() assigned the priority */
	inline void set_pri(int pri)	{ __pri = pri; }

	/* ***********************************************
 	 METHODS FOR INTERNAL USE AND FRIENDS
	 ****************************************************/
protected:
	friend class Entity;
	friend class Interface;
	friend class InChannel;
	friend class OutChannel;
	friend class Timeline;

	/** Record which InChannel caused the current Process activation */
	void put_activeChannel( InChannel* ac )  { __active = ac;   }

	/* ***********************************************
 	 INTERNAL STATE VARIABLES
	 ****************************************************/
	/** Entity that owns this process */
	Entity* __owner;

	/** the function pointer to use when process is called */
	void (Entity::*__func)(Activation);

	/** InChannel that triggered activation (due to waitOn) or NULL if triggered by waitFor */
	InChannel* __active;

	/** priority that can be given to a process */
	int __pri;

	/** unique identifier */
	unsigned int __s3fid;

	/** cached value of owner's timeline */
	Timeline* __alignment;

	/* ***********************************************
 	 STATIC STATE VARIABLES
	 ****************************************************/
	/** variable used to enumerate processes as created */
	static unsigned int __num_processes;

	/** mutex protection for access to Process state */
	static pthread_mutex_t process_class_mutex;
};

#endif /*__PROCESS_H__*/

/**
 * \file inchannel.h
 * \brief Header file for the S3F InChannel
 *
 * authors : David Nicol, Dong (Kevin) Jin
 */

#ifndef __INCHANNEL_H__
#define __INCHANNEL_H__

#ifndef __S3F_H__
#error "inchannel.h can only be included by s3f.h"
#endif

/**
 * class ProcessPri is a helper class used internally
 * to order processes to be activated when an activation
 * arrives at the InChannel. It holds all that is needed
 * to execute the activation. In addition it has a priority
 * field used to order the list (Processes with lower priority
 * value execute earlier) and a flag that indicates whether
 * the process is "bound" to the InChannel, meaning that it
 * remains on the list even after an activation. Unbound
 * Processes are removed.
 */
class ProcessPri {
public:
	ProcessPri(Process *p, bool b, int pri) : __proc(p), __b(b), __pri(pri) {
		__entity = p->owner();
	};
	inline Process* get_proc()   { return __proc; }
	inline Entity*  get_entity() { return __entity; }
	inline bool     get_bound()  { return __b;    }
	inline int      get_pri()    { return __pri;    }

protected:
	Process* __proc;
	Entity*  __entity;
	bool     __b;
	int      __pri;
};

/**
 *  An InChannel is the endpoint of connections made by
 *  an OutChannel's mapto call.  If an InChannel is named
 *  as the target of such a call, then whenever the OutChannel
 *  write method is used to send an Activation, Processes that
 *  somehow have been attached to the Inchannel are activated.
 *
 *  A Process may be attached by calling InChannel methods bind,
 *  or waitOn.  The former attachment persists after an activation
 *  is made, whereas the latter does not.  Binds and waitOn calls
 *  may be undone by calls unbind and unwaitOn.
 */
class InChannel 
{
public:

	/* ******************************************************
        PUBLIC INTERFACE TO CLASS INCHANNEL 
	 *****************************************************/

	/** InChannels are owned by Entities; at construction the identity of the owning Entity must be given */
	InChannel(Entity*);
	/** Have to declare a dtor, but nothing intelligent is done by this implementation if an InChannel were deleted. */
	virtual ~InChannel();
	/** Returns a pointer to the Entity that owns this InChannel */
	inline Entity* owner() { return __owner; }
	/** Returns a pointer to the Timeline on which the InChannel's owning entity is aligned. */
	inline Timeline* alignment()    { return __alignment; }
	/** waitOn() may be called by an executing Process, in which case that process is attached to the InChannel.
	 *  It is automatically dis-attached after completing the activation.*/
	void waitOn();
	/** The named Process is attached to the InChannel to be activated on the next Activation to arrive.
	 *  It is automatically dis-attached after completing the activation.
	 *  Unlike the original SSF, there are no assumed or implied suspension semantics.
	 *  Control is returned from the waitOn call and the calling process may go on to do other things.
     */
	void waitOn(Process*);
	/** UnwaitOn disconnects the named process from the InChannel list.
	 *  Returning true if the Process was found and removed, false otherwise.
	 *  Unlike the original SSF, there are no assumed or implied suspension semantics.
	 *  Control is returned from the waitOn call and the calling process may go on to do other things.
	 */
	bool unwaitOn(Process*);
	/** UnwaitOn disconnects the current running process from the InChannel list.
	 *  Returning true if the Process was found and removed, false otherwise. */
	bool unwaitOn( );
	/** The named Process is attached to the InChannel to be activated on the next Activation to arrive.
	 * It remains attached after an process activation.*/
	void bind( );
	/** bind() may be called by an executing Process, in which case that process is attached to the InChannel.
	 * It remains attached after an process activation. */
	void bind( Process *);
	/** Removes the named Process from the list of Processes attached to the InChannel.
	 * True returned if the Process found and removed, false otherwise. */
	bool unbind( Process *);
	/** Removes the running Process from the list of Processes attached to the InChannel.
	 *  True returned if the Process found and removed, false otherwise. */
	bool unbind( );
	/** Returns true if the named Process is attached to the InChannel but not bound to it. */
	bool is_waiting( Process * );
	/** Returns true if the running Process is attached to the InChannel but not bound to it. */
	bool is_waiting();
	/** Returns true if the named Process is bound to the InChannel. */
	bool is_bound( Process * );
	/** Returns true if the running Process is bound to the InChannel. */
	bool is_bound();
	/** Returns the unique id assigned to this InChannel. */
	unsigned int s3fid()    { return __s3fid; }

	/* ******************************************************
        PUBLIC INTERFACE TO CLASS INCHANNEL EXTENSION 
	 *****************************************************/
	/** An InChannel may declare a string at construction, which is put in a directory at its owning Entity. */
	InChannel(Entity*, string);

	/* *****************************************************
        INTERNAL METHODS  
	 ********************************************************/

protected:
	friend class Entity;
	friend class OutChannel;
	friend class Timeline;
	friend class Interface;

	/** add_process is called by the two versions of waitOn to attach the named Process, but not bind it.
	    If the process is already attached---an error---the request is ignored. */
	void add_process( Process *);

	/** check alignment, and then see if the timeline's active channel is in fact this one. */
	void insertion_gateway( Process *, bool, unsigned int);

	/** insert_process_pri is called both as a result of calling waitOn and as a result of calling bind.
	 *  The ProcessPri structure has all the information needed to activate the Process,
	 *  holds whether the Process is bound or not, and has the priority used to sequence
	 *  of activations of Processes that are attached to the same InChannel.
	 *
	 *  Called to put a description of a Process to activate in the InChannel's list of such.
	 *  The ProcessPri structure identifies the Process's owner (needed because the Process code body is an Entity method),
	 *  the Process code body, whether the association persists (bound) after an activation, and the priority on the list.
	 *
	 *  The method does a linear insertion into the list, placing the element immediately
	 *  before the first entry it encounters (scanning by increasing priority) with a priority
	 *  that is strictly larger. This implies that insertions with the same priority are ordered FCFS among themselves.
	 */
	void insert_process_pri( Process *, bool, unsigned int );

	/* ******************************************************
        INTERNAL STATE VARIBLES
	 *****************************************************/

	/** Ordered list of processes attached to the InChannel, awaiting activation.
	 *  List is sorted low to high by the priority field in the ProcessPri structure */
	list<ProcessPri> __waitingList;

	string    __extname;     ///< name of the inchannel (if apply)
	Entity*   __owner;       ///< owner of the channel
	Timeline* __alignment;   ///< timeline of the entity that owns this InChannel
	unsigned int __s3fid;    ///< unique id


	/* ******************************************************
        STATIC CLASS VARIBLES
	 *****************************************************/

	/** mutex protects access to __num_inchannels */
	static pthread_mutex_t inchannel_class_mutex;

	/** enumerate the number of inchannels we have created */
	static unsigned int __num_inchannels;
};

#endif /*__INCHANNEL_H__*/

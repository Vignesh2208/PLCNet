/**
 * \file entity.h
 * \brief S3F Entity
 *
 * authors : David Nicol
 * 			 Dong (Kevin) Jin
 */

#ifndef __ENTITY_H__
#define __ENTITY_H__

#ifndef __S3F_H__
#error "entity.h can only be included by s3f.h"
#endif

/** In S3F the Entity object holds the simulation state,
   the methods that express the logic of the simulation 
   model, and the endpoints of the communication channels.

   Processes, InChannels, and OutChannels are all owned by
   some Entity.  Each Entity is aligned on some Timeline.

   There is a static Entity directory that maps Entity names
   (which can be declared when the Entity is created) to 
   the memory address of that object.  This supports construction
   of some global name space for Entities, useful for a number
   of things (i.e., directing communication from outside this
   system to the correct Entity.)   

   Each instantiation of an Entity also keeps a directory of
   InChannels it owns, supporting construction of a local
   name-space for these. One might use these like ports in 
   networking,  with well-known string names (i.e., TCP, UDP)
   bound to InChannels bound to Processes that handle the associated
   type of input.

   Each Entity provides a function named "init".  This is called
   by the S3F framework after it has built all the Entity 
   objects for the simulation.  Normally one expects the
   Entity constructors to create all the InChannels and OutChannels
   it owns, and then do the mapping between Enitites in their Entity::init
   methods.

   The notion of S3F Process is just a specially designated Entity
   method that can be caused to execute.   Entity method waitFor 
   is used to schedule execution of the Process named as argument,
   at a distance in the future given by another argument.  
 */
class Entity
{
public:

	/* *******************************************************
        PUBLIC INTERFACE TO CLASS ENTITY
     bool cancel(HandleCode h)
     todo: Cancel the waitFor that returned h as a handle. Return flags whether it was successful

	 *****************************************************************/
	/** An Entity is constructed always directed to a Timeline it aligns upon
	 *  The heavy lifting is done by a call to method BuildEntity
	 */
	Entity(Timeline* tl);

	/** destructor. No real destruction consider here, yet. */
	virtual ~Entity();

	/** Time on Entity's Timeline, either the time at which the current
	 *  (or most recent) event on the timeline was executed, or the end
	 *  of the last synchronization window.
	 */
	ltime_t 		now();

	/** Returns pointer to the Timeline on which this Entity is aligned */
	Timeline* 	alignment();

	/** Required of all Entities, is called in a phase where it is assumed
	 *  that S3F objects have been constructed but not connected to each other.
	 *  An Entity's init function will do the connection.
	 */
	virtual void 	init() {} ;

	/** The waitFor method schedules the activation of the named (or current) process at a distance in the future given by waitinterval.
	 *  The named process must be owned by an entity aligned to this timeline---checked by the Timeline::schedule method.
	 *  A HandleCode object is returned, it can be used to cancel the scheduled activation.
	 *  No priority is included, so Timeline::default_schedule_pri is used as a basis.
	 *
	 *  @param waitinterval wait interval (from now) that S3F will process the activation.
	 *  @param act activation (event); If no Activation is included, one is created with NULL as the protected pointer.
	 *  @param proc The process to execute. Must be owned by an Entity that is co-aligned with this Entity. If no process in included, the default process of this Entity is used.
	 */
	HandleCode 		waitFor(Process* proc, Activation act, ltime_t waitinterval);

	/** The waitFor method schedules the activation of the named (or current) process at a distance in the future given by waitinterval.
	 *  The named process must be owned by an entity aligned to this timeline---checked by the Timeline::schedule method.
	 *  A HandleCode object is returned, it can be used to cancel the scheduled activation.
	 *  No priority is included, so Timeline::default_schedule_pri is used as a basis.
	 *  No Activation is included, so one is created with NULL as the protected pointer.
	 *
	 *  @param waitinterval wait interval (from now) that S3F will process the activation.
	 *  @param proc The process to execute. Must be owned by an Entity that is co-aligned with this Entity. If no process in included, the default process of this Entity is used.
	 */
	HandleCode 		waitFor(Process* proc, ltime_t waitinterval);

	/** The waitFor method schedules the activation of the named (or current) process at a distance in the future given by waitinterval.
	 *  The named process must be owned by an entity aligned to this timeline---checked by the Timeline::schedule method.
	 *  A HandleCode object is returned, it can be used to cancel the scheduled activation.
	 *
	 *  @param waitinterval wait interval (from now) that S3F will process the activation.
	 *  @param act activation (event); If no Activation is included, one is created with NULL as the protected pointer.
	 *  @param proc The process to execute. Must be owned by an Entity that is co-aligned with this Entity. If no process in included, the default process of this Entity is used.
	 *  @param pri A tie-breaking priority. If no priority is included, Timeline::default_schedule_pri is used as a basis.
	 */
	HandleCode 		waitFor(Process* proc, Activation act, ltime_t waitinterval, unsigned int);

	/** The waitFor method schedules the activation of the named (or current) process at a distance in the future given by waitinterval.
	 *  The named process must be owned by an entity aligned to this timeline---checked by the Timeline::schedule method.
	 *  A HandleCode object is returned, it can be used to cancel the scheduled activation.
	 *  No Activation is included, so one is created with NULL as the protected pointer.
	 *
	 *  @param waitinterval wait interval (from now) that S3F will process the activation.
	 *  @param proc The process to execute. Must be owned by an Entity that is co-aligned with this Entity. If no process in included, the default process of this Entity is used.
	 *  @param pri A tie-breaking priority. If no priority is included, Timeline::default_schedule_pri is used as a basis.
	 */
	HandleCode		waitFor(Process* proc, ltime_t waitinterval, unsigned int);

	/** The waitFor method schedules the activation of the named (or current) process at a distance in the future given by waitinterval.
	 *  The named process must be owned by an entity aligned to this timeline---checked by the Timeline::schedule method.
	 *  A HandleCode object is returned, it can be used to cancel the scheduled activation.
	 *  No priority is included, so Timeline::default_schedule_pri is used as a basis.
	 *  No process in included, so the default process of this Entity is used.
	 *
	 *  @param waitinterval wait interval (from now) that S3F will process the activation.
	 *  @param act Activation (event); If no Activation is included, one is created with NULL as the protected pointer.
	 */
	HandleCode 		waitFor(Activation act, ltime_t waitinterval);

	/** The waitFor method schedules the activation of the named (or current) process at a distance in the future given by waitinterval.
	 *  The named process must be owned by an entity aligned to this timeline---checked by the Timeline::schedule method.
	 *  A HandleCode object is returned, it can be used to cancel the scheduled activation.
	 *  No priority is included, so Timeline::default_schedule_pri is used as a basis.
	 *  No process in included, so the default process of this Entity is used.
	 *  No Activation is included, so one is created with NULL as the protected pointer.
	 *
	 *  @param waitinterval wait interval (from now) that S3F will process the activation.
	 */
	HandleCode 		waitFor(ltime_t waitinterval);

	/** The waitFor method schedules the activation of the named (or current) process at a distance in the future given by waitinterval.
	 *  The named process must be owned by an entity aligned to this timeline---checked by the Timeline::schedule method.
	 *  A HandleCode object is returned, it can be used to cancel the scheduled activation.
	 *  No process in included, so the default process of this Entity is used.
	 *
	 *  @param waitinterval wait interval (from now) that S3F will process the activation.
	 *  @param act Activation (event); If no Activation is included, one is created with NULL as the protected pointer.
	 *  @param pri A tie-breaking priority. If no priority is included, Timeline::default_schedule_pri is used as a basis.
	 */
	HandleCode 		waitFor(Activation act, ltime_t waitinterval, unsigned int);

	/** The waitFor method schedules the activation of the named (or current) process at a distance in the future given by waitinterval.
	 *  The named process must be owned by an entity aligned to this timeline---checked by the Timeline::schedule method.
	 *  A HandleCode object is returned, it can be used to cancel the scheduled activation.
	 *  No process in included, so the default process of this Entity is used.
	 *  No activation is included, so one is created with NULL as the protected pointer.
	 *
	 *  @param waitinterval wait interval (from now) that S3F will process the activation.
	 *  @param pri A tie-breaking priority. If no priority is included, Timeline::default_schedule_pri is used as a basis.
	 */
	HandleCode		waitFor(ltime_t waitinterval, unsigned int);

	/** Unique identifier for Entity, index 0 to total number of Entities created. */
	unsigned int 	s3fid()				{ return __s3fid; }

	/* ***************************************************************
			Public Auxiliary Methods

  This implementation of S3F includes some useful but not essential
  extensions. 
	 *******************************************************************/

	/** An Entity can optionally provide a textual identifier that goes into
	 *  a directory so that a pointer to the Entity can be found by name.
	 *  The heavy lifting is done by a call to method BuildEntity.
	 */
	Entity(Timeline* tl, string);

	/** Return the text name ("" if none was given at initialization) */
	string 			getName()   	   	{ return __name;}

	/** Directory look up of the Entity pointer, given its text name */
	static Entity* 	getEntity(string);

	/** When an InChannel is create it can provide a text name, unique among
	 *  InChannels on the same Entity.  That name goes into a directory.
	 *  This method retrieves the name, returning NULL if no associated mapping
	 *  was found.
	 */
	InChannel* 		getInChannel(string);

	/** When a Process is create it can provide a text name, unique among
	 *  Processes on the same Entity.  That name goes into a directory.
	 *  This method retrieves the name, returning NULL if no associated mapping
	 *  was found.
	 */
	Process* 			getProcess(string);

	/** Conversion routines to translate from clock ticks to double precision. */
	double  			t2d(ltime_t t);

	/** Conversion routines to translate from double precision to clock ticks. */
	ltime_t 			d2t(double d);

	/** Conversion routines to translate from clock ticks to double precision.
	 *  @param t clock ticks input
	 *  @param s specify the units of the double precision input or output, in units of log_10(clock ticks per second).
	 *  So, for example, if a double represents milliseconds, the s value is 3
	 */
	double  			t2d(ltime_t t, int s);

	/** Conversion routines to translate from double precision to clock ticks.
	 *  @param d double input
	 *  @param s specify the units of the double precision input or output, in units of log_10(clock ticks per second).
	 *  So, for example, if a double represents milliseconds, the s value is 3
	 */
	ltime_t 			d2t(double d, int s);


	/* ****************************************************************
          INTERFACE FOR ACCESS BY FRIEND CLASSES AND
          ENTITY METHODS
	 ***************************************************************/
protected:
	friend class Timeline;
	friend class Interface;
	friend class OutChannel;
	friend class InChannel;
	friend class Process;

	/**  The core of the Entity construction is done by BuildEntity.
	 *   Finish constructing the Entity.
	 *   <BR> -- save the Timeline pointer
	 *   <BR> -- build the tables used to support tick - double precision conversion
	 *   <BR> -- give the entity a unique name
	 *   <BR> -- put the entity onto its Timeline's list of entities.
	 */
	void 			BuildEntity(Timeline*);

	/** Among all outbound cross-timeline connections with OutChannels in this Entity,
	 *  find the smallest min-delay that is at least as large as the argument.
	 */
	ltime_t 		min_sync_cross_timeline_delay(ltime_t);

	/** Count the number of outbound cross-timeline connections with minimum
	 *  delay at least as large as thrs.
	 */
	unsigned int 	count_min_delay_crossings(ltime_t);

	/* *******************************************************************
  					INTERNAL VARIABLES
	 ******************************************************************/

	// state related to identity
	string __name;	          ///< text name given at initialization
	Timeline*  __timeline;      ///< pointer to Timeline on which this Entity is aligned
	unsigned int __s3fid;		  ///< Unique identity for Entity

	// state related to time unit conversion
	/** pow[i] = 10^{i-18}. Used in time scale transformation */
	static double __pow[36];
	/**  time-scale of clock.  Obtained from timeline when the entity is created.*/
	int __log_ticks_per_sec;


	// list of S3F objects owned by this entity
	vector<OutChannel*> __outchannel_list;    ///< list of outchannels owned by this entity
	vector<InChannel*>  __inchannel_list;     ///< list of inchannels owned by this entity
	vector<Process*>    __process_list;       ///< list of processes owned by this entity

	// directories of Processes and InChannels owned by this Entity
	map<string,Process*> ProcessName;
	map<string,InChannel*> InChannelName;    ///< directory of inchannels declared with string names

	/** mutex for access to Entity instances state */
	pthread_mutex_t entity_inst_mutex;

	// static state

	/** Entity text to Pointer Directory */
	static map<string,Entity*> EntityName;

	/** Mutex that protects access to Entity state */
	static pthread_mutex_t entity_class_mutex;

	/** counter of number of entities created */
	static unsigned int  __num_entities;

};

#endif /*__ENTITY_H__*/

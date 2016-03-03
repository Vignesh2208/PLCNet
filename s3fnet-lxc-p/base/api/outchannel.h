/**
 * \file outchannel.h
 * \brief Header file for the S3F OutChannel
 *
 * authors : David Nicol, Dong (Kevin) Jin
 */

#ifndef __OUTCHANNEL_H__
#define __OUTCHANNEL_H__

#ifndef __S3F_H__
#error "outchannel.h can only be included by s3f.h"
#endif

/**
 *  mappedChannel is a helper class that stores information
 *  about a mapping from this OutChannel to some InChannel,
 *  and also serves to store information about changes to a mapping.
 */
class mappedChannel {
	friend class OutChannel;
	friend class Timeline;

public:
	mappedChannel(OutChannel* oc, InChannel* ic, ltime_t t, int s);

	// getor functions for the mappedChannel state variables
	//
	inline InChannel* ic()                { return __ic; }
	inline OutChannel* oc()               { return __oc; }
	inline ltime_t transfer_delay()       { return __delay; }
	inline ltime_t min_write_delay()      { return __delay; }
	inline int select()                   { return __select; }
	inline bool xtimeline()               { return __xtimeline; }
	inline bool asynchronous()            { return __asynchronous; }

	inline void set_transfer_delay(ltime_t d) { __delay = d; }
	inline void set_min_write_delay(ltime_t d) { __delay = d; }
	inline void set_asynchronous(bool as)   { __asynchronous = as; }

private:
	OutChannel* __oc;                     ///< the output channel end of the mapto
	InChannel*  __ic;		             ///< the input channnel end of the mapto

	/**
	 * Either the OutChannel min_write_delay, or the mapto transfer delay, depending on use.
	 * Transfer delay when this structure either holds an established mapto,
	 * or when that delay is being changed. Can hold a min_write_delay when being changed.
	 */
	ltime_t     __delay;

	/**
	 * =0 means __delay is the transfer delay,
	 * =1 means it's a min-write delay.
	 * When stored to describe an implemented mapto __select is 0
	 */
	bool        __select;
	/** boolean indicating whether the InChannel and OutChannel are on the same Timeline. Set at construction. */
	bool        __xtimeline;
	/** boolean indicating whether this mapping crosses an asynchronous connection between timelienes */
	bool        __asynchronous;
};

typedef std::tr1::shared_ptr<mappedChannel> Mapping;

/**
 * An OutChannel is the the source end of a communication connection.
 * One establishes a connection by finding the address of an InChannel
 * (e.g. the InChannelName directory of the owning Entity might be searched
 * to find it), and call OutChannel method mapto in order to establish the
 * connection.  An OutChannel may declare multiple mapto's---when the
 * "write" method is called passing a message, that message will be delivered
 * to all the InChannels to which the OutChannel has mapped itself.
 *
 * A mapto declaration includes a delay parameter that is used in the
 * time of arrival calculation. On making a call to write, a parameter of the write is the
 * "write time" delay.  The time of arrival is the sum of the time at which the
 * write is made is the the write-time delay plus the delay included in the mapto.
 * At the time an OutChannel is created one may declare a lower bound on
 * any write-time increment that would be included. The bound helps in
 * establishing synchronization windows
 */
class OutChannel 
{
public:
	/* ******************************************************
        PUBLIC INTERFACE TO CLASS OUTCHANNEL
	 *****************************************************************************/

	/** constructor, require specification of the Entity that owns it */
	OutChannel(Entity*);
	/** constructor, require specification of the Entity that owns it. A minimum "per-write" delay can also be specified. */
	OutChannel(Entity*, ltime_t);
	/** destructor. */
	virtual ~OutChannel();
	/** Returns the identity of the Entity that owns this OutChannel */
	inline Entity* owner() { return __owner; };

	/**
	 *  Declare a connection from here to the named InChannel,
	 *  with a transfer delay equal to the argument delay.
	 *  The return value is a simulation time after which the mapping
	 *  is assured to have been implemented.  A negative value is returned
	 *  as an error code if already a mapping exists between this OutChannel
	 *  and the named InChannel and the delays are different.
	 *  The address of the InChannel must be known.  InChannels can be declared
	 *  with string names, whose scope is local to the entity that owns them.
	 *  Each entity has a directory of string-to-InChannel names that can
	 *  be consulted for these mappings.
	 */
	ltime_t mapto(InChannel*, ltime_t);

	/**
	 *  unmap undoes a mapping between this OutChannel and the named InChannel.
	 *  If found the unmapping is done immediately.
	 *  Any Activations still "in flight" through this mapping are still delivered.
	 *  Boolean return indicates whether the InChannel was found and removed.
	 */
	bool unmap(InChannel*);

	/** Return true if the named if a mapping exists to the named InChannel */
	bool is_mapped(InChannel *);

	/** Return a vector of all the inchannels currently mapped to this outchannel */
	vector<InChannel*> mapped();

	/**
	 * Return the  transfer delay from OutChannel to named InChannel.
	 * -1 if there is no connection.
	 */
	ltime_t transfer_delay(InChannel*);

	/**
	 * new_transfer_delay allows the user to change the transfer delay on the mapping.
	 * A simulation time after which the remapping is assured to be successful is returned
	 * if the InChannel is found, otherwise a negative value indicating an error.
	 */
	ltime_t new_transfer_delay(InChannel*,ltime_t);

	/**
	 * Return the  minimum transfer delay from OutChannel to any InChannel aligned to a different Timeline.
	 * Time returned does NOT include min_write_delay. Returns -1 if there is none.
	 */
	ltime_t min_xtransfer_delay();

	/** Return the promised lower bound on the per-write delay given in a write call. */
	ltime_t min_write_delay()   { return __min_write_delay; }

	/**
	 * Change the minimum write delay associated with the OutChannel.
	 * The simulation time after which the change is assured is returned.
	 */
	ltime_t new_min_write_delay(ltime_t);

	/**************************************************************
  bool write(Activation, ltime_t delay, unsigned int pri); 
  bool write(Activation, ltime_t delay); 
  bool write(Activation,  unsigned int pri); 
  bool write(Activation); 


  One form of write specifies a scheduling priority to be used as a 2nd key
  in the event-list of the target timeline.  If no priority is specified,
  the default is the priority code of the process activated by this write.
  Likewise, a per-write delay may or may not be included.  If it is not, zero is assumed.

	 ***********************************************************************************/

	/**
	 * Method write directs the passed event (Activation act) to be carried to all
	 * inchannels to which this outchannel is mapped. The written event is scheduled
	 * to arrive at its destination at the time equal to the current time plus the offered
	 * per-write delay plus the delay associated with the mapto connection.
	 *
	 * A write attempts to write a copy of Activation act to every InChannel targeted by
	 * a mapto call by this OutChannel. Any InChannel that is on this same Timeline will
	 * get its Activation.  Any write done where the per-write delay is at least as large
	 * as the minimum declared at construction will deliver the Activation to all the targets.
	 * However, we allow a per-write delay that is actually smaller than the promised minimum,
	 * provided that the overall delivery time still places the receipt in a future synchronization
	 * window. The upper edge of the current window is accessible through Timeline::horizon().
	 *
	 * A write that would try to place the Activation across Timelines and within the current window
	 * is not performed, but all legal deliveries  from the write will be delivered.  True is returned
	 * if all deliveries are made, False if one or more do not.  A model may test the return from write
	 * and has access to enough information to determine which deliveries were not made.
	 *
	 * @param act typedef for shared_ptr<Message>.  Message is the base class for a communication,
	 * shared_ptr templates a very capable smart pointer, that, in particular, alleviates the need for
	 * deleting Activations as they are past around.  In practice a modeler will create a derived class
	 * for Message and create an Activation for it. The Message base class carries an unsigned int type
	 * that can be used to encode the derived type of Message being carried, needed for casting at the destination.
	 *
	 * @param delay a per-write delay. If it is not included, zero is assumed.
	 *
	 * @param pri a scheduling priority to be used as a 2nd key in the event-list of the target timeline.
	 * If no priority is specified, the default is the priority code of the process activated by this write.
	 *
	 */
	bool write(Activation, ltime_t delay, unsigned int pri);

	/**
	 * Same as the write(Activation, ltime_t delay, unsigned int pri),
	 * except the default priority (priority code of the process activated by this write) is used.
	 */
	bool write(Activation, ltime_t delay);

	/**
	 * Same as the write(Activation, ltime_t delay, unsigned int pri),
	 * except the zero per-write delay is used.
	 */
	bool write(Activation,  unsigned int pri);

	/**
	 * Same as the write(Activation, ltime_t delay, unsigned int pri),
	 * except the default priority (priority code of the process activated by this write)
	 * and the zero per write delay are used.
	 */
	bool write(Activation);

	/**
	 * Return the least delay among all mappings whose minimum delay is at least as large as the argument.
	 * Thresholding used to support composite synchronization algorithm.
	 */
	ltime_t min_sync_cross_timeline_delay(ltime_t);

	Timeline* alignment()		{ return __alignment; }
	unsigned int s3fid()          { return __s3fid;   }

protected:
	/* *****************/
	/*    Internal    */
	/******************/

	/**
	 * given a string name for an InChannel, see whether its corresponding InChannel address is known.
	 * NULL is returned if that address is not yet known.
	 */
	InChannel* getInChannel(string);

	/** count the number of cross-timeline connections whose minimum complete delay is less than or equal to thrs */
	unsigned int count_min_delay_crossings(ltime_t);

	/* list of friendly classes for opening access */
	friend class Entity;
	friend class Process;
	friend class Interface;
	friend class InChannel;
	friend class Timeline;

	Entity* __owner;                    ///< owner entity
	ltime_t __min_write_delay;          ///< lower bound on per-write delay
	Timeline* __alignment;              ///< timeline of outchannel's owner
	unsigned int __s3fid;               ///< S3f identity
	static unsigned int __num_outchannels;     ///< for identifier
	static pthread_mutex_t outchannel_class_mutex;   ///< synchronize getting id
	vector<mappedChannel> __mapped_to;  ///< collection of mappings
};
#endif /* __OUTCHANNEL_H__ */

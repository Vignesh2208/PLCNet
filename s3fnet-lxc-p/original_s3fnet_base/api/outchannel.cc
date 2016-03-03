/**
 * \file outchannel.cc
 * \brief Source file for the S3F OutChannel
 *
 * authors : David Nicol, Dong (Kevin) Jin
 */

#include <s3f.h>
using namespace s3f;

/** constructor for helper class mappedChannel */
mappedChannel::mappedChannel(OutChannel* oc, InChannel* ic, ltime_t t, int s) :
    				  __oc(oc), __ic(ic), __delay(t),
    				  __select(s), __xtimeline(false)
{ if(ic) __xtimeline = (ic->alignment() != oc->alignment())?true:false; }


/**
 * An OutChannel constructor is told the identity of the Entity that owns it,
 * and optionally a minimum per-write delay.  That delay is a lower bound on
 * any ltime_t argument a write method call on this OutChannel will observe.
 *
 * The chief role of the constructor is to initialize its state variable
 * <BR> __owner           : owning entity
 * <BR> __min_write_delay : lower bound on per-write argument
 * <BR> __alignment       : the Timeline on which the owning Entity is aligned
 * <BR> __s3fid           : the unique OutChannel id
 *
 * In addition, the constructor adds this OutChannel to the OutChannel list held by its owner.
 */
OutChannel::OutChannel(Entity* theowner, ltime_t min_write_delay = 0) : 
				   __owner(theowner), __min_write_delay(min_write_delay), __alignment(theowner->alignment())
{
	pthread_mutex_lock(&outchannel_class_mutex);
	__s3fid = __num_outchannels++;
	pthread_mutex_unlock(&outchannel_class_mutex);

	// register outchannel with entity that owns it
	pthread_mutex_lock(&theowner->entity_inst_mutex);
	theowner->__outchannel_list.push_back(this);
	pthread_mutex_unlock(&theowner->entity_inst_mutex);
}

OutChannel::~OutChannel() {
	if( !s3fid() ) pthread_mutex_destroy(&outchannel_class_mutex);
}


/* ***************************************************************

  bool OutChannel::write(Activation act, ltime_t delay, unsigned int pri)
  bool OutChannel::write(Activation act, ltime_t delay)
  bool OutChannel::write(Activation act)
  bool OutChannel::write(Activation act, unsigned int pri)

  This method write directs the passed event (Activation act) 
  to be carried to all inchannels to which
  this outchannel is mapped.  The written event is scheduled
  to arrive at its destination at the time equal to the current time plus the offered 
  per-write delay plus the delay associated with the mapto connection.

  One form of write specifies a scheduling priority to be used as a 2nd key
  in the event-list of the target timeline.  If no priority is specified,
  the default is the priority code of the process activated by this write.
  Likewise, a per-write delay may or may not be included.  If it is not, zero is assumed.

  A write attempts to write a copy of Activation act to every InChannel targeted by
  a mapto call by this OutChannel. Any InChannel that is on this same
  Timeline will get its Activation.  Any write done where the 
  per-write delay is at least as large as the minimum declared at construction
  will deliver the Activation to all the targets.  However, we allow a per-write
  delay that is actually smaller than the promised minimum, provided that the
  overall delivery time still places the receipt in a future synchronization window.
  The upper edge of the current window is accessible through Timeline::horizon().
    A write that would try to place the Activation across Timelines and within the 
  current window is not performed, but all legal deliveries eminating from the write
  will be delivered.  True is returned if all deliveries are made, False if one or more
  do not.  A model may test the return from write and has access to enough information
  to determine which deliveries were not made.

  Activation is typedef for shared_ptr<Message>.  Message is the base class for
  a communication, shared_ptr templates a very capable smart pointer, that, in particular,
  alleviates the need for deleting Activations as they are past around.  In practice
  a modeler will create a derived class for Message and create an Activation for it.  
  The Message base class carries an unsigned int type that can be used to encode the
  derived type of Message being carried, needed for casting at the destination.

 **************************************************************/

bool OutChannel::write(Activation act, ltime_t write_delay, unsigned int pri) {

	bool allsent = true;
	bool onesent = false;

	// compute the portion of the delivery time that is known
	ltime_t delivery1 = owner()->now() + write_delay;
	ltime_t delivery2;

	// what is the synchronization horizon (i.e., time at which
	// all Timelines next synchronize)?
	ltime_t horizon = owner()->alignment()->horizon();

	// the list won't change as we loop through so this caches the "end of loop"
	// index
	unsigned int end = __mapped_to.size();

	// for every inchannel mapped to this outchannel,
	// schedule an activation on the timeline to which the
	// InChannel's owner is mapped
	//
	for(unsigned int i=0; i<end; i++) {

		// compute arrival time over this connection
		delivery2 = delivery1 + __mapped_to[i].transfer_delay() + __min_write_delay;

		// send it if either the inchannel is aligned with the outchannel,
		// the delivery time is at least as large as the timeline's event horizon,
		// or the communication occurs over an asynchronous channel
		//
		if( !__mapped_to[i].xtimeline() || horizon <= delivery2 || __mapped_to[i].asynchronous() )
		{
#ifndef SAFE_MSG_PTR
			if(onesent == false) //first packet, no need to duplicate
			{
				__owner->alignment()->schedule( __mapped_to[i].ic(), delivery2, user_pri(pri), act );
				onesent = true;
			}
			else
			{
				Activation act_dup = act->clone();
				__owner->alignment()->schedule( __mapped_to[i].ic(), delivery2, user_pri(pri), act_dup );
			}
#else
			__owner->alignment()->schedule( __mapped_to[i].ic(), delivery2, user_pri(pri), act );
#endif
		}
		else allsent = false;
	}

#ifndef SAFE_MSG_PTR
	//no packet is sent, delete it
	if(onesent == false)
	{
		printf("s3f outchannel write drop \n");
		act->erase_all();
	}
#endif

	return allsent;
}

bool OutChannel::write(Activation act, ltime_t delay) {
	return write(act, delay, Timeline::default_sched_pri);
}

bool OutChannel::write(Activation act) {
	return write(act, 0, Timeline::default_sched_pri);
}

bool OutChannel::write(Activation act, unsigned int pri) {
	return write(act, 0, pri);
}

/**
 * mapto connects this outchannel with the InChannel whose address is passed.
 * A per-mapping delay can be associated with each mapping. If the OutChannel
 * and target InChannel are on different Timelines
 * then at least one { OutChannel __min_write_delay, per-mapping delay}
 * must be non-zero, otherwise the mapping is not permitted (in which case a -1 is returned).
 *
 * A mapto to an InChannel that has already been mapped to, but with a different
 * mapping delay causes an error code -1 to be returned also.  However, if there
 * is already a mapping to that InChannel but with the same mapping delay,
 * no error is thrown and the current time is returned.
 *
 * A legitimate mapping is encoded in an instance of the mappedChannel class.
 * However, depending on context, the mapping might not immediately be implemented.
 * A mapping delay that would permit a write to arrive in the current synchronization
 * window will cause implementation of the mapto to be be delayed until the end of the
 * synchronization window. This is implemented by putting a description of the mapping
 * into a Timeline list __channels_to_map, these are dealt with at the end of the
 * synchronization window, and the upper edge of the synchronization window is reported
 * as the time when the mapto will be effected.
 */
ltime_t OutChannel::mapto(InChannel* ic, ltime_t transfer_delay ) {

	// return error code if this mapping can't be implemented
	if( (transfer_delay == 0 && min_write_delay() == 0 ) &&
			ic->owner()->alignment() != owner()->alignment() ) {
		return -1;
	}

	// search the __mapped_to vector for a match with the InChannel.  
	// If present and if the associated delay is different 
	// from mapping_delay, do nothing and pass back false
	//
	unsigned int end = __mapped_to.size();
	for(unsigned int i=0; i< end; i++) {
		if (__mapped_to[i].ic() == ic ) {

			// Error if the delays are different. Do nothing if they are
			// the same.
			//
			if( __mapped_to[i].transfer_delay() != transfer_delay ) return -1;
			else return owner()->now();
		}
	}

	// look for the requested mapping in the timeline's __channels_to_map vector
	end = alignment()->__channels_to_map.size();
	for(unsigned int i=0; i< end; i++) {
		if (alignment()->__channels_to_map[i]->ic() == ic  &&
				alignment()->__channels_to_map[i]->oc() == this) {
			// Error if the delays are different. Do nothing if they are
			// the same.
			//
			if( alignment()->__channels_to_map[i]->transfer_delay() != transfer_delay ) return -1;
			else return owner()->now();
		}
	}

	// create a data structure to hold the mapping description 
	mappedChannel mc(this, ic, transfer_delay, false);

	// if the timeline state is not RUNNING we can implement the mapto
	// immediately by storing it in the outchannel's __mapped_to array.
	// Otherwise store it in the timeline's __channels_to_map array
	//
	if( owner()->alignment()->get_state() != RUNNING) {
		__mapped_to.push_back(mc);

		// tell the timeline about a cross-timeline delay
		if ( mc.xtimeline() ) {
			owner()->alignment()->cross_timeline_delay( transfer_delay );
		}
		// report an immediate mapping made
		return owner()->now();

	}
	else {
		// can't do it now so buffer it up
		owner()->alignment()->__channels_to_map.push_back(
				new mappedChannel( this, ic, transfer_delay, false) );

		// channel mapping will be implemented at the end of the current barrier
		// window.  This is stored in variable __stop_at in the timeline
		//
		return owner()->alignment()->horizon();
	}
}

/**
 * we can undo a mapto by calling unmap.  If we find the mapping
 * in the OutChannel's list, then it is removed and true is returned.
 * If we don't find the mapping, we look in the timeline's list of buffered
 * maptos for later implementation, and if found there, remove it.
 * If not found at all, false is returned.
 *
 * When found, check whether the connection's delay (OutChannel __min_write_delay plus tranfer delay)
 * is identically __min_sync_cross_timeline_delay, in which case decrement the count of such connections.
 */
bool OutChannel::unmap(InChannel *ic) {

	// cache for efficiency
	unsigned int end = __mapped_to.size();
	for( unsigned int i=0; i<end; i++)
		if( __mapped_to[i].ic() == ic) {
			// found it.
			// if this happens to be the minimum delay cross-timeline
			// channel then decrement count of these and see if that triggers
			// a recomputation
			if( __mapped_to[i].xtimeline() &&
					min_write_delay() + __mapped_to[i].transfer_delay()
					== alignment()->get_min_sync_cross_timeline_delay() ) {
				if ( --alignment()->__channels_at_minimum == 0 ) {
					alignment()->__recompute_min_delay = true;
				}
			}

			// We can remove the mapping from the __mapped_to list
			// by moving the tail end of the list into its position,
			// and then pop the end of the list.
			//
			__mapped_to[i] = __mapped_to[end];
			__mapped_to.pop_back();
			return true;
		}
	// didn't find it in the main list.  Look in the buffered list

	end = alignment()->__channels_to_map.size();
	for( unsigned int i=0; i<end; i++)
		if( alignment()->__channels_to_map[i]->ic() == ic &&
				alignment()->__channels_to_map[i]->oc() == this ) {
			// found it, so just shorten the list
			// by copying the end element into this
			// position and pop the vector
			alignment()->__channels_to_map[i] = alignment()->__channels_to_map[end-1];
			alignment()->__channels_to_map.pop_back();
			return true;
		}
	// couldn't find it at all
	return false;
}

/* *************************************************************
  bool OutChannel::is_mapped(InChannel *ic)

  is_mapped is called to determine if the named InChannel is mapped
     to the OutChannel, "true" means "yes".
 ****************************************************************/
bool OutChannel::is_mapped(InChannel *ic) {

	// cache for efficiency
	int end = __mapped_to.size();

	// return true if any of the InChannel pointers saved match the input
	for(int i=0; i<end; i++)
		if( __mapped_to[i].ic() == ic) return true;
	return false;
}

/* *************************************************************
  vector<InChannel*> OutChannel::mapped()

  mapped creates a list of all the inchannel's current mapped to
  the outchannel
 ****************************************************************/
vector<InChannel*> OutChannel::mapped() {
	vector<InChannel*> ans;
	int end = __mapped_to.size();

	for(int i=0; i<end; i++) ans.push_back( __mapped_to[i].ic() );
	return ans;
}


/* ************************************************************
  ltime_t OutChannel::transfer_delay(InChannel* ic)  

   transfer_delay returns the transfer delay between OutChannel
   and InChannel, if a mapping exists.  It otherwise returns -1.
 ****************************************************************/

ltime_t OutChannel::transfer_delay(InChannel* ic) {

	// cached for efficiency
	int end = __mapped_to.size();

	// return delay if any of the InChannel pointers saved match the input
	for(int i=0; i<end; i++)
		if( __mapped_to[i].ic() == ic) return __mapped_to[i].transfer_delay();

	// none found so return -1
	return -1;
}

/* *************************************************************
  ltime_t OutChannel::min_xtransfer_delay()  

   min_transfer_delay returns the smallest transfer delay between OutChannel
   and any InChannel mapped to a different Timeline.  
   -1 is returned if there is no mapping to a cross-timeline InChannel.
 ****************************************************************/
ltime_t OutChannel::min_xtransfer_delay() {
	ltime_t ans = -1;

	// cached for efficiency
	int end = __mapped_to.size();

	for(int i=0; i<end; i++)
		if(  __mapped_to[i].xtimeline() ) {
			if (ans<0 || __mapped_to[i].transfer_delay() < ans )
				ans = __mapped_to[i].transfer_delay();
		}
	return ans;
}

/* *************************************************************
  ltime_t OutChannel::new_transfer_delay(InChannel* ic, ltime_t delay) 

   new_transfer_delay changes the transfer delay between the OutChannel
   and named InChannel, provided the mapping exists. If it does not exist,
   -1 is returned.  If it does exist, a time by which the change will be
   implemented is returned.
 ****************************************************************/
ltime_t OutChannel::new_transfer_delay(InChannel* ic, ltime_t delay) {

	// cache for efficiency
	int end = __mapped_to.size();

	Timeline* tl = alignment();

	// find the named InChannel
	for(int i=0; i< end; i++) {
		if( __mapped_to[i].ic() == ic)  {

			// Found it. If the new minimum is equal to the old one there
			//  is nothing to do, just return the current time.
			//
			if( delay == __mapped_to[i].transfer_delay() ) return tl->now();

			// if the channel endpoints are co-aligned, just
			// make the change immediately and return the current time
			//
			if( !__mapped_to[i].xtimeline() ) {
				__mapped_to[i].set_transfer_delay(delay);
				return tl->now();
			}

			// If the new delay is larger than the old we can implement it
			// knowing it does not affect synchronization
			//
			if( __mapped_to[i].transfer_delay() < delay ) {
				__mapped_to[i].set_transfer_delay(delay);
				return tl->now();
			}

			// The delay got smaller. compute the new minimum transfer time under this change
			ltime_t xtransfer = min_write_delay() + delay;

			// if the new minimum is equal to the timeline's
			// minimum channel delay then increment the number
			// of channels at the minimum
			//
			if( xtransfer == tl->get_min_sync_cross_timeline_delay() ) {
				tl->__channels_at_minimum++;
				__mapped_to[i].set_transfer_delay(delay);
				return tl->now();
			}

			// if the new minimum is less than the timeline's
			// minimum channel delay it may still be possible to implement,
			//  if
			//    -- the timeline isn't currently running
			//    -- we've advanced far enough into the synchronization window
			//        that we can implement the change without threatening
			//        the synchronization
			//
			if( xtransfer < tl->get_min_sync_cross_timeline_delay() &&
					((tl->get_state() != RUNNING) ||
							( tl->horizon() <=  tl->now()+xtransfer ))) {
				__mapped_to[i].set_transfer_delay(delay);
				return tl->now();
			}

			// necessary to delay implementation and recompute the minimum cross timeline delay
			tl->__recompute_min_delay = true;

			// Create a mappedChannel structure and place it in the
			// timeline's __delays_to_change vector
			//
			tl->__delays_to_change.push_back(
					new mappedChannel( this, ic, delay, false) );
			break;
		}
	}
	return tl->horizon();
}

/* ****************************************************************
 ltime_t OutChannel::new_min_write_delay()

 Change the minimum per write delay associated with the OutChannel.
 Return the simulation time by which the change will have been implemented
 ***************************************************************/
ltime_t OutChannel::new_min_write_delay(ltime_t mwd) {

	// if the new one is larger than the old we can implement immediately
	if( __min_write_delay <= mwd ) {
		__min_write_delay = mwd;
		return owner()->now();
	}

	// if the sum of the new minimum and the minimum transfer time
	// is at least as large as the event horizon, we can implement immediately
	//
	if ( alignment()->horizon() <= mwd  + min_xtransfer_delay() ) {
		__min_write_delay = mwd;
		return owner()->now();
	}

	// has to wait until the end of the window. Put request in Timeline's
	// __delays_to_change queue.   Changes processed in FCFS order, so
	// in both transfer changes and min_write_delay changes only the last
	// change made in a window "sticks"
	//
	alignment()->__delays_to_change.push_back(
			new mappedChannel( this, NULL, mwd, true) );

	// will have taken place by the time of the event horizon
	return alignment()->horizon();
}

/* ****************************************************************
 ltime_t OutChannel::min_sync_cross_timeline_delay( ltime_t thrs)  

 among all cross timeline mappings attached to this outchannel, find the minimum
 sum of OutChannel's min_write_delay plus cross timeline transfer delay
 such that that sum is at least thrs.  Thresholding used by composite
 synchronization algorithm.

 -1 returned if there is no such mapping
 ***************************************************************/
ltime_t OutChannel::min_sync_cross_timeline_delay( ltime_t thrs) {

	int end = __mapped_to.size();
	if( end == 0 ) return -1;
	ltime_t ans = -1;

	// A more direct test in the loop below is thrs < min_write_delay()+delay
	//  but that involves an addition every time we can avoid by subtracting
	// min_write_delay() from thrs, and test thrs < delay
	//	
	thrs -= min_write_delay();

	ltime_t  delay; 
	for(int i=0; i< end; i++) {
		delay = __mapped_to[i].transfer_delay();
		if( __mapped_to[i].xtimeline()
				&& thrs <= delay && ( (delay<ans) || (ans<0) ) )
			ans=delay;
	}

	// return -1 if no xtimeline channels observed
	if(ans == -1) return -1;

	// found at least one xtimeline connection, so ans was changed
	return ans + min_write_delay();
}

/* ****************************************************************
 unsigned int OutChannel::count_min_delay_crossings(ltime_t thrs) 

 count the number of cross-timeline connections whose minimum
 complete delay is less than or equal to thrs
 ***************************************************************/

unsigned int OutChannel::count_min_delay_crossings(ltime_t thrs) {

	unsigned int ans = 0;
	int end = __mapped_to.size()-1;
	if( end == 0 ) return 0;
	ltime_t  delay;

	for(int i=0; i<= end; i++) {
		delay = __mapped_to[i].transfer_delay() + min_write_delay();
		if( __mapped_to[i].xtimeline() && delay <= thrs) ans++;
	}
	return ans;
}

pthread_mutex_t OutChannel::outchannel_class_mutex;
unsigned int OutChannel::__num_outchannels = 0;

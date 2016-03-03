/**
 * \file pq.h
 * \brief base classes for priority queues
 */

#ifndef __PQ_H__
#define __PQ_H__

#ifndef __S3F_H__
#error "pq.h can only be included by s3f.h"
#endif


/**
 * The interface to pq is _almost_ identical to that of the STL
 * priority queue.  The singular difference is a method
 * that returns a vector of _all_ events with the same priority
 */
class pq {
 public:
  pq() {}
  virtual ~pq() {}
  virtual bool empty()				= 0;
  virtual int size()				= 0;
  virtual EventPtr top()			= 0;
  virtual void push(EventPtr)			= 0;
  virtual void pop()				= 0;
};



#endif /*__PQ_H__*/

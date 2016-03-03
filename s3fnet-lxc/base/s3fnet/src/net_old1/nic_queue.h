/**
 * \file nic_queue.h
 * \brief Header file for the NicQueue class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __NIC_QUEUE_H__
#define __NIC_QUEUE_H__

#include "util/shstl.h"
#include "dml.h"

namespace s3f {
namespace s3fnet {

class ProtocolSession;
class LowestProtocolSession;

#define S3FNET_INIT_NIC_QUEUES 16

/**
 * \brief The control command types for the NIC queue.
 */
enum NicQueueCtrlCmd {
  /** Tear down flows with a list of destinations. */
  NIC_QUEUE_ROUTE_DOWN  = 0,
};

/**
 * \brief A base class for implementation of queues in an NIC.
 *
 * All derived queues must implement the enqueue member function,
 * which takes charge of buffering packets or flow events.
 */
class NicQueue {
 public:
  /** The constructor. */
  NicQueue(LowestProtocolSession* _nic); 

  /** The destructor.  */
  virtual ~NicQueue();

  /** Configure the nic queue. */
  virtual void config(s3f::dml::Configuration* cfg);

  /** Initialize the queue. */
  virtual void init();

  /** Insert a packet into the queue before it can be sent. This
      method must be orverridden in the derived class. */
  virtual void enqueue(Activation pkt) = 0;

  /** Control information; nothing implemented as default. */
  virtual int control(int ctrltyp, void* ctrlmsg, ProtocolSession* sess) 
    { return 0; }

  /** The wrapup method is called when the simulation finishes. */
  virtual void wrapup() {}
  
  /** Return the type of this queue. */
  virtual int type() = 0;

 protected:
  /** The lowest protocol session that this queue is associated
      with. */
  LowestProtocolSession* phy_sess;

  /** Bitrate of the link the NIC is attached to. */
  double bitrate;

  /** The buffer size of the queue. */
  long bufsize;

  /** The latency of the NIC. */
  ltime_t latency;

  /** The jitter range of this NIC. */
  float jitter_range;
};

/**
 * \brief A list of NIC queues.
 *
 * Class NicQueues implements a static queue management module. It
 * provides the most possible flexibility for designating a queue type
 * in the dml script.  The user can specify a queue type. If this type
 * is registered in NicQueues, the creation function for it will be
 * returned. Actually, the same method is applied elsewhere, e.g.,
 * dynamic protocol session creation.
 */
class NicQueues {
 public:
  /** The constructor. */
  NicQueues() {}

  /** The destructor. */
  virtual ~NicQueues() {}

  /** Register a new queue creation function and put it in a
      vector. This method is used by the S3FNET_REGISTER_QUEUE
      macro. */
  static int registerNicQueue(NicQueue* (*fct)(char*, LowestProtocolSession*));
  
  /** 
   * Create a queue instance. Go through all available factory methods
   * and then use the right one to create a new queue.
   */
  static NicQueue* newNicQueueInstance(char* queuename, LowestProtocolSession* lowsess);

  /** The vector used to keep registered queues and their factory
      method. */
  static S3FNET_POINTER_VECTOR* registered_queues;
};


/**
 * Each type of queue defined in the system must declare itself using
 * the following macro.
 */
#define S3FNET_REGISTER_QUEUE(classname, queuename)	\
  static NicQueue* s3fnet_new_queuetype_##classname	\
    (char* queueClassName, LowestProtocolSession* sess) \
    { if(!strcasecmp(queuename, queueClassName))	\
        return new classname(sess); else return 0; }    \
   int s3fnet_register_queuetype_##classname =		\
     NicQueues::registerNicQueue			\
     (s3fnet_new_queuetype_##classname)

}; // namespace s3fnet
}; // namespace s3f

#endif /*__NIC_QUEUE_H__*/

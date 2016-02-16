/**
 * \file droptail_queue.h
 * \brief Header file for the DroptailQueue class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __DROPTAIL_QUEUE_H__
#define __DROPTAIL_QUEUE_H__

#include "net/nic_queue.h"

namespace s3f {
namespace s3fnet {

#define DROPTAIL_QUEUE_TYPE 1
#define DROPTAIL_QUEUE_NAME "droptail"

/** 
 * \brief A FIFO queue used inside a network interface.
 *
 * The drop-tail queue maintains a buffer that holds up to a fixed
 * number of bits. Bits go out one by one at the NIC's bit-rate. If
 * attempting to enqueue a message whose size is larger than the
 * currently available free buffer space, the packet is dropped.  This
 * is accomplished by the following scheme. Each interface tracks its
 * "queueing delay," which accumulates as packets are written
 * out. Each packet contributes its transmission time (computed as the
 * size of the packet in bits divided by the bit rate of the NIC) to
 * the queueing delay of future packets.  This guarantees that the
 * interface will never exceed the local NIC's set bit rate .
 */
class DroptailQueue: public NicQueue { 
 public:
  /** The constructor.
   *  The lowest protocol session in the interface that contains the queue is passed as an argument.
   */
  DroptailQueue(LowestProtocolSession* lowsess);
  
  /** The destructor. */
  ~DroptailQueue() {}

  /** Initialize the queue. */
  virtual void init();

  /** Return the type of this queue. */
  virtual int type() { return DROPTAIL_QUEUE_TYPE; }

  /**
   * Enqueue the message at the tail of the buffer. If the NIC hasn't
   * yet used up all of its bandwidth, the message is sent after the
   * appropriate buffer delay. If the queue is full, the message will
   * be dropped.
   */
  void enqueue(Activation msg);
  
 protected:
  /**
   * Called by the enqueue method. The queueing delay is calculated
   * relative to the last recorded transmission. If we have moved
   * forward in time, we need to recalibrate the accumulated delay so
   * that it remains relative to the current time. We need to
   * recalibrate before every transmission. If the jitter range is
   * defined in the interface, this method also samples and returns a
   * jitter for sending the message.
   */
  ltime_t calibrate(int length);

 private:  
  /** The time of the last transmission. */
  ltime_t last_xmit_time;

  /** Queuing delay since the last transmission time. */
  ltime_t queue_delay;

  /** Maximum queuing delay is bounded by the queue length. */
  ltime_t max_queue_delay;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__DROPTAIL_QUEUE_H__*/

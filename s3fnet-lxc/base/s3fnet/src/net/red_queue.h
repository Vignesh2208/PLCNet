/**
 * \file red_queue.h
 * \brief Header file for the RedQueue class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __RED_QUEUE_H__
#define __RED_QUEUE_H__

#include "net/nic_queue.h"

namespace s3f {
namespace s3fnet {

#define RED_QUEUE_TYPE 1
#define RED_QUEUE_NAME "red"

/** 
 * \brief A simple RED queue used inside a network interface.
 *
 * The RED queue maintains a buffer that holds up to a fixed number of
 * bits. Bits go out one by one at the NIC's bit-rate. If attempting
 * to enqueue a message whose size is larger than the currently
 * available free buffer space, the packet is dropped. If there's
 * available buffer space, RED policy is enforced in terms of treating
 * newly arrived packets. A packet could be dropped according to a
 * probability calculated from the following function:
 *   - P(x)=0, if 0 <= x < qmin;
 *   - P(x)=(x-qmin)/(qmax_qmin)*pmax, if qmin <= x < qmax;
 *   - P(x)=(x-qmax)/(qcap-qmax)*(1-pmax)+pmax, if qmax <= x < qcap,
 *   - P(x)=1, otherwise
 * where x is the average queue length. The average queue length is
 * calculated at each packet arrival using an exponentially weighed
 * moving average (EWMA) method. That is, avgque =
 * (1-w)*avgque+w*queue, where w is a configurable parameter.
 */
class RedQueue: public NicQueue { 
 public:
  /** The constructor. The lowest protocol session in the interface
      that contains the queue is passed as an argument. */
  RedQueue(LowestProtocolSession* lowsess);
  
  /** The destructor. */
  ~RedQueue();

  /** Configure the nic queue. */
  virtual void config(s3f::dml::Configuration* cfg);

  /** Initialize the queue. */
  virtual void init();

  /** Return the type of this queue. */
  virtual int type() { return RED_QUEUE_TYPE; }

  /**
   * Enqueue the message at the tail of the buffer. If the NIC hasn't
   * yet used up all of its bandwidth, the msg is sent after the
   * appropriate buffer delay. If the queue is full or the message is
   * marked to be dropped according to RED policy, the message will
   * be dropped.
   */
  void enqueue(Activation msg);

 private:  
  /** Weight used by the AQM policy to calculate average queue length
      ON EACH PACKET ARRIVAL. */
  float weight; 

  /* Parameters to calculate packet drop probability based on the
     average queue length x(t):
       P(x)=0, if 0 <= x < qmin;
       P(x)=(x-qmin)/(qmax_qmin)*pmax, if qmin <= x < qmax;
       P(x)=(x-qmax)/(bufsize-qmax)*(1-pmax)+pmax, if x > qmax.
     Both qmin and qmax are in bits.
   */
  float qmin; ///< Parameter to calculate packet drop probability.
  float qmax; ///< Parameter to calculate packet drop probability.
  float qcap; ///< Parameter to calculate packet drop probability.
  float pmax; ///< Parameter to calculate packet drop probability.

  /** This RED option is used to avoid marking/dropping two packets in
      a row. */
  bool wait_opt;

  /** Mean packet size in bits. */
  float mean_pktsiz;

  /** The instantaneous queue size. */
  float queue;

  /** The average queue size. */
  float avgque;

  /** The loss probability calcuated from avg queue length. */
  float loss;

  /** False if the average queue length is crossing the min threshold
      for the first time. */
  bool crossing;

  /** The number of bits arrived between two consecutive packet drops
      (when the qmin threshold is crossed). */
  float interdrop;

  /** The time of the last queue update. */
  ltime_t last_update_time;

  /** The time when the queue will be emptied. */
  ltime_t vacate_time;

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
  ltime_t calibrate(float length);
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__RED_QUEUE_H__*/

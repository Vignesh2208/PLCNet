/**
 * \file simple_phy.h
 * \brief Header file for the SimplePhy class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __SIMPLE_PHY_H__
#define __SIMPLE_PHY_H__

#include "os/base/lowest_protocol_session.h"
#include "s3f.h"

namespace s3f {
namespace s3fnet {

class NicQueue;

#define SIMPLE_PHY_PROTOCOL_CLASSNAME "S3F.OS.SimplePhy"

/**
 * \brief A simple PHY layer protocol.
 *
 * The layer below MAC in a NIC. Essentially, it bridges the
 * associated nic queue and the upper layers in the host. For example,
 * a packet from the upper layer will be turned in to the nic queue to
 * calculate the queueing delay. In addition, a received packet will
 * be submitted to the upper layer through this protocol layer. This
 * layer derives from the LowestProtocolSession class.
 */
class SimplePhy : public LowestProtocolSession {
 public:
  /** The constructor. */
  SimplePhy(ProtocolGraph* graph);

  /** The destructor. */
  virtual ~SimplePhy(); 
  
  /**
   * Configurate this protocol layer. The configuration includes
   * reading buffer size, bandwidth, and latency, as well as the nic
   * queue attribute (such as the type of nic queue one uses in this
   * network interface.
   */
  virtual void config(s3f::dml::Configuration* cfg);

  /** Initialize the protocol layer. */
  virtual void init();

  /** Return the protocol number. */
  virtual int getProtocolNumber() { return S3FNET_PROTOCOL_TYPE_SIMPLE_PHY; }

  /** Handle control messages. */
  virtual int control(int ctrltyp, void* ctrlmsg, ProtocolSession* sess);
  
  /** Send out a packet. */
  void sendPacket(Activation pkt, ltime_t delay);

  /** Receive a packet. */
  void receivePacket(Activation pkt);

  double getBitrate() { return bitrate; }

 private:
  /**
   * A packet to be sent is pushed down from the protocol stack from
   * the upper layer. Here, msg must be NULL and the extinfo parameter
   * carries the packet.
   */
  virtual int push(Activation pkt, ProtocolSession* hi_sess, void* extinfo, size_t extinfo_size = 0);

 protected:
  /** The physical bandwidth of the link this NIC is attached to. */
  double bitrate;

  /** The buffer size of this NIC. */
  long bufsize;

  /** This is the latency of the NIC itself. */
  ltime_t latency;
  
  /** The jitter range of the NIC. It should be within [0,1]. */
  float jitter_range;

  /** The nic queue for outgoing packets. */
  NicQueue* buffer;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__SIMPLE_PHY_H__*/

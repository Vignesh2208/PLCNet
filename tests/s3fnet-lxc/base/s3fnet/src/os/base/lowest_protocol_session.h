/**
 * \file lowest_protocol_session.h
 * \brief Header file for the LowestProtocolSession class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __LOWEST_PROTOCOL_SESSION_H__
#define __LOWEST_PROTOCOL_SESSION_H__

#include "os/base/nic_protocol_session.h"

namespace s3f {
namespace s3fnet {

/**
 * \brief The lowest protocol session in the protocol stack.
 *
 * The LowestProtocolSession class is the base class of the lowest
 * protocol session in an interface, thus also the lowest in a
 * protocol graph.  It provides methods to receive packet frame from
 * the neighboring hosts.
 */
class LowestProtocolSession : public NicProtocolSession {
 public:
  /** The constructor. */
  LowestProtocolSession(ProtocolGraph* graph) : NicProtocolSession(graph) {}

  /**
   * Handle the control messages. This control method only recognizes
   * PSESS_CTRL_SESSION_IS_LOWEST control message (called from from
   * the protocol graph).
   */
  virtual int control(int ctrltyp, void* ctrlmsg, ProtocolSession* sess);

  /**
   * Derived class should use this method to send packet frames out
   * from the host to its peer(s).  The protocol messages sent down to
   * this protocol session should be wrapped with a packet first
   * before being delivered.  A delay is provided as the second
   * argument, which models the time needed for physical device to
   * start transmitting the packet.
   */
  virtual void sendPacket(Activation pkt, ltime_t delay) = 0;

  /**
   * The method is called by the interface when a packet has
   * arrived. Derived class must overload this method to accept
   * arrival packet.
   */
  virtual void receivePacket(Activation pkt) = 0;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__LOWEST_PROTOCOL_SESSION_H__*/

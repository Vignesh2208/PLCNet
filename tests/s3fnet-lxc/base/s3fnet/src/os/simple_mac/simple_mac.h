/**
 * \file simple_mac.h
 * \brief Header file for the SimpleMac class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __SIMPLE_MAC_H__
#define __SIMPLE_MAC_H__

#include "os/base/nic_protocol_session.h"

namespace s3f {
namespace s3fnet {

#define SIMPLE_MAC_PROTOCOL_CLASSNAME "S3F.OS.SimpleMac"

/**
 * \brief A simple MAC layer protocol.
 *
 * The SimpleMac class here is simply a placeholder for a protocol
 * that provides medium access control. This is because for wired
 * simulation, we do not model things below IP. The class does simple
 * things for packet transmissions, such as checking the next hop
 * address.
 */
class SimpleMac: public NicProtocolSession {
 public:
  /** The constructor. */
  SimpleMac(ProtocolGraph* graph);

  /** The destructor. */
  virtual ~SimpleMac();
    
  /** Initializations. For the moment, we do nothing. */
  virtual void init();

  /**
   * Return the protocol number. This protocol number will be stored
   * as the index to the protocol graph.
   */
  virtual int getProtocolNumber() { return S3FNET_PROTOCOL_TYPE_SIMPLE_MAC; }

 private:
  /**
   * Push message down from the upper layer. msg is the protocol
   * message the upper layer. The extinfo is a pointer to the
   * IPOptionToBelow object. It contains the routing entry for the
   * destination or a source routing vector; The IPOptionToBelow
   * object also has a pointer to the forwarded packet when the IP
   * layer decides to forward a received packet.
   */
  virtual int push(Activation msg, ProtocolSession* hi_sess, void* extinfo, size_t extinfo_size);
  
  /**
   * Whenever the associated physical layer receives a packet, it will
   * call this function.
   */
  virtual int pop(Activation msg, ProtocolSession* lo_sess, void* extinfo, size_t extinfo_size);

};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__SIMPLE_MAC_H__*/

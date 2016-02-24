/**
 * \file dummy_mac.h
 * \brief Header file for the DummyMac class.
 *
 * authors : Vignesh Babu
 */

#ifndef __DUMMY_MAC_H__
#define __DUMMY_MAC_H__

#include "os/base/nic_protocol_session.h"

namespace s3f {
namespace s3fnet {

#define DUMMY_MAC_PROTOCOL_CLASSNAME "S3F.OS.DummyMac"

/**
 * \brief A Dummy MAC layer protocol.
 */
class DummyMac: public NicProtocolSession {
 public:
  /** The constructor. */
  DummyMac(ProtocolGraph* graph);

  /** The destructor. */
  virtual ~DummyMac();
    
  /** Initializations. For the moment, we do nothing. */
  virtual void init();

  /**
   * Return the protocol number. This protocol number will be stored
   * as the index to the protocol graph.
   */
  virtual int getProtocolNumber() { return S3FNET_PROTOCOL_TYPE_DUMMY_MAC; }

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

#endif /*__DUMMY_MAC_H__*/

/**
 * \file dummy_mac_message.h
 * \brief Header file for the DummyMacMessage class.
 *
 * authors : Vignesh Babu
 */

#ifndef __DUMMY_MAC_HDR_H__
#define __DUMMY_MAC_HDR_H__

#include "os/base/protocol_message.h"
#include "s3fnet.h"

namespace s3f {
namespace s3fnet {

/**
 * \brief Protocol message for the Dummy MAC layer.
 */ 
class DummyMacMessage: public ProtocolMessage {
 public:
  /** The default constructor without any argument. */
  DummyMacMessage();
  
   /** The copy constructor. */
  DummyMacMessage(const DummyMacMessage&);

  /** Clone the protocol message (as required by the ProtocolMessage
      base class). */
  virtual ProtocolMessage* clone();
  
  /** Return the protocol type that the message represents. */
  int type() { return S3FNET_PROTOCOL_TYPE_DUMMY_MAC; }

  /**
   * This method returns the number of bytes that are needed to
   * pack this pseudo-mac header. This is for serialization.
   */
  virtual int packingSize()
  {
    return ProtocolMessage::packingSize();
  }
  
  /* The Dummy MAC header is not counted as part of the packet
     when we compute the bandwidth use and delay, etc. */
  virtual int realByteCount() { return 0; }
  
 protected:
  /** The destructor. */
  virtual ~DummyMacMessage();

};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__DUMMY_MAC_HDR_H__*/

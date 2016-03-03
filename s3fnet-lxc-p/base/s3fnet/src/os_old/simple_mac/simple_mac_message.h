/**
 * \file simple_mac_message.h
 * \brief Header file for the SimpleMacMessage class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __SIMPLE_MAC_HDR_H__
#define __SIMPLE_MAC_HDR_H__

#include "os/base/protocol_message.h"
#include "s3fnet.h"

namespace s3f {
namespace s3fnet {

class Mac48Address;

/**
 * \brief Protocol message for the simple MAC layer.
 *
 * Define the simple MAC header protocol message, which simply
 * contains the next hop address. We don't model MAC/PHY layers in
 * detail for wired network simulation. For now, a next hop address
 * will suffice. For more complicated link layer models, we would need
 * more details.
 */ 
class SimpleMacMessage: public ProtocolMessage {
 public:
  /** The default constructor without any argument. */
  SimpleMacMessage();
  
  /** The constructor with the MACADDR field. */
  SimpleMacMessage(MACADDR src, MACADDR dest);

  /** The constructor with the Mac48Address field. */
  SimpleMacMessage(Mac48Address* _src, Mac48Address* _dst);

  /** The copy constructor. */
  SimpleMacMessage(const SimpleMacMessage&);

  /** Clone the protocol message (as required by the ProtocolMessage
      base class). */
  virtual ProtocolMessage* clone();
  
  /** Return the protocol type that the message represents. */
  int type() { return S3FNET_PROTOCOL_TYPE_SIMPLE_MAC; }

  /**
   * This method returns the number of bytes that are needed to
   * pack this pseudo-mac header. This is for serialization.
   */
  virtual int packingSize()
  {
    return 2*sizeof(MACADDR) + ProtocolMessage::packingSize();
  }
  
  /* The simple MAC header is not counted as part of the packet
     when we compute the bandwidth use and delay, etc. */
  virtual int realByteCount() { return 0; }

  /** The source MAC address of this frame. */
  MACADDR src;

  /** The destination MAC address of this frame. */
  MACADDR dest;

  /** The source MAC address of this frame in IEEE 48-bit format. */
  Mac48Address* src48;

  /** The destination MAC address of this frame in IEEE 48-bit format */
  Mac48Address* dst48;

 protected:
  /** The destructor. */
  virtual ~SimpleMacMessage();

};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__SIMPLE_MAC_HDR_H__*/

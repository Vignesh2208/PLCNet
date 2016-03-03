/**
 * \file udp_message.h
 * \brief Header file for the UDPMessage class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __UDP_MESSAGE_H__
#define __UDP_MESSAGE_H__

#include "os/base/protocol_message.h"
#include "s3fnet.h"

namespace s3f {
namespace s3fnet {

#define  S3FNET_UDPHDR_LENGTH 8

/**
 * \brief UDP header.
 */ 
class UDPMessage : public ProtocolMessage {
 public:
  /** Source port number. */
  uint16 src_port;

  /** Destination port number. */
  uint16 dst_port;

 public:
  /** The default constructor (without any argument). */
  UDPMessage();

  /** The constructor with set fields. */
  UDPMessage(uint16 sport, uint16 dport);

  /** The copy constructor. */
  UDPMessage(const UDPMessage& msg);

  /** Clone the protocol message. */
  virtual ProtocolMessage* clone();

  /** Set the source and destination ports. */
  void setPorts(uint16 sport, uint16 dport);

  /** Return the protocol number that the message is representing. */
  int type() { return S3FNET_PROTOCOL_TYPE_UDP; }

  /**
   * This method returns the number of bytes that are needed to pack
   * this UDP header. This is for serialization.
   */
  virtual int packingSize() {
    return 2*sizeof(uint16)+ProtocolMessage::packingSize();
  }

  /** Pack this UDP header to buffer starting from the given offset. */
  //virtual void serialize(byte* buf, int& offset);
  
  /** Unpack the UDP header from buf from given offset. */
  //virtual void deserialize(byte* buf, int& offset);
  
  /**
   * Return the number of bytes that a UDP header really occupies in
   * the real world, even though we might not actually allocating the
   * buffer with this size in simulation.
   */
  virtual int realByteCount() { return S3FNET_UDPHDR_LENGTH; }

 protected:
  /** The destructor is protected from accidental invocation. */
  virtual ~UDPMessage();
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__UDP_MESSAGE_H__*/

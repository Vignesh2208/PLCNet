/**
 * \file ip_message.h
 * \brief Header file for the IPMessage class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __IP_MESSAGE_H__
#define __IP_MESSAGE_H__

#include "os/base/protocol_message.h"
#include "net/ip_prefix.h"
#include "s3fnet.h"

namespace s3f {
namespace s3fnet {

#define S3FNET_IPHDR_LENGTH 20
#define S3FNET_IP_MAX_DATA_SIZE (65536-S3FNET_IPHDR_LENGTH)

/**
 * \brief IP header.
 */ 
class IPMessage: public ProtocolMessage {
 public:
  /** The default constructor (without any argument). */
  IPMessage();

  /** The constructor with set fields. */
  IPMessage(IPADDR src, IPADDR dest, uint8 protono, uint8 live);

  /** The copy constructor. */
  IPMessage(const IPMessage& iphdr);

  /** Clone the protocol message. */
  virtual ProtocolMessage* clone();
  
  /** Return the protocol number that the message is representing. */
  virtual int type() { return S3FNET_PROTOCOL_TYPE_IPV4; }
  
  /**
   * This method returns the number of bytes that are needed to pack
   * this IP header. This is for serialization.
   */
  virtual int packingSize()
  {
    return 2*sizeof(IPADDR)+2*sizeof(uint8)+ProtocolMessage::packingSize();
  }
  
  /**
   * Return the number of bytes that a IP header really occupies in
   * the real world, even though we might not actually allocating the
   * buffer with this size in simulation.
   */

  virtual int realByteCount() { return S3FNET_IPHDR_LENGTH; }

  /** Source ip address. */
  IPADDR src_ip;
  
  /** Destination ip address. */
  IPADDR dst_ip;
  
  /** Protocol identifier for the protocol above IP. */
  uint8 protocol_no;
  
  /** Number of hops before the protocol message is dropped. */
  uint8 time_to_live;

 protected:
  /** The destructor is protected from accidental invocation. */
  virtual ~IPMessage();
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__IP_MESSAGE_H__*/

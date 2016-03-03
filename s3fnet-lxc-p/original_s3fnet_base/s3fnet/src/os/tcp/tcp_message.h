/**
 * \file tcp_message.h
 * \brief Header file for the TCPMessage class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __TCP_MESSAGE_H__
#define __TCP_MESSAGE_H__

#include "os/base/protocol_message.h"
#include "s3fnet.h"

namespace s3f {
namespace s3fnet {

#define S3FNET_TCPHDR_LENGTH 20
#define S3FNET_TCPHDR_LENGTH_IN_WORDS 5

/**
 * \brief TCP header.
 */ 
class TCPMessage: public ProtocolMessage {
 public:
  enum {
    TCP_FLAG_FIN = 0x01, /**< final segmenet from sender */
    TCP_FLAG_SYN = 0x02, /**< start of a new connection */
    TCP_FLAG_RST = 0x04, /**< connection to be reset */
    TCP_FLAG_PSH = 0x08, /**< push operation invoked */
    TCP_FLAG_ACK = 0x10, /**< acknowledgment field valid */
    TCP_FLAG_URG = 0x20  /**< urgent pointer field valid */
  };

  /** The source port number. */
  uint16 src_port;

  /** The destination port number. */
  uint16 dst_port;

  /** The sequence number. */
  uint32 seqno;
       
  /** The acknowledgment sequence number. */
  uint32 ackno;

  /** TCP header length in words. */
  byte length;

  /** Flags indicating packet type. */
  byte flags;

  /** Receive window size. */
  uint16 wsize;
   
  /** TCP options, e.g., in SACK. */
  byte* options;

  /* checksum and urgent fields are ignored for the moment. */

 public:
  /** The default constructor (without any argument). */
  TCPMessage();

  /** The constructor with set fields. */
  TCPMessage(uint16 sport, uint16 dport, uint32 seq, uint32 ack, byte flags, uint16 wsize,
		  byte length = S3FNET_TCPHDR_LENGTH_IN_WORDS, byte* options = NULL);

  /** The copy constructor. */
  TCPMessage(const TCPMessage&);

  /** Cloning the protocol message. */
  virtual ProtocolMessage* clone();
  
  /** Return the protocol number that the message is representing. */
  int type() { return S3FNET_PROTOCOL_TYPE_TCP; }

  /** Return the number of bytes occupied by options. */
  int options_length() { return (length-S3FNET_TCPHDR_LENGTH_IN_WORDS)*sizeof(uint32);}

  /**
   * This method returns the number of bytes that are needed to
   * pack this TCP header. This is for serialization.
   */
  virtual int packingSize()
  {
    return 3*sizeof(uint16)+2*sizeof(uint32)+2*sizeof(byte)+
      options_length()+ProtocolMessage::packingSize();
  }
  
  /**
   * Return the number of bytes that a TCP header really occupies in
   * the real world, even though we might not actually allocating the
   * buffer with this size in simulation.
   */
  virtual int realByteCount() { return length*sizeof(uint32); }

 protected:
  /** The destructor is protected from accidental invocation. */
  virtual ~TCPMessage();
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__TCP_MESSAGE_H__*/

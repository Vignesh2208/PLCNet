/**
 * \file tcp_message.cc
 * \brief Source file for the TCPMessage class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "os/tcp/tcp_message.h"

namespace s3f {
namespace s3fnet {

TCPMessage::TCPMessage() {}

TCPMessage::TCPMessage(uint16 sport, uint16 dport, uint32 seq, uint32 ack,
		       byte f, uint16 w, byte len, byte* opt) :
  src_port(sport), dst_port(dport), seqno(seq), ackno(ack),
  length(len), flags(f), wsize(w), options(opt) {}

TCPMessage::TCPMessage(const TCPMessage& msg) :
  ProtocolMessage(msg), // important!
  src_port(msg.src_port), dst_port(msg.dst_port), 
  seqno(msg.seqno), ackno(msg.ackno),
  length(msg.length), flags(msg.flags), wsize(msg.wsize)
{
  if(msg.options)
  {
    int optlen = options_length(); assert(optlen > 0);
    options = new byte[optlen]; assert(options);
    memcpy(options, msg.options, optlen);
  } else options = 0;    
}

ProtocolMessage* TCPMessage::clone() {
  return new TCPMessage(*this);
}

TCPMessage::~TCPMessage()
{
  if(options) delete[] options;
}

S3FNET_REGISTER_MESSAGE(TCPMessage, S3FNET_PROTOCOL_TYPE_TCP);

}; // namespace s3fnet
}; // namespace s3f

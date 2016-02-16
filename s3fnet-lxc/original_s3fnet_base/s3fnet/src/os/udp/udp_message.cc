/**
 * \file udp_message.cc
 * \brief Source file for the UDPMessage class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "os/udp/udp_message.h"

namespace s3f {
namespace s3fnet {

UDPMessage::UDPMessage() : src_port(0), dst_port(0) {}

UDPMessage::UDPMessage(uint16 sport, uint16 dport) :
  src_port(sport), dst_port(dport) {}

UDPMessage::UDPMessage(const UDPMessage& msg) :
  ProtocolMessage(msg), // important!
  src_port(msg.src_port), dst_port(msg.dst_port) {}

ProtocolMessage* UDPMessage::clone() {
  return new UDPMessage(*this);
}

UDPMessage::~UDPMessage() {}

void UDPMessage::setPorts(uint16 sport, uint16 dport) {
  src_port = sport;
  dst_port = dport;
}

/*void UDPMessage::serialize(byte* buf, int& offset)
{
  ProtocolMessage::serialize(buf, offset);

  s3f::ssf::ssf_compact::serialize(src_port, &buf[offset]);
  offset += sizeof(uint16);

  s3f::ssf::ssf_compact::serialize(dst_port, &buf[offset]);
  offset += sizeof(uint16);
}

void UDPMessage::deserialize(byte* buf, int& offset)
{
  ProtocolMessage::deserialize(buf, offset);
  
  s3f::ssf::ssf_compact::deserialize(src_port, &buf[offset]);
  offset += sizeof(uint16);

  s3f::ssf::ssf_compact::deserialize(dst_port, &buf[offset]);
  offset += sizeof(uint16);
}*/

S3FNET_REGISTER_MESSAGE(UDPMessage, S3FNET_PROTOCOL_TYPE_UDP);

}; // namespace s3fnet
}; // namespace s3f

/**
 * \file ip_message.cc
 * \brief Source file for the IPMessage class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "os/ipv4/ip_message.h"
#include "util/errhandle.h"

namespace s3f {
namespace s3fnet {

IPMessage::IPMessage() : src_ip(IPADDR_INVALID), dst_ip(IPADDR_INVALID), protocol_no(0), time_to_live(0)
{}

IPMessage::IPMessage(IPADDR src, IPADDR dest, uint8 protono, uint8 live) :
  src_ip(src), dst_ip(dest), protocol_no(protono), time_to_live(live)
{}

IPMessage::IPMessage(const IPMessage& iph) :
  ProtocolMessage(iph), // important!!!
  src_ip(iph.src_ip), dst_ip(iph.dst_ip),
  protocol_no(iph.protocol_no), time_to_live(iph.time_to_live)
{}

ProtocolMessage* IPMessage::clone()
{
  printf("IPMessage cloned()\n");
  return new IPMessage(*this);
}

IPMessage::~IPMessage(){}

S3FNET_REGISTER_MESSAGE(IPMessage, S3FNET_PROTOCOL_TYPE_IPV4);

}; // namespace s3fnet
}; // namespace s3f

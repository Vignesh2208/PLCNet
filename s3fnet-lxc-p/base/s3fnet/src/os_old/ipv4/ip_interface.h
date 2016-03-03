/**
 * \file ip_interface.h
 * \brief Header file for misc. classes for interfacing with IP session.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __IP_INTERFACE_H__
#define __IP_INTERFACE_H__

#include "net/ip_prefix.h"

namespace s3f {
namespace s3fnet {

class NetworkInterface;
class ProtocolMessage;

#define DEFAULT_IP_TIMETOLIVE 64

/**
 * \brief Parameters for protocol messages pushing down to IP.
 *
 * IP will expect parameters from protocols above it passed to it as a
 * pointer to an IPPushOption object.
 */
class IPPushOption {
 public:
  IPPushOption() : src_ip(IPADDR_INADDR_ANY), dst_ip(IPADDR_NEXTHOP_UNSPECIFIED),
  	  prot_id(S3FNET_PROTOCOL_TYPE_INVALID), ttl(0){}

  /** Constructor with initialization, with source ip. */
  IPPushOption(IPADDR _src_ip, IPADDR _dst_ip, uint8 _p_id, uint8 _ttl) : 
    src_ip(_src_ip), dst_ip(_dst_ip), prot_id(_p_id), ttl(_ttl){}

 public:
  /** The source IP address. */
  IPADDR src_ip;    

  /** The destination IP address. */
  IPADDR dst_ip;     

  /** The protocol ID. */
  uint8 prot_id;

  /** Time-to-live. */
  uint8 ttl;
};

/**
 * \brief Parameters for IP messages pushing down.
 *
 * IP will send parameters to the protocols below it with a pointer to an 
 * IPOptionToBelow object.
 */
class IPOptionToBelow {
 public:
  /** The constructor. */
  IPOptionToBelow() : routing_info(0), is_forward(false){}
  
  IPOptionToBelow(void* r, bool isForward = false, bool hop_routing = true) :
	  routing_info(r), is_forward(isForward){}
  
 public:
  /** Routing information carried from IP layer. */
  void* routing_info;
  bool is_forward;
};

/**
 * \brief Parameters for IP messages popping up.
 *
 * IP will send parameters to the protocols above with a pointer to an
 * IPOptionToAbove object. Note that emulation session is implemented
 * as an intercept session in IP, which passes the entire IP
 * packet. Since this data structure is not used by emulation session,
 * we do not need to include fields, such as TOS, IDENT, and OFFSET.
 */
class IPOptionToAbove {
 public:
  /** Source ip address. */
  IPADDR src_ip;

  /** Destination ip address. */
  IPADDR dst_ip;

  /** time to live. */
  uint8 ttl;
};

/**
 * \brief Push request received from lower layers.
 *
 * IP can receive push request from lower protocol layers like in the
 * NIC. It looks like the protocol message in the request is pushed
 * down from the upper layer. At the same time, the request should
 * provide an IPPushOption pointer.
 */
class IPPushRequest {
 public:
  /** The constructor. */
  IPPushRequest(IPPushOption* op, ProtocolMessage* m) :
    options(op), msg(m) {}

 public:
  /** Points to the parameters for push operation. */
  IPPushOption* options;

  /** The protocol message. */
  ProtocolMessage* msg;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__IP_INTERFACE_H__*/

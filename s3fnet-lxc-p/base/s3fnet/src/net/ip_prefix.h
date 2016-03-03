/**
 * \file ip_prefix.h
 * \brief Header file for the IPPrefix class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __IPPREFIX_H__
#define __IPPREFIX_H__

#include <stdio.h>

namespace s3f {
namespace s3fnet {

/**
 * \typedef IPADDR
 * IP address should be a 32-bit unsigned integer.
 */
typedef unsigned int IPADDR;

/**
 * \def IPADDR_LENGTH
 * IP address length int bits (=32).
 */
#define IPADDR_LENGTH int(sizeof(IPADDR)*8)

/**
 * \def IPADDR_INVALID
 * Invalid IP address (0.0.0.0).
 */
#define IPADDR_INVALID ((IPADDR)0)

/**
 * \def IPADDR_ANYDEST
 * IP broadcast address (255.255.255.255).
 */
#define IPADDR_ANYDEST ((IPADDR)-1)

/**
 * \def IPADDR_LOOPBACK
 * Loopback IP address (127.0.0.1).
 */
#define IPADDR_LOOPBACK ((IPADDR)0x7f000001)

/**
 * Reserved IP addresses for internal use. In any IP address
 * assignment schemes, these IP addresses should never be assigned
 * to any network interface.
 */
enum {
  /**
   * When any interface has received a packet with this next hop
   * address, the other side will not check the next hop address to
   * decide whether to report to the upper layer. In other words, a
   * packet with this next hop address will always be popped up.
   */
  IPADDR_NEXTHOP_IGNORED     = 0xFFFFFFFF,
  
  /**
   * When the route information carries this special next hop
   * address, the next hop address will be set to the destination IP
   * address.
   */
  IPADDR_NEXTHOP_UNSPECIFIED = 0xFFFFFFFE,
  
  /**
   * When the upper layer wants to send a packet out, it may ignore
   * the source IP address. Instead, this IP address will be
   * used. The IP layer will substitute it with the IP address of
   * the outgoing network interface.
   */
  IPADDR_INADDR_ANY          = 0xFFFFFFFF
};

/**
 * \brief IP prefix, such as 10.10.0.0/16.
 *
 * IPv4 address class. It also includes some utility functions one can
 * use to process an IP address.
 */
class IPPrefix {
 public:
  /** The constructor. */
  IPPrefix(IPADDR ipaddr = 0, int iplen = IPADDR_LENGTH): 
    addr(ipaddr), len(iplen) {}

  /** Convert an IP prefix to a string. */
  static char* ip2txt(IPADDR ip, char* str);

  /** 
   * Convert an IP prefix to a string and store it in an internal
   * static buffer.  This buffer will be re-used everytime this method
   * is called.
   */
  static char* const ip2txt(IPADDR ip);

  /**
   * Map from a string to an IP prefix; the length of the prefix is
   * returned via the output argument.
   */
  static IPADDR txt2ip(const char* str, int* len = 0);

  /**
   * Map from a string to an IP address; the mask length is
   * ignored. Returns true if the conversion is successful.
   */
  static bool txt2ip(const char* str, IPADDR& addr);

  /** The assignment operator. */
  IPPrefix& operator=(const IPPrefix& rhs);

  /** Returns true if the given IP prefix is contained in the subnet
      specified by this prefix. */
  bool contains(IPPrefix& ip);

  /** Returns true if the given IP address is contained in the subnet
      specified by this prefix. */
  bool contains(IPADDR ipaddr);

  /** Display the IP prefix to an output file (stdout by default). */
  void display(FILE* fp = 0) const;

  /** Display the IP prefix into a buffer. */
  char* toString(char* buf = 0) const;

  /** Comparison operator. */
  friend bool operator==(const IPPrefix& ip1, const IPPrefix& ip2);

 public:
  /** The network address of the prefix. */
  IPADDR addr;

  /** The length of the mask. */
  int len;

 private:
  /** A static lookup table to calculate subnet masks. */
  static IPADDR masks[33];

  /** The buffer used to store the textual representation of this
      prefix for display. */
  static char dispbuf[50];
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__IPPREFIX_H__*/

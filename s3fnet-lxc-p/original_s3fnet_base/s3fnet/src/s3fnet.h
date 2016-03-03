/**
 * S3FNet is a high-performance network simulator based on S3F Scalable Simulation Framework.
 * S3FNet is developed with reference to the PRIME SSFNet
 * developed and maintained by Professor Jason Liu's research group at Florida
 * International University and the iSSFNet simulator developed and maintained by
 * Professor David M. Nicol's research group at the University of Illinois at
 * Urbana-Champaign.
 *
 * \file s3fnet.h
 *
 * \brief Header file of s3fnet.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __S3FNET_H__
#define __S3FNET_H__

#include "dml.h"
#include "s3f.h"

/**
 * \namespace s3f::s3fnet Network Simulator developed based on the S3F Scalable Simulation Framework
 */
namespace s3f {
namespace s3fnet {

typedef char		int8;	///< 8-bit character type
typedef unsigned char	uint8;	///< 8-bit unsigned character type
typedef unsigned char	byte;	///< 8-bit unsigned character type
typedef short		int16;	///< 16-bit integer type (short)
typedef unsigned short uint16;	///< 16-bit unsigned integer type (unsigned short)
typedef int		int32;	///< 32-bit integer type
typedef unsigned int	uint32;	///< 32-bit unsigned integer type
typedef long long int	int64;	///< 64-bit integer type
typedef unsigned long long int uint64;	///< 64-bit unsigned integer type
typedef unsigned int word;
typedef void general_data;
typedef unsigned long ulong;
typedef unsigned int uint;

// min, max, and abs operators
#define mymin(a, b) (((a) < (b)) ? (a) : (b))
#define mymax(a, b) (((a) > (b)) ? (a) : (b))
#define myabs(a)    (((a) > 0.0) ? (a) : -(a))

/*
 * Type definitions, used globally.
 */

/**
 * \var MACADDR
 * For convenience, we use a 64-bit unsigned long integer to denote the MAC
 * address and use simple type casting to mimic the function of ARP.
 */
typedef uint64 MACADDR;

/// Convert from MAC address to IP address.
#define ARP_MAC2IP(macaddr) IPADDR(macaddr)

/// Convert from IP address to MAC address.
#define ARP_IP2MAC(ipaddr) MACADDR(ipaddr)

class Net;

// some standard protocol session names...
/** PHY layer session name. */
#define PHY_PROTOCOL_NAME    "phy"
/** MAC layer session name. */
#define MAC_PROTOCOL_NAME    "mac"
/** Network layer session name. */
#define NET_PROTOCOL_NAME   "net"
#define IP_PROTOCOL_NAME   "ip"
/** TCP layer session name. */
#define TCP_PROTOCOL_NAME    "tcp"
/** UDP layer session name. */
#define UDP_PROTOCOL_NAME    "udp"
/** Socket layer session name. */
#define SOCKET_PROTOCOL_NAME "socket"
/** ICMP layer session name. */
#define ICMP_PROTOCOL_NAME   "icmp"
/** OSPF routing layer session name. */
#define OSPF_PROTOCOL_NAME   "ospf"
/** BGP routing layer session name. */
#define BGP_PROTOCOL_NAME    "bgp"
/** FTP layer session name. */
#define FTP_PROTOCOL_NAME    "ftp"
/** HTTP layer session name. */
#define HTTP_PROTOCOL_NAME   "http"
/** SNMP layer session name. */
#define SNMP_PROTOCOL_NAME   "snmp"
/** DUMMY protocol layer session name. */
#define DUMMY_PROTOCOL_NAME  "dummy"

/**
 * \enum S3FNetProtocolType
 *
 * Protocol type (or protocol number) is used both by protocol
 * sessions and protocol messages to uniquely identify different
 * protocol layers. The IP layer uses this number to distinguish
 * upper-layer protocols. All protocol implemented are listed here.
 */
enum S3FNetProtocolType {
  /** Reserved type, which should not be used by any protocol. */
  S3FNET_PROTOCOL_TYPE_INVALID = 0,

  /* Full implementation of real network protocols. */

  /** The Internet Control Message Protocol (ICMP) for ping. */
  S3FNET_PROTOCOL_TYPE_ICMP = 1,

  /** The Internet Protocol (IP) version 4. */
  S3FNET_PROTOCOL_TYPE_IPV4 = 4,

  /** The Transport Control Protocol (TCP). */
  S3FNET_PROTOCOL_TYPE_TCP =  6,

  /** The User Datagram Protocol (UDP). */
  S3FNET_PROTOCOL_TYPE_UDP =  17,

  /** The Open Shortest Path First (OSPF) protocol. */
  S3FNET_PROTOCOL_TYPE_OSPF = 89,
   
  /** The Socket protocol. */
  S3FNET_PROTOCOL_TYPE_SOCKET = 200,

  /** A simple physical layer protocol. */
  S3FNET_PROTOCOL_TYPE_SIMPLE_PHY = 201,

  /** A simple MAC layer protocol. */
  S3FNET_PROTOCOL_TYPE_SIMPLE_MAC = 202,

  /** This protocol type is used only by the DataMessage class (for
      an opaque payload). The protocol does not represent any
      protocols in the real world. */
  S3FNET_PROTOCOL_TYPE_OPAQUE_DATA = 203,

  /** A client-side protocol for testing the TCP model. */
  S3FNET_PROTOCOL_TYPE_TCPTEST_CLIENT = 221,

  /** A server-side protocol for testing the TCP model. */
  S3FNET_PROTOCOL_TYPE_TCPTEST_SERVER = 222,

  /** A client-side protocol for testing the TCP model with blocking socket. */
  S3FNET_PROTOCOL_TYPE_TCPTEST_BLOCKING_CLIENT = 223,

  /** A server-side protocol for testing the TCP model with blocking socket. */
  S3FNET_PROTOCOL_TYPE_TCPTEST_BLOCKING_SERVER = 224,

  /** A client-side protocol for testing the TCP model with non-blocking socket. */
  S3FNET_PROTOCOL_TYPE_TCPTEST_NON_BLOCKING_CLIENT = 225,

  /** A server-side protocol for testing the TCP model with non-blocking socket. */
  S3FNET_PROTOCOL_TYPE_TCPTEST_NON_BLOCKING_SERVER = 226,

  /** A client-side protocol for testing the UDP model. */
  S3FNET_PROTOCOL_TYPE_UDPTEST_CLIENT = 227,

  /** A server-side protocol for testing the UDP model. */
  S3FNET_PROTOCOL_TYPE_UDPTEST_SERVER = 228,

  /** A protocol for communicating with the database for sending and receiving commands */
  S3FNET_PROTOCOL_TYPE_COMMAND = 238,

  /** A dummy protocol. */
  S3FNET_PROTOCOL_TYPE_DUMMY = 254,
};

/**
 * \enum ProtocolSessionCtrlTypes
 *
 * General control types. User defined control types should use numbers
 * greater than 1000. The numbers below are reserved.
 */
enum ProtocolSessionCtrlTypes {
  /*
   * control commands for a generic protocol session
   */

  /**
   * Check if the protocol is at the bottom of the protocol stack. The
   * ctrlmsg argument in this case is a pointer to a boolean value (of
   * type bool), which is supplied by the caller for the callee to
   * return value. The ProtocolSession class implements the default by
   * returning the value false, since by default a protocol session is
   * not the lowest in the protocol stack. The LowestProtocolSession
   * class overrides this method and returns true for this control
   * type. All protocol sessions that consider themselves functioning
   * as the lowest protocol session (such as SimplePhy) should simply
   * derive from the LowestProtocolSession class so that this control
   * type is handled appropriately.
   */
  PSESS_CTRL_SESSION_IS_LOWEST	= 1,

  /**
   * Get the owner network interface if this protocol session belongs
   * to a network interface (such as SimpleMac and SimplePhy). A
   * pointer to an Interface object shall be returned through the
   * ctrlmsg argument to the control method. The ProtocolSession class
   * implements the default by returning an NULL, indicating a
   * protocol session by default is not in a network interface. The
   * NicProtocolSession class overrides the control method and returns
   * the owner network interface. All protocol sessions that consider
   * themselves functioning within a network interface (such as
   * SimpleMac and SimplePhy) should derive from the
   * NicProtocolSession class so that this control type is handled
   * appropriately.
   */
  PSESS_CTRL_SESSION_GET_INTERFACE = 2,

  /**
   * Set the protocol session above the current one. This control type
   * is applicable to a protocol session that belongs to a network
   * interface (such as SimpleMac and SimplePhy). In the
   * ProtocolSession class, this control type is simply ignored.  The
   * NicProtocolSession class overrides the control method and
   * establishes the connection among protocol sessions within the
   * network interface. All protocol sessions that consider themselves
   * functioning within a network interface (such as SimpleMac and
   * SimplePhy) should derive from the NicProtocolSession class so
   * that this control type is handled appropriately.
   */
  PSESS_CTRL_SESSION_SET_PARENT = 3,

  /**
   * Set the protocol session above the current one. This control type
   * is applicable to a protocol session that belongs to a network
   * interface (such as SimpleMac and SimplePhy). In the
   * ProtocolSession class, this control type is simply ignored.  The
   * NicProtocolSession class overrides the control method and
   * establishes the connection among protocol sessions within the
   * network interface. All protocol sessions that consider themselves
   * functioning within a network interface (such as SimpleMac and
   * SimplePhy) should derive from the NicProtocolSession class so
   * that this control type is handled appropriately.
   */
  PSESS_CTRL_SESSION_SET_CHILD	= 4,


  /*
   * control commands for the (simple) PHY layer
   */

  /**
   * Get the bitrate of the physical medium in bits per second. The
   * ctrlmsg argument to the control method should be a pointer to a
   * double floating point number. The control method always returns a
   * zero for this control type.
   */
  PHY_CTRL_GET_BITRATE		= 21,

  /**
   * Get the buffer size in bytes.  The ctrlmsg argument to the
   * control method should be a pointer to a long integer. The control
   * method always returns a zero for this control type.
   */
  PHY_CTRL_GET_BUFFER_SIZE	= 22,

  /**
   * Get the latency of the network interface in seconds. The ctrlmsg
   * argument to the control method should be a pointer to a floating
   * point number. The control method always returns a zero for this
   * control type.
   */
  PHY_CTRL_GET_LATENCY		= 23,

  /**
   * Get the jitter range of the network interface. The ctrlmsg
   * argument to the control method should be a pointer to a floating
   * point number. A jitter should be between 0 and 1: 0 means no
   * jitter (the default), and 1 means maximum jitter (as large as the
   * time to send the packet). The control method always returns a
   * zero for this control type.
   */
  PHY_CTRL_GET_JITTER_RANGE	= 24,

  /**
   * Get the network queue. The ctrlmsg argument to the control method
   * should be a pointer to a pointer of NicQueue type. The control
   * method always returns a zero for this control type.
   */
  PHY_CTRL_GET_QUEUE		= 25,

  /*
   * control commands for IP
   */

  /** Check whether a given ip address is acceptable by local ip. */
  IP_CTRL_VERIFY_TARGET_IP_ADDRESS = 40,

  /** Check whether a given ip address is local. */
  IP_CTRL_VERIFY_LOCAL_IP_ADDRESS = 41,

  /** Get the forwarding table on the host. */
  IP_CTRL_GET_FORWARDING_TABLE	= 42,

  /** Load forwarding table from DML. */
  IP_CTRL_LOAD_FORWARDING_TABLE	= 43,

  /** Insert a new route. */
  IP_CTRL_INSERT_ROUTE		= 44,

  /** Insert a new route. */
  IP_CTRL_DELETE_ROUTE		= 45,

  /** Insert a new dml route. */
  IP_CTRL_INSERT_DMLROUTE	= 46,

  /** Add an intercepting session to support packet capturing. */
  IP_CTRL_ADD_INTERCEPT_SESSION	= 47,

  /** Push a protocol message down through IP (same as through the
      pushdown() method). */
  IP_CTRL_PUSH_REQUEST		= 48,

  /** Inform FIB listener protocol sessions of routes being added. */
  IP_CTRL_ADD_FIB_LISTENER	= 49,

  /** Inform FIB listener protocol sessions of routes being added. */
  IP_CTRL_FIB_ADDROUTE		= 50,

  /** Inform FIB listener protocol sessions of routes being
      deleted. */
  IP_CTRL_FIB_DELROUTE		= 51,


  /*
   * control commands for UDP
   */

  UDP_CTRL_GET_MAX_DATAGRAM_SIZE = 71,


  /*
   * control commands for Socket session
   */

  SOCK_CTRL_CLEAR_SIGNAL	= 81,
  SOCK_CTRL_SET_SIGNAL		= 82,
  SOCK_CTRL_DETACH_SESSION	= 83,
  SOCK_CTRL_REINIT			= 84,
  SOCK_CTRL_GET_APP_SESSION	= 85,

  /*
   * dummy protocol control commands
   */
  DUMMY_CTRL_COMMAND1		= 1512,
  DUMMY_CTRL_COMMAND2		= 1513,
};

/**
 * \brief The S3FNet simulation interface class.
 *
 * The S3FNet network simulation interface class for interacting with the underlying S3F API.
 */
class SimInterface : public Interface {
 public:
   /** the constructor
    * @param tl the number of simulation threads to create (pthreads).
    * @param ltps the time scale of the clock ticks in terms of log base 10 clock ticks per second.
    */
   SimInterface(int tl, int ltps) : Interface(tl,ltps), topnet(0) {}

   /** the destructor */
   virtual ~SimInterface();

   /** to build the s3fnet simulation model based on the DML file */
   void BuildModel( s3f::dml::dmlConfig* cfg );

 protected:
   /** pointer to the topnet object */
   Net* topnet;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__S3FNET_H__*/

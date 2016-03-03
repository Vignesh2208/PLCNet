/**
 * \file ip_session.h
 * \brief Header file for the IPSession class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __IP_SESSION_H__
#define __IP_SESSION_H__

#include "os/base/protocol_session.h"
#include "net/ip_prefix.h"
#include "s3fnet.h"
#include "util/shstl.h"

namespace s3f {
namespace s3fnet {

#define IP_PROTOCOL_CLASSNAME "S3F.OS.IP"

class IPMessage;
class ForwardingTable;
class RouteInfo;
class DMLRouteInfo;

/**
 * \brief The Internet protocol session.
 */
class IPSession: public ProtocolSession {
 public:
  /**
   * \brief All possible return values of IP push operation.
   */ 
  enum IPPushReturn {
    /** The packet has been pushed down successfully. */
    IPPUSHRET_DOWN_DONE         = 0,

    /** The packet has been dealt with locally by popping up to an
	upper protocol successfully. */
    IPPUSHRET_TO_LOCAL_DONE     = 1,

    /** The packet cannot be dealt with locally by popping up to an
	upper protocol successfully. */
    IPPUSHRET_TO_LOCAL_FAIL     = 2,

    /** The packet cannot be pushed down due to no route to the
	destination being found. */
    IPPUSHRET_NO_ROUTE          = 3,

    /** The packet is dropped due to TTL limit reached. */
    IPPUSHRET_DROPPED_TTL_LIMIT = 4,
  };

  /**
   * \brief All possible return values of IP pop operation.
   */
  enum IPPopReturn {
    /** The packet has been popped up to an upper layer
	successfully. */
    IPPOPRET_UP_DONE = 0,

    /** The packet has been forwarded successfully. */
    IPPOPRET_FORWARD_DONE        = 1,

    /** The packet is dropped by the filter (i.e., the intercept
	session). */
    IPPOPRET_DROPPED_BY_FILTER   = 2,

    /** The packet is dropped due to no matching protocol on this
	host. */
    IPPOPRET_DROPPED_NO_PROTOCOL = 3,

    /** The packet is dropped due to no route found. */
    IPPOPRET_DROPPED_NO_ROUTE    = 4,

    /** The packet is dropped due to TTL limit reached. */
    IPPOPRET_DROPPED_TTL_LIMIT   = 5,
  };

 public:
  /** The constructor. */
  IPSession(ProtocolGraph* graph);
  
  /** The destructor. */
  virtual ~IPSession();
    
  /** Configure the IP session. */
  virtual void config(s3f::dml::Configuration *cfg);

  /**
   * Return the protocol number. This specifies the relationship
   * between this protocol and the other protocols defined in the same
   * protocol stack.
   */
  virtual int getProtocolNumber() { return S3FNET_PROTOCOL_TYPE_IPV4; }

  /**
   * Initialize the IP session. This function will be called after the
   * entire network is read in and configured and all the links are
   * connected, but before the simulation starts running.
   */
  virtual void init();

  /** 
   * The control method is used by the protocol session to receive
   * control message (or query information) from adjacent protocol
   * sessions both above and below. The protocol session invoking this
   * method will have a reference to itself in the third argument. The
   * control messages are distinguished by types. The message type is
   * an integer; its meaning is specified by the particular
   * protocol. The control message is of type void* and should be cast
   * to the appropriate types accordingly. Note that the caller is
   * responsible to reclaim the control message once the method
   * returns. The method returns an integer, indicating whether the
   * control message is successfully processed. Unless specified
   * otherwise, a zero means success. Nonzero means trouble and the
   * returned value can be used as the error number, the meaning of
   * which is specified by each particular protocol.  The default
   * behavior does nothing except returning zero.
   * <pre>
   * [ctrltyp = IP_CTRL_VERIFY_LOCAL_ADDRESS]:
   *    ctrlmsg should be a pointer to a 32-bit destination address.
   *    if the returned value is 1, it means that it is a local ip address;
   *    if the returned value is 0, it means that it is not a local ip address.
   *    NOTE THIS METHOD DOES NOT CONFIRM TO THE ORIGINAL DESIGN PHYLOSOPHY;
   *    THE RETURN VALUE INDICATES WHETHER THE ADDRESS IS LOCAL, RATHER
   *    THAN WHETHER THE CONTROL CALL IS SUCCESSFUL.
   *
   * [ctrltyp = IP_CTRL_GET_FORWARDING_TABLE]:
   *    ctrlmsg should be a pointer to a pointer to a forwarding table. The forwarding
   *    table of the host will be assigned to that point.
   *    0 is always returned.
   *
   * [ctrltype = IP_CTRL_LOAD_FORWARDING_TABLE]:
   *    ctrlmsg is a pointer to the configuration. It forces the IP to create a forwarding table and
   *    read in the entries from the the configuraion. 0 is always returned.
   *
   * [ctrltyp = IP_CTRL_INSERT_ROUTE]:
   *    ctrlmsg should be a pointer to a RouteInfo object. Insert the RouteInfo object
   *    to the forwarding table.
   *    0 is always returned.
   *
   * [ctrltyp = IP_CTRL_DELETE_ROUTE]:
   *    ctrlmsg should be a pointer to a RouteInfo object. Delete the RouteInfo object
   *    from the forwarding table.
   *    0 is always returned.
   *
   * [ctrltyp = IP_CTRL_INSERT_DMLROUTE]:
   *    ctrlmsg should be a pointer to a DMLRouteInfo object. Convert and insert the RouteInfo object
   *    to the forwarding table.
   *    0 is always returned.
   *
   * [ctrltype = IP_CTRL_ADD_INTERCEPT_SESSION]:
   *    the caller will be set as the intercept session; there can be at most
   *    only one session that intercepts all pop up IP packets.
   *    return 0 if succeeded; abort on error.
   *
   * [ctrltyp = IP_CTRL_PUSH_REQUEST]:
   *    ctrlmsg should be a pointer to an IPPushRequest object. The
   *    push method is called as if the message is pushed from the
   *    protocol session above IP.
   *
   * [ctrltyp = IP_CTRL_ADD_FIB_LISTENTER]: 
   *    set the caller to be the protocol session who wish to receive a
   *    notification upon changes to the forwarding table. 
   *    0 is always returned.
   * </pre>
   */
  virtual int control(int ctrltyp, void* ctrlmsg, ProtocolSession* sess);

  /* Safe alternative to atoi() if valid numbers could be 0 (which atoi() returns on error). */
  static inline int strtonum(const char* str);

 private:
  /** Send a message down the protocol stack to lower layer. **/
  virtual int push(Activation msg, ProtocolSession* hi_sess, void* extinfo = 0, size_t extinfo_size = 0);
  
  /** Receive data up the protocol stack to the upper layer. **/
  virtual int pop(Activation msg, ProtocolSession* lo_sess, void* extinfo = 0, size_t extinfo_size = 0);

 protected:
  /** Verify whether an ip address is acceptable by local ip or not. */
  bool verify_target_ip_address(IPADDR ipaddr);

  /** Verify whether an ip address is local or not. */
  bool verify_local_ip_address(IPADDR ipaddr);

  /** Load forwarding table. */
  void load_forwarding_table(s3f::dml::Configuration* cfg);

  /** Find parent by protocol number. */
  ProtocolSession* get_parent_by_protocol(uint32 protocol);

  /** Handle packets that are popped up. */
  int handle_pop_pkt(Activation ip_msg, ProtocolSession* lo_sess, void* extinfo=NULL, int extinfo_size=0);

  /** Handle packets that are popped up and targetted to this machine. */
  int handle_pop_local_pkt(Activation ip_msg, ProtocolSession* lo_sess);

 private:
  /** Forwarding table used to deliver packets. */
  ForwardingTable* forwarding_table;

  /** Whether to print a message when a packet has been dropped. */
  bool show_drop;

  /** Route specified in dml file. */
  S3FNET_VECTOR(RouteInfo*)* route_vec;
  S3FNET_VECTOR(DMLRouteInfo*)* dml_route_vec;
  S3FNET_VECTOR(ProtocolSession*)* listener_vec;

  /** session that intercept packet. */
  ProtocolSession* pop_intercept_sess;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__IP_SESSION_H__*/

/**
 * \file forwardingtable.h
 * \brief Header file for the ForwardingTable class.
 *
 * authors : Dong (Kevin) Jin, Lenny Winterrowd
 */

#ifndef __FORWARDINGTABLE_H__
#define __FORWARDINGTABLE_H__

#include "s3fnet.h"
#include "util/shstl.h"
#include "util/trie.h"
#include "net/ip_prefix.h"

namespace s3f {
namespace s3fnet {

class NetworkInterface;
class ProtocolSession;
class Host;

typedef S3FNET_SET(ProtocolSession*) S3FNET_FIB_PROTO_SET;

// return value for adding and removing a route
#define FT_ROUTE_SUCCESS	0
#define FT_ROUTE_OVERWRITTEN	1201
#define FT_ROUTE_NOT_REPLACED	1202
#define FT_ROUTE_NOT_FOUND	1203

/**
 * \brief A route entry of a forwarding table.
 */
class RouteInfo : public TrieData {
 public:
  /** How/who installed the RouteInfo into the forwarding table? */
  enum RoutingProtocol {
    UNSPEC = 0, ///< unspecified
    STATIC = 1, ///< this is a static route
    IGP    = 2, ///< this route was installed by an Interior Gateway Protocol
    EGP    = 3, ///< this route was installed by an Exterior Gateway Protocol
    BGP    = 4, ///< this route was installed by the Border Gateway Protocol
    OSPF   = 5, ///< this route was installed by the Open Shortest Path First protocol
    XORP   = 6, ///< this route was installed by XORP
    PAO    = 7, ///< installed by Policy-Aware On-demand routing mechanism
    REVERSE = 8, ///< installed using ip reverse route
  };
  static char* routingProtocol2Str[];
  
  /** The constructor. */
  RouteInfo(): next_hop(IPADDR_INVALID), nic(0),
    cost(1), protocol(STATIC), resolved(false) {}

  /** The destructor. */
  virtual ~RouteInfo() {}

  /** Validate this route info is valid by chekcing the next hop is connected with this interface.
   * Return NULL if not.
   */
  RouteInfo* resolve(Host* machine);

  /** Override the equal operator. */
  bool operator==(RouteInfo& rhs) { 
    return destination == rhs.destination &&
      next_hop == rhs.next_hop &&
      nic == rhs.nic &&
      cost == rhs.cost &&
      protocol == rhs.protocol &&
      resolved == rhs.resolved;
  }

  /** Override the inequal operator. */
  bool operator!=(RouteInfo& rhs) { return !(*this==rhs); }
  
  /** Override the assign operator. */
  RouteInfo& operator=(const RouteInfo& r) {
    destination = r.destination;
    next_hop    = r.next_hop;
    nic         = r.nic;
    cost        = r.cost;
    protocol    = r.protocol;
    resolved    = r.resolved;
    return *this;
  }

  /** Output the route to the give file stream. */
  void display(FILE* fp);

  /** Configure the route entry for the specified host. */
  void config(Host* h, s3f::dml::Configuration* cfg);

 protected:
  virtual int size() {
	return sizeof(RouteInfo);
  }

 public:  
  /** The destination IP prefix. */
  IPPrefix destination;

  /** The IP address of the next hop. */
  IPADDR next_hop;

  /** The network interface through which it gets to the next hop. */
  NetworkInterface* nic;

  /** The cost of this routing entry. */
  int cost;

  /** The protocol that maintains this routing entry. */
  int protocol;

  /** Indicate whether the resolve() method has been called. */
  bool resolved;
};

/**
 * \brief ROUTE and NHI_ROUTE attributes from DML.
 *
 * The ROUTE and NHI_ROUTE attributes will be eventually converted to
 * RouteInfo and inserted into the forwarding table.
 */
class DMLRouteInfo {
 public:
  /** The constructor. */
  DMLRouteInfo() : 
    dst_ip(false),dst_str(0), nexthop_str(0), iface_id(0), cost(1), 
    protocol(RouteInfo::STATIC) {}
  
  /** The destructor. */
  ~DMLRouteInfo() { 
    if(dst_str) delete[] dst_str; 
    if(nexthop_str) delete[] nexthop_str;
  }

  /** Read the NHI_ROUTE attribute from the DML file. */
  void config(s3f::dml::Configuration* cfg);
  
  /**
   * This method translates the nhi address read from the DML file to
   * the corresponding ip address. If successful, the method returns a
   * newly created RouteInfo object.
   */
  RouteInfo* resolve(Host* machine);

 public:
  /** Indicates whether dst_str is an IP or NHI */
  bool dst_ip;

  /** The character string of the destination NHI. */
  char* dst_str;

  /** The character string of the next hop NHI. */
  char* nexthop_str;

  /** The network interface id. */
  int iface_id;

  /** The cost of this routing entry. */
  int cost;

  /** The protocol that installs this routing entry. */
  int protocol;
};

/**
 * \brief Base class for any routing caches.
 *
 * Stores a limited subset of RouteInfo values for faster lookup
 * than in the Trie structure.
 *
 * This class is designed to allow varied cache implementations
 * regardless of the Trie implementation used at the cost of a
 * single additional memory access vs a static, non-configurable
 * cache as part of ForwardingTable's data.
 */
class RouteCache {
  public:
	enum RouteCacheType {
		SINGLE_ENTRY = 0,
		DIRECT_MAPPED = 1,
		ASSOCIATIVE = 2,
		NONE = 255
	};

	/* Return a route for the given ipdaddr if it exists.
     * Returns NULL (0) otherwise.
     * Should be designed to be as fast as possible.
     */
	virtual RouteInfo* lookup(IPADDR ipaddr) = 0;

	/* Adds the given route to the cache. */
	virtual void update(IPADDR ipaddr, RouteInfo* route) = 0;

	/* Invalidates all entries in the cache.
	 * Necessary for proper functionality in most of
	 * ForwardingTables's methods.
	 */
	virtual void invalidate() = 0;
};

/**
 * \brief The forwarding table.
 *
 * This class maps IP addresses (in the form a.b.c.d/p) into RouteInfo
 * structures that specify the interface and the next hop among
 * others. We need the next hop in order to be able to implement LAN
 * links. The forwarding table is implemented as a Trie so we can
 * quickly find the most specific prefix for any destination.
 *
 * Obviously each machine will have a forwarding table which is the
 * result of routing calculations done by a routing protocol. The
 * forwarding table is used by a network layer protocol, IP, to be
 * specific. Conceptually the ProtoGraph class owns the forwarding
 * table and the routing protocols (like OSPF and BGP) should be able
 * to access and change it accordingly.
 */
class ForwardingTable {
 public:
  /** The constructor. */
  ForwardingTable(ProtocolSession* ipsess, int trie_version = -1, int cache_version = -1);

  /** The destructor. */
  virtual ~ForwardingTable();

  /**
   * Add the preferred route to a given ip prefix. If there's already
   * an entry for this address and if replace is true, the old route
   * will be overwritten and the method returns
   * FT_ROUTE_OVERWRITTEN. Also, the old route will be
   * reclaimed. Otherwise, the method returns 0.
   */
  int addRoute(RouteInfo* route, bool replace = true);

  /**
   * Remove a route from the forwarding table to a given ip
   * prefix. The removed route will be deleted. The method returns 0
   * if the route has been found and reclaimed. Oherwise, the method
   * returns FT_ROUTE_NOT_FOUND.
   */
  int removeRoute(RouteInfo* route);

  /**
   * Find a route to a given ip address. The method returns NULL if no
   * such route exists.
   */
  RouteInfo* getRoute(IPADDR ipaddr);

  /** Prints out the forwarding table. */
  void dump(FILE* fp = NULL);

  /**
   * Invalidate all routes in forwarding table that were installed by
   * the given protocol. If the protocol is UNSPEC, all routes will be
   * removed.
   */
  void invalidateAllRoutes(int protocol);

  /** If there is a default route, the method returns it; otherwise,
      it returns NULL. */
  RouteInfo* getDefaultRoute();

  /** Add a listener protocol session, who wish to be notified of the
      changes to the forwarding table. */
  void addListener(ProtocolSession* sess);

 protected:
  /** The recursive helper to the dump method. */
  static void dump_helper(TrieNode* root, uint32 sofar, int n, FILE* fp);

 private:
  /** The routing cache. Configurable/extendable much like Trie. */
  RouteCache* mCache;

  /** The Trie in which routes are stored (ForwardingTable originally inherited from Trie) */
  Trie* mTrie;

  /** The IP session is the owner of the forwarding table. */
  ProtocolSession* ip_sess;

  /** List of protocol sessions who wish to be notified of FIB
      changes. The protocol sessions will be informed of the changes
      via calls to the control() method with the following types:
      IP_CTRL_FIB_ADDROUTE and IP_CTRL_FIB_DELROUTE. In both cases,
      the ctrlmsg argument should be a pointer to the corresponding
      RouteInfo object being added to or removed from the forwarding
      table. */
  S3FNET_FIB_PROTO_SET listeners;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__FORWARDINGTABLE_H__*/

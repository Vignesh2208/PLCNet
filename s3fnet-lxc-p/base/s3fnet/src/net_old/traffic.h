/**
 * \file traffic.h
 * \brief Header file for the Traffic class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __TRAFFIC_H__
#define __TRAFFIC_H__

#include "util/shstl.h"
#include "net/ip_prefix.h"
#include "net/nhi.h"

namespace s3f {
namespace s3fnet {

class TrafficServerData;
class TrafficPattern;
class Net;
class Host;

#define TRAFFIC_PATTERN              "pattern"
#define TRAFFIC_CLIENT               "client"
#define TRAFFIC_SERVERS              "servers"
#define TRAFFIC_SERVER_LIST          "list"
#define TRAFFIC_SERVER_SESSIONS      "sessions"
#define TRAFFIC_SERVER_PORT          "port"
#define TRAFFIC_SERVER_NHI           "nhi"
#define TRAFFIC_SERVER_NHI_RANGE     "nhi_range"

typedef S3FNET_VECTOR(TrafficServerData*) TRAFFIC_SERVERDATA_VECTOR;

/**
 * \brief Specification of traffic in DML.
 * 
 * This class manages the traffic specification found in the outermost
 * net attribute in the DML. The traffic specification maps the
 * clients to servers with which they should establish connections.
 * Application-layer client and server protocols should be able to
 * access this traffic information and act upon it accordingly.
 */
class Traffic /*: public DmlObject*/ {
 public:
  typedef S3FNET_LIST(TrafficPattern*) TRAFFIC_PATTERN_LIST;
  typedef TRAFFIC_PATTERN_LIST::iterator TRAFFIC_PATTERN_LIST_ITERATOR;

  /** The constructor. */
  Traffic(Net* net);

  /** The destructor. */
  virtual ~Traffic();

  /** Configure traffic from DML. */
  void config(s3f::dml::Configuration* cfg);

  /**
   * Initialize the traffic, primarily to resolve the nhi address in
   * the traffic patterns. This method is called after the entire
   * network has been read in and configured.
   */
  void init();

  /** Print out information about this traffic object. */
  virtual void display(int indent = 0);

  /**
   * This function is meant to be called by a client protocol. It will
   * check if there's a match between the protocol's machine and the
   * client specification in traffic patterns.  If so, information the
   * corresponding servers will be returned through the parameter as a
   * vector of TrafficServerData objects. If list is not NULL, only
   * servers that offering exactly the same type of service are
   * returned. It is important to note that TrafficServerData objects
   * belong to the system; the user shouldn't reclaim them.
   */
  bool getServers(Host* host, 
		  S3FNET_VECTOR(TrafficServerData*)& servers,
		  const char* list = NULL);
  
 protected:
  /**  Resolve the client nhi addresses in the traffic patterns. */
  void resolve_nhi();

  /** Reclaim the space occupied by traffic patterns. */
  void release_patterns();

 private:
  /** The top network that owns this traffic object. */
  Net* topnet;

  /** List of traffic patterns. */
  TRAFFIC_PATTERN_LIST* patterns;
};

/**
 * \brief A traffic pattern in the traffic specification.
 *
 * A traffic pattern is defined as a mapping from a client nhi to a
 * list of servers. Traffic patters are specified in DML.
 */
class TrafficPattern {
 public:
  /** The constructor. */
  TrafficPattern();

  /** The destructor. */
  virtual ~TrafficPattern();

  /** Configure the traffic pattern. */
  void config(s3f::dml::Configuration* cfg);

  /** Print out this traffic pattern. */
  void display(int indent = 0);

 protected:
  /** Deallocate the content of the servers vector. */
  void deallocate_servers();

  /** Read server data from DML. */
  void configure_servers(s3f::dml::Configuration* cfg);

 public:
  /** The nhi address of the client in this traffic pattern. */
  Nhi client;

  /** The list of servers in this traffic pattern. */
  TRAFFIC_SERVERDATA_VECTOR servers;
};

/**
 * \brief Contain information about a server.
 */
class TrafficServerData {
 public:
  /** The default constructor. */
  TrafficServerData(): nhi(NULL), port(0), sessions(1), list(0) {}
  
  /** The constructor with set fields. */
  TrafficServerData(Nhi* n, int p, char* l, int s) :
    nhi(n), port(p), sessions(s), list(l) {}

  /** The destructor. */
  virtual ~TrafficServerData() { 
    if(nhi) delete nhi; 
    if(list) delete[] list;
  }
  
#if 0
  /** Translate the server nhi address to ip address. */
  IPADDR nicnhi2ip(Community* community);

  /** Override the == operator. */
  bool operator==(TrafficServerData& rhs) { return (nhi==rhs.nhi); }
#endif

 public:
  /** The nhi address of the server. */
  Nhi* nhi;
  
  /** The port number of the server. */
  int port;

  /** This field indicates how many sessions the client will choose to
      connect with this server. */
  int sessions;

  /** This field is used to limit what types of servers a client will
      connect. */
  char* list;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__TRAFFIC_H__*/

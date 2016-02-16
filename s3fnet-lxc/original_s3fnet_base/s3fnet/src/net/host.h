/**
 * \file host.h
 * \brief Header file for the Host class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __HOST_H__
#define __HOST_H__

#include "util/shstl.h"
#include "net/dmlobj.h"
#include "net/net.h"
#include "os/base/protocol_graph.h"
#include "net/ip_prefix.h"

namespace s3f {
namespace s3fnet {

class NetworkInterface;
class Net;

typedef S3FNET_MAP(int,NetworkInterface*) S3FNET_HOST_IFACE_MAP;

/**
 * \brief A host or router or switch.
 *
 */
class Host : public Entity, public DmlObject, public ProtocolGraph {
  friend class Net;

 public:
  /** The constructor.
   * The parent DML object is the network containing the host.
   */
  Host(Timeline* tl, Net* net, long id);

  /** The destructor. */
  virtual ~Host();

  Process*     listen_proc;

  /** listen on all inChannels at NetworkInterfaces (binding)
   *  and send the arrived packet to the right network interface,
   *  one lisent_proc per host entity */
  void listen(Activation pkt);

  /** unique number per host
   *  Used for ensuring the S3F event processing order is always deterministic,
   *  Last element used for tie breaking in case that other factors for determining the priority are the same (e.g., event timestamp)
   *  refer to S3F Entity::waitFor() for more info  */
  long tie_breaking_seed;

  /**
   * The config method is used to configure the host from DML. It is
   * expected to be called first, right after the Host object is
   * created and before the init method is called. A DML configuration
   * should be passed as an argument to the method. Aside from
   * configuring the host, the method also calls the config method of
   * the base class (i.e., ProtocolGraph), and therefore configure the
   * protocol graph (i.e., a stack of protocol sessions) on this host.
   */
  virtual void config(s3f::dml::Configuration* cfg);

  /**
   * The init method is used to initialize the host once it has been
   * configured. The init method will initialize the interfaces and
   * the random seed of this host. The method also calls the init
   * method of base class (i.e., ProtocolGraph) and initializes all
   * protocol sessions in the protocol graph.
   */
  virtual void init();

  /** Return the network containing this host. */
  Net* inNet();

  /** Return the random number generator of this host. */
  Random::RNG* getRandom() {return rng;}

  /**
   * The method returns the random seed of this host.
   * If it's zero, the protocol sessions on this host will share the same random stream;
   * otherwise, each session (protocol) has its own random number generator.
   */
  int getHostSeed() { return host_seed; }

  /** Return true if this host is a router. */
  bool isRouter() { return is_router; }

  /** Return the network layer protocol (i.e., the IP protocol). There
      must be one and only one such protocol defined within each host
      or router. An error will be prompted if it is undefined. */
  ProtocolSession* getNetworkLayerProtocol();

  /**
   * Return network interface of the given id. NULL will be returned if it is
   * not found. Note that this method can only be called after the config phase.
   */
  NetworkInterface* getNetworkInterface(int id);

  /** Return the network interface of this host that has the given IP
      address. NULL if it does not exist. */
  NetworkInterface* getNetworkInterfaceByIP(IPADDR ipaddr);

  /** Return the IP address of the first network interface. */
  IPADDR getDefaultIP();

  /** Load forwarding table from DML. */
  void loadForwardingTable(s3f::dml::Configuration* cfg);

  /** Print out the name of this host. */
  virtual void display(int indent = 0);

  /** print out the current simulation time with thousand separator */
  char* getNowWithThousandSeparator();

  /** return the pointer of the top net */
  Net* getTopNet() {return inNet()->getTopNet();}

 protected:
  /**
   * The random number generator, per host. Each host is expected to
   * maintain a unique random stream, seeded initially as a function
   * of the host id.
   */
  Random::RNG* rng;

  /**
   * The seed of this host.
   * If the seed is 0, it means that all the protocols on the host share a single random stream.
   * Otherwise, each session (protocol) has its own random number generator.
   */
  int host_seed;

  /** Whether this machine is a router*/
  bool is_router;

  /** Whether this machine is a switch */
  bool is_switch;

  Nhi nhi_host;

  /** The list of network interfaces. */
  S3FNET_HOST_IFACE_MAP ifaces;

  /** Point to the network protocol session. */
  ProtocolSession* network_prot;
  
  /** Configure a network interface. */
  void config_network_interface(s3f::dml::Configuration* cfg, int id);

  /** This method is called to register the network protocol. There
      must be one and only one such protocol defined within each host
      or router. An error will be prompted if there are more than one
      network protocols defined */
  void set_network_layer_protocol(ProtocolSession* ip);

  /** This is the virtual method defined in ProtocolGraph. Here it
      returns a pointer to itself. */
  virtual Host* getHost() { return this; }

  char disp_now_buf[32];

};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__HOST_H__*/

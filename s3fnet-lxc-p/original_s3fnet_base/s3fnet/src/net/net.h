/**
 * \file net.h
 * \brief Header file for the Net class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __NET_H__
#define __NET_H__

#include "util/shstl.h"
#include "net/ip_prefix.h"
#include "net/dmlobj.h"
#include "s3fnet.h"

namespace s3f {
namespace s3fnet {

class Link;
class NetworkInterface;
class Traffic;
class NetAccessory;
class CidrBlock;
class Host;
class NameService;

//only TRAFFIC is in use
enum {
  NET_ACCESSORY_IS_AS_BOUNDARY	= 1,
  NET_ACCESSORY_AS_NUMBER	= 2,
  NET_ACCESSORY_AREA_NUMBER	= 3,
  NET_ACCESSORY_TRAFFIC		= 4,
  NET_ACCESSORY_QUEUE_MONITORS	= 5,
  NET_ACCESSORY_UNSPECIFIED	= 1000
};

// control message and types and responses
enum {
  NET_CTRL_IS_AS_BOUNDARY	= 1,
  NET_CTRL_GET_AS_NUMBER	= 2,
  NET_CTRL_GET_AREA_NUMBER	= 3,
  NET_CTRL_GET_TRAFFIC		= 4,
  NET_CTRL_GET_QUEUE_MONITORS	= 5,

  AS_NUMBER_UNSPECIFIED		= -1,
  AREA_NUMBER_UNSPECIFIED	= 0,
};

/**
 * \brief A network.
 *
 * The net class is the container for everything.  All DML objects
 * (including internal nets) must be contained in a net.  Conceptually
 * a net is just a collection of hosts and routers and other nets. The
 * concept of a net is not that important for protocols like TCP and
 * IP, but it's crucial for OSPF and BGP because a net can be an
 * autonomous system (AS) or an OSPF area.  This net class will start
 * the configuration of the network.  it will scan for hosts, routers,
 * links and other nets in the DML and create/link these entities.
 * nets also play a very important role in resolution of NHI
 * addresses.
 */
class Net: public DmlObject {
 public:
  /** The constructor used by the top net. */
  Net(SimInterface* sim_interface);

  /** The constructor used by nets other than the top net. */
  Net(SimInterface* sim_interface, Net* parent, long id);

  /** The destructor. */
  virtual ~Net();

  /** Configure this net from DML. */
  void config(s3f::dml::Configuration* cfg);

  /**
   * Initialize the network. This function is called after the entire
   * network has been read from the DML but before the simulation
   * actually starts.
   */
  void init();

  /** Displays the contents of this net.*/
  virtual void display(int indent = 0);

  /**
   * Resolves the given NHI address relative to this network.  Returns
   * NULL if no such object exists in this network.  Depending on the
   * type of NHI, we resolve net/host/interface addresses.  It is
   * important that the NHI must have determined its type before this
   * method can be called.
   */
  DmlObject* nhi2obj(Nhi* nhi);

  /**
   * Resolve the given interface NHI to an address.
   * If failed, it returns IPADDR_INVALID.
   */
  IPADDR nicnhi2ip(S3FNET_STRING nhi);
  
  /**
   * Resolve IPADDR to the corresponding nic nhi.
   * If failed, it returns null.
   */
  const char* ip2nicnhi(IPADDR addr);

  /** Get the IP prefix of this network. */
  IPPrefix getPrefix() { return ip_prefix; }

  /** Get the limits of an id-range */
  static void getIdrangeLimits(s3f::dml::Configuration* rcfg, int& low, int& high);
  
  /** Return all hosts in this level of net, including routers and switches.*/
  S3FNET_INT2PTR_MAP& getHosts() { return hosts; }

  /** Return all subnets of this level of net. */
  S3FNET_INT2PTR_MAP& getNets() { return nets; }

  /** Return all links in this level of net. */
  S3FNET_POINTER_VECTOR& getLinks() { return links; }

  /** Return global name resolution service. */
  NameService* getNameService();

  /** Return the network interface object by giving the NHI of the interface */
  NetworkInterface* nicnhi_to_obj(S3FNET_STRING nhi_str);

  /** Register local host with given name.
   *  This method should be called in the config phase of each connected interface.
   */
  void register_host(Host* host, const S3FNET_STRING& name);

  /**
   * Register local interface with the given IPADDR.
   * This method should be called in the config phase of each connected interface.
   */
  void register_interface(NetworkInterface* iface, IPADDR ip);

  /**
   * Ask some information about the current network
   * For some queries, if the current network does not know the
   * answer, it will forward the question to its parent network.
   *
   * [ctrltyp = NET_CTRL_GET_TRAFFIC] get the traffic manager from the top net;
   * for a sub-network the query will be forwarded to its parent network.
   * ctrlmsg should be a pointer to a pointer to a Traffic object.
   * The control method returns 1 if found; 0 otherwise.
   */
  int control(int ctrltyp, void* ctrlmsg);

  /** return pointer of the top net */
  Net* getTopNet() {return top_net;}

 protected:
  /** The simulation interface (only one) of the S3FNet */
  SimInterface* sim_iface;

  /** The timeline (logical process) which the net is aligned to */
  int alignment;

  /**
   * The extra options for a net.
   * Not all Net objects should own some attributes like as ASBoundary (whether this net is
   * at the boudary of an AS), AS number, etc.
   */
  NetAccessory* netacc;

  /** IP prefix of this net. */
  IPPrefix ip_prefix;

  /** List of hosts: a map from id to a pointer of the host. */
  S3FNET_INT2PTR_MAP hosts;

  /** List of sub-networks: a map from id to a pointer of the network. */ 
  S3FNET_INT2PTR_MAP nets;

  /** List of links. */
  S3FNET_POINTER_VECTOR links;

  /** Used by the config methods to refer the cidr block of the
      current network it is processing. */
  CidrBlock* curCidr;

  /** indicator on whether this net object is the top net */
  bool is_top_net;

  /* the top net */
  Net* top_net;

  /** The global name resolution service, created at topnet, passed to all subnets */
  NameService* namesvc;

  /** Traffic object (topnet only) */
  Traffic* traffic;

  /** Configure the top net. */
  void config_top_net(s3f::dml::Configuration* cfg);

  /** Configure a link. */
  void config_link(s3f::dml::Configuration* cfg);

  /** Configure a host. */
  void config_host(s3f::dml::Configuration* cfg, S3FNET_LONG_SET& uniqset, bool is_router=false);

  /** Configure a router. */
  void config_router(s3f::dml::Configuration* cfg, S3FNET_LONG_SET& uniqset);

  /** Configure a sub network. */
  void config_net(s3f::dml::Configuration* cfg, S3FNET_LONG_SET& uniqset);

  /** 
   * This function is a helper function for the configure
   * function. Once the configure method has read in all the hosts,
   * routers and links, this function will go through the links and
   * connect the interfaces.
   */
  void connect_links();  

  /**
   * Process configurations that can't be done until all
   * hosts/nets/links have been configured.
   */
  void finish_config_top_net(s3f::dml::Configuration* cfg);

  /** load forwarding tables of all the hosts containing in this net */
  void load_fwdtable(s3f::dml::Configuration* cfg);
};

/**
 * \brief Extra options associated with a net.
 *
 * We observe that not all net objects should own some attributes like
 * as AS boundary (whether this net is at the boundary of an AS), AS
 * number, etc. This class provides a uniform interface for these
 * attributes.
 */
class NetAccessory {
 public:
  /** The constructor. */
  NetAccessory()  {}

  /** The destructor. */
  ~NetAccessory();

  /** Add an attribute of bool type. */
  void addAttribute(byte at, bool value);

  /** Add an attribute of int type. */
  void addAttribute(byte at, int value);

  /** Add an attribute of double type. */
  void addAttribute(byte at, double value);

  /** Add an attribute of pointer type. */
  void addAttribute(byte at, void* value);

  /** Request an attribute of bool type. */
  bool requestAttribute(byte at, bool& value);

  /** Request an attribute of int type. */
  bool requestAttribute(byte at, int& value);

  /** Request an attribute of double type. */
  bool requestAttribute(byte at, double& value);

  /** Request an attribute of pointer type. */
  bool requestAttribute(byte at, void*& value);

 protected:
  /**
   * \internal
   * A common attribute in Net.
   */
  struct NetAttribute {
    /** The attribute name. */
    byte name;
    /** The attribute value. */
    union {
      bool bool_val;     ///< boolean type value
      int int_val;       ///< integer type value
      double double_val; ///< double type value
      void* pointer_val; ///< pointer type value
    } value;

    /** The destructor. */
    ~NetAttribute();
  };

  typedef S3FNET_VECTOR(NetAttribute*) NET_ATTRIB_VECTOR;

  /** List of attributes. */
  NET_ATTRIB_VECTOR netAttribList;
};
    
}; // namespace s3fnet
}; // namespace s3f

#endif /*__NET_H__*/

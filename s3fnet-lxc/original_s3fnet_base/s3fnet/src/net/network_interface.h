/**
 * \file network_interface.h
 * \brief Header file for the NetworkInterface class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __NETWORK_INTERFACE_H__
#define __NETWORK_INTERFACE_H__

#include "os/base/protocol_graph.h"
#include "net/dmlobj.h"
#include "os/base/lowest_protocol_session.h"
#include "net/ip_prefix.h"
#include "net/mac48_address.h"
#include "s3fnet.h"
#include "s3f.h"

namespace s3f {
namespace s3fnet {

class Host;
class Link;

typedef S3FNET_VECTOR(IPADDR) S3FNET_IFACE_IPADDR_VECTOR;

/**
 * \brief A network interface.
 *
 * This class represents a network interface card (NIC) inside a node (e.g., host, router, switch).
 * A node may contain multiple network interfaces.
 * The network_interface class is a protocol graph consisting of two protocol sessions: the MAC layer and the PHY layer.
 * If unspecified in DML, the default MAC layer protocol is SimpleMac and the default PHY layer protocol is SimplePhy.
 */
class NetworkInterface : public DmlObject, public ProtocolGraph {
  friend class Link;
  friend class TxTimerQueue;

 public:
  /**
   * The constructor.
   * The parent object is the host containing this network interface.
   */
  NetworkInterface(Host* parent, long id);

  /**
   * The destructor.
   */
  virtual ~NetworkInterface();
  
  /** Configure the network interface from DML. */
  virtual void config(s3f::dml::Configuration* cfg);

  /** Initialize all protocol sessions defined in this network interface. */
  virtual void init();

  /**
   * Get the IP address of this network interface.
   * The IP address is not settled until the end of the config phase.
   */
  IPADDR getIP() { return ip_addr; }

  /** 
   * Get the MAC address of this network interface. In this function, the MAC address
   * is treated the same as IP (cast to long). The MAC address is not
   * settled until the end of the config phase.
   */
  MACADDR getMacAddr() { return ARP_IP2MAC(ip_addr); };

  /** Get the 48-bit MAC address of this network interface */
  Mac48Address* getMac48Addr() { return mac48_addr; }

  /** 
   * Return the link that this network interface is attached to.
   * This method should not be called before the end of the config phase.
   */
   Link* getLink() { return attached_link; }

  /** Return the highest protocol session in the interface, i.e., the MAC session. */
  ProtocolSession* getHighestProtocolSession() { return mac_sess; }

  /** Return the lowest protocol session in the interface, i.e., the physical layer session. */
  ProtocolSession* getLowestProtocolSession() { return phy_sess; }

  /** Return the host that contains this network interface. */
  Host* getHost();

  /** Print out the name of this network interface. */
  virtual void display(int indent = 0);

  /**
   * Send a packet with the given delay.
   * Essentially, the OutChannel->write is called
   * @param pkt packet to send.
   * @param delay OutChannel Write Delay, including propagation delay and packet transfer delay
   */
  void sendPacket(Activation pkt, ltime_t delay);

  /**
   * Receive a packet.
   * Called by the inChannnel activation function
   */
  void receivePacket(Activation pkt);

  /** The associated S3F OutChannel. */
  OutChannel* oc;

  /** The associated S3F InChannel. */
  InChannel* ic;

 protected:
  /** The IP address allocated to this interface. */
  IPADDR ip_addr;

  /** The MAC address allocated to this interface. */
  MACADDR mac_addr;

  /** The MAC48 address (48 bit IEEE addresses) allocated to this interface. */
  Mac48Address* mac48_addr;

  /**
   * Indicator of whether the OutChannel is connected.
   * Default is false, change when mapping to other InChannel
   */
  bool is_oc_connected;

  /**
   * Indicator of whether the InChannel is connected.
   * Default is false, change when mapping to other OutChannel
   */
  bool is_ic_connected;

  /** Set the medium that this interface is attached to
   *  attached_link() is called after link connection is created
   */
  void attach_to_link(Link* link);

  /** The link that this interface is attached to. */
  Link* attached_link;

  /** The MAC layer protocol.
   *  Should be the highest protocol layer in the interface's protocol graph.
   */
  NicProtocolSession* mac_sess;

  /** The PHY layer protocol.
   *  Should be the lowest protocol layer in the interface's protocol graph.
   */
  LowestProtocolSession* phy_sess;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__NETWORK_INTERFACE_H__*/

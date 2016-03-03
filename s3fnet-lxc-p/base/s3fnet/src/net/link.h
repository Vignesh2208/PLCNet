/**
 * \file link.h
 * \brief Header file for the Link class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __LINK_H__
#define __LINK_H__

#include "util/shstl.h"
#include "net/ip_prefix.h"
#include "net/dmlobj.h"

namespace s3f {
namespace s3fnet {

class Net;
class NetworkInterface;

/** 
 * \brief A link that connects interfaces.
 */
class Link : public DmlObject {
  friend class Net;
  friend class NetworkInterface;
  
 public:
  /** The constructor. */
  Link(Net* net);

  /** The destructor. */
  virtual ~Link();

  /** Configure the link from DML specification. */
  void config(s3f::dml::Configuration* cfg);

  /** Initialize the link. */
  void init();
  
  //map IC and OC, getInterface from nhi (from TimelineService or Net)
  int connect(Net* pNet);

  /** Prints out the properties of this link. */
  virtual void display(int indent = 0);

  /** Return the IP address with a length. */
  inline void getPrefix(IPPrefix& prefix) { prefix = ip_prefix; }

  /** Return the delay of the link. */
  inline double getDelay() { return delay; }

  int getNumOfNetworkInterface();

  /**
   * Return the vector of all the connected network interfaces.
   */

  vector<NetworkInterface*> getNetworkInterfaces() { return connected_nw_iface_vec;}

 protected:
  /** The parent net of this link */
  Net* owner_net;

  /** The vector storing all the connected network interfaces of this link */
  vector<NetworkInterface*> connected_nw_iface_vec;

  /** The minimum link delay.
   *  e.g., min packet size / max bandwidth
   */
  double min_delay;

  /** The propagation delay */
  double prop_delay;

  /** The link delay, used as mapping delay */
  double delay;

  /** IP Address block for this link. */
  IPPrefix ip_prefix;

  /** Called by the net to create a link. */
  static Link* createLink(Net* net, s3f::dml::Configuration* cfg);

};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__LINK_H__*/

/**
 * \file network_interface.cc
 *
 * \brief Source file for the NetworkInterface class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "net/network_interface.h"
#include "os/base/protocols.h"
#include "util/errhandle.h"
#include "net/net.h"
#include "net/host.h"
#include "os/simple_phy/simple_phy.h"
#include "os/simple_mac/simple_mac.h"
#include "env/namesvc.h"
#include "net/link.h"


namespace s3f {
namespace s3fnet {

#define DEFAULT_PHY_CLASSNAME SIMPLE_PHY_PROTOCOL_CLASSNAME
#define DEFAULT_MAC_CLASSNAME SIMPLE_MAC_PROTOCOL_CLASSNAME

#ifdef IFACE_DEBUG
#define IFACE_DUMP(x) printf("NW_IFACE: "); x
#else
#define IFACE_DUMP(x)
#endif

NetworkInterface::NetworkInterface(Host* parent, long nicid) :
  DmlObject(parent, nicid), attached_link(0), mac_sess(0), phy_sess(0)
{
  assert(myParent); // the parent is the host

  IFACE_DUMP(printf("[id=%ld] new network interface.\n", nicid));

  /* init mac48_addr */
  mac48_addr = Mac48Address::Allocate();

  // wait until the config phase to settle the nhi and ip address, as
  // well as the mac and phy layer protocol sessions..
}

NetworkInterface::~NetworkInterface()
{
  IFACE_DUMP(printf("[nhi=\"%s\", ip=\"%s\"] delete network interface.\n",
		    nhi.toString(), IPPrefix::ip2txt(ip_addr)));
}

void NetworkInterface::config(s3f::dml::Configuration* cfg)
{
  // settle the nhi address of this interface
  nhi = myParent->nhi;
  nhi += id;
  nhi.type = Nhi::NHI_INTERFACE;

  // settle the ip address and register the interface to the community
  S3FNET_STRING nhi_str = nhi.toStlString();

  Net* owner_net = getHost()->inNet();
  ip_addr = owner_net->getNameService()->nhi2ip(nhi_str);
  owner_net->register_interface(this, ip_addr);
  IFACE_DUMP(cout << "NIC = " << nhi_str.c_str()
		  << ", IP = " << IPPrefix::ip2txt(ip_addr)
  	  	  << ", MAC = " << mac48_addr << endl;);

  // configure the protocol sessions (this will skip the mac/phy
  // session config unless they are specified explicitly in dml)
  ProtocolGraph::config(cfg);

  // if mac protocol is not specified, create the default mac protocol
  mac_sess = (NicProtocolSession*)sessionForName(MAC_PROTOCOL_NAME);
  if(!mac_sess)
  {
    mac_sess = (NicProtocolSession*)Protocols::newInstance(DEFAULT_MAC_CLASSNAME, this);
    if(!mac_sess)
    	error_quit("ERROR: unregistered protocol \"%s\".\n", DEFAULT_MAC_CLASSNAME);

    // configure the mac protocol session using the interface dml attributes
    mac_sess->config(cfg);
    mac_sess->name = sstrdup(MAC_PROTOCOL_NAME);
    mac_sess->use  = sstrdup(DEFAULT_MAC_CLASSNAME);
    insert_session(mac_sess);
  }
  IFACE_DUMP(printf("[nhi=\"%s\", ip=\"%s\"] config(): mac session \"%s\".\n",
		    nhi_str.c_str(), IPPrefix::ip2txt(ip_addr), mac_sess->use));

  // if phy protocol is not specified, create the default phy protocol
  phy_sess = (LowestProtocolSession*)sessionForName(PHY_PROTOCOL_NAME);
  if(!phy_sess)
  {
    phy_sess = (LowestProtocolSession*)Protocols::newInstance(DEFAULT_PHY_CLASSNAME, this);
    if(!phy_sess)
    	error_quit("ERROR: unregistered protocol \"%s\".\n", DEFAULT_PHY_CLASSNAME);

    // configure the phy protocol session using the interface dml attributes
    phy_sess->config(cfg);
    phy_sess->name = sstrdup(PHY_PROTOCOL_NAME);
    phy_sess->use  = sstrdup(DEFAULT_PHY_CLASSNAME);
    insert_session(phy_sess);
  }

  bool is_lowest;
  if(phy_sess->control(PSESS_CTRL_SESSION_IS_LOWEST, &is_lowest, 0) || !is_lowest)
    error_quit("ERROR: PHY session \"%s\" is not the lowest protocol session.\n", phy_sess->use);
  IFACE_DUMP(printf("[nhi=\"%s\", ip=\"%s\"] config(): phy session \"%s\".\n",
		    nhi_str.c_str(), IPPrefix::ip2txt(ip_addr), phy_sess->use));

  ProtocolSession* ip_sess = getHost()->getNetworkLayerProtocol();

  mac_sess->control(PSESS_CTRL_SESSION_SET_CHILD, 0, phy_sess);
  mac_sess->control(PSESS_CTRL_SESSION_SET_PARENT, 0, ip_sess);
  phy_sess->control(PSESS_CTRL_SESSION_SET_PARENT, 0, mac_sess);

  //create InChannel
  stringstream ss;
  ss << "IC-" << nhi.toStlString();
  ic = new InChannel( (Host *)this->myParent, ss.str());

}

void NetworkInterface::init()
{
  IFACE_DUMP(printf("[nhi=\"%s\", ip=\"%s\"] init().\n", nhi.toString(), IPPrefix::ip2txt(ip_addr)));

  ProtocolGraph::init();
}

void NetworkInterface::sendPacket(Activation pkt, ltime_t delay)
{
  IFACE_DUMP(printf("[nhi=\"%s\", ip=\"%s\"] %s: send packet with delay %ld.\n",
		    nhi.toString(), IPPrefix::ip2txt(ip_addr), getHost()->getNowWithThousandSeparator(), delay));

  unsigned int pri = getHost()->tie_breaking_seed;

  /** The delay in the argument is transmission delay + queueing delay + jitter + iface_latency (i.e. oc per-write-delay)
   *  Adjust this delay before outChannel write by removing the min_packet_transfer_delay portion (i.e. mini pkt/max_bandwidth)
   *  Link prop_delay will be added (in s3f).
   *  Actually, both min_transfer_delay and prop_delay will be added when calling outchannel.write().
   */

  ltime_t link_min_delay = getHost()->d2t(attached_link->min_delay, 0);
  if(delay - link_min_delay < 0)
  {
	  printf("Warning: packet delay (%ld) < link's min packet transfer delay (%ld), "
			  "consider re-assign the min_packet_transfer_delay in dml\n",
			  delay, link_min_delay);
	  delay = 0;
  }
  else
  {
	  delay = delay - link_min_delay;
  }

  IFACE_DUMP(printf("NetworkInterface::sendPacket, link_min_delay = %ld, "
		  "delay to write to outChannel = %ld, pri = %u\n", link_min_delay, delay, pri));

  oc->write(pkt, delay, pri);
}

void NetworkInterface::receivePacket(Activation pkt)
{
  IFACE_DUMP(printf("[nhi=\"%s\", ip=\"%s\"] %s: receive a packet.\n",
		    nhi.toString(), IPPrefix::ip2txt(ip_addr), getHost()->getNowWithThousandSeparator()));

  // we pass the packet directly to the phy session
  phy_sess->receivePacket(pkt);
}

void NetworkInterface::attach_to_link(Link* link)
{
  // we should check that an interface is attached to but one link;
  // however, the preprocessing step should have already done that
  if(attached_link)
    error_quit("ERROR: NetworkInterface::attach_to_link(), the interface cannot be attached to multiple links.\n");
  attached_link = link;
}

Host* NetworkInterface::getHost()
{
  return dynamic_cast<Host*>(myParent);
}

void NetworkInterface::display(int indent)
{
  printf("%*cNetworkInterface %lu", indent, ' ', id);
}

}; // namespace s3fnet
}; // namespace s3f

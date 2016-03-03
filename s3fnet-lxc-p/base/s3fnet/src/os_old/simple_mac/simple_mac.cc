/**
 * \file simple_mac.cc
 * \brief Source file for the SimpleMac class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "os/simple_mac/simple_mac.h"
#include "util/errhandle.h"
#include "net/network_interface.h"
#include "net/mac48_address.h"
#include "os/base/protocols.h"
#include "os/ipv4/ip_interface.h"
#include "net/forwardingtable.h"
#include "os/ipv4/ip_message.h"
#include "os/simple_mac/simple_mac_message.h"
#include "net/mac48_address.h"
#include "net/host.h"
#include "env/namesvc.h"

namespace s3f {
namespace s3fnet {

#ifdef SMAC_DEBUG
#define SMAC_DUMP(x) printf("SMAC: "); x
#else
#define SMAC_DUMP(x)
#endif

S3FNET_REGISTER_PROTOCOL(SimpleMac, SIMPLE_MAC_PROTOCOL_CLASSNAME);

SimpleMac::SimpleMac(ProtocolGraph* graph) : NicProtocolSession(graph)
{
  SMAC_DUMP(printf("[nic=\"%s\"] new simple_mac session.\n", ((NetworkInterface*)inGraph())->nhi.toString()));
}

SimpleMac::~SimpleMac() 
{
  SMAC_DUMP(printf("[nic=\"%s\"] delete simple_mac session.\n", ((NetworkInterface*)inGraph())->nhi.toString()));
}

void SimpleMac::init()
{  
  SMAC_DUMP(printf("[nic=\"%s\", ip=\"%s\"] init().\n", 
		   ((NetworkInterface*)inGraph())->nhi.toString(),
		   IPPrefix::ip2txt(getNetworkInterface()->getIP())));
  NicProtocolSession::init(); 

  if(strcasecmp(name, MAC_PROTOCOL_NAME))
    error_quit("ERROR: SimpleMac::init(), unmatched protocol name: \"%s\", expecting \"MAC\".\n", name);
}

int SimpleMac::push(Activation msg, ProtocolSession* hi_sess, void* extinfo, size_t extinfo_size)
{
  SMAC_DUMP(printf("[nic=\"%s\", ip=\"%s\"] %s: push().\n",
		   ((NetworkInterface*)inGraph())->nhi.toString(),
		   IPPrefix::ip2txt(getNetworkInterface()->getIP()), getNowWithThousandSeparator()));

  // cast the extinfo to the standard structure.
  IPOptionToBelow* mac_ext_info = (IPOptionToBelow*)extinfo;
  assert(mac_ext_info);

  //add new mac header for every packet for both new packet and forwarding
  RouteInfo* route = (RouteInfo*)(mac_ext_info->routing_info);

  /* Use Mac48Address instead of MACADDR (MACADD = IP) */
  /* get dest MAC */
  //MACADDR dest;
  Mac48Address* dst_mac;
  if(route)
  {
    if(IPADDR_NEXTHOP_UNSPECIFIED != route->next_hop)
    {
      //dest = ARP_IP2MAC(route->next_hop);
    	dst_mac = inHost()->getTopNet()->getNameService()->ip2mac48(route->next_hop);
    }
    else
    {
      //dest = ARP_IP2MAC(((IPMessage*)msg)->dst_ip);
      dst_mac = inHost()->inNet()->getNameService()->ip2mac48(((IPMessage*)msg)->dst_ip);
    }
  }
  else
  {
	  error_quit("SimpleMac::push(), no route\n");
  }

  /* get src MAC */
  //MACADDR src = getNetworkInterface()->getMacAddr();
  Mac48Address* src_mac = getNetworkInterface()->getMac48Addr();

  //Generate the MAC Header and put it in front of the IP Message
  //SimpleMacMessage* simple_mac_header = new SimpleMacMessage(src, dest);
  SimpleMacMessage* simple_mac_header = new SimpleMacMessage(src_mac, dst_mac);
  simple_mac_header->carryPayload((IPMessage*)msg);
  Activation simp_mac_hdr(simple_mac_header);

  if(!child_prot)
    error_quit("ERROR: SimpleMac::push(), child protocol session has not been set.\n");

  return child_prot->pushdown(simp_mac_hdr, this);
}

int SimpleMac::pop(Activation msg, ProtocolSession* lo_sess, void* extinfo, size_t extinfo_size)
{  
  SMAC_DUMP(printf("[nic=\"%s\", ip=\"%s\"] %s: pop().\n",
		   ((NetworkInterface*)inGraph())->nhi.toString(),
		   IPPrefix::ip2txt(getNetworkInterface()->getIP()), getNowWithThousandSeparator()));

  SimpleMacMessage* mac_hdr = (SimpleMacMessage*)msg;

  /*SMAC_DUMP(char s0[50]; char s1[50];
  	  	  	printf("[nic=\"%s\", ip=\"%s\"] msg with dest MAC \"%s\".\n",
  	  	  			((NetworkInterface*)inGraph())->nhi.toString(),
  	  	  			IPPrefix::ip2txt(getNetworkInterface()->getIP(), s0),
  	  	  			IPPrefix::ip2txt(ARP_MAC2IP(mac_hdr->dest), s1)));*/

  SMAC_DUMP(cout << "[nic=\"" << ((NetworkInterface*)inGraph())->nhi.toString()
		  << "\", ip=\"" << IPPrefix::ip2txt(getNetworkInterface()->getIP())
  	  	  << "\"] src48_mac = " << mac_hdr->src48
  	  	  << ", dst48_mac = " << mac_hdr->dst48 << endl;);

  // check whether this packet is for myself. Ignore it if not.
  //if(mac_hdr->dest != 0 && getNetworkInterface()->getMacAddr() != mac_hdr->dest)
  //if(getNetworkInterface()->getMacAddr() != mac_hdr->dest)
  if(!getNetworkInterface()->getMac48Addr()->IsEqual(mac_hdr->dst48))
  {
	msg->erase_all();
	return 0;
  }

  if(!parent_prot)
  {
    error_quit("ERROR: SimpleMac::pop(), parent protocol session has not been set.\n");
  }

  //send to upper layer, currently it is the IP layer
  ProtocolMessage* payload = mac_hdr->dropPayload();
  assert(payload);
  Activation ip_msg (payload);
  mac_hdr->erase(); //delete MAC header

  return parent_prot->popup(ip_msg, this);
}

}; // namespace s3fnet
}; // namespace s3f

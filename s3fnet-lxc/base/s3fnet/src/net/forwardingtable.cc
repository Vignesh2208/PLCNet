/**
 * \file forwardingtable.cc
 * \brief Source file for the ForwardingTable class.
 *
 * authors : Dong (Kevin) Jin, Lenny Winterrowd
 */

#include "net/forwardingtable.h"
#include "util/errhandle.h"
#include "net/net.h"
#include "net/host.h"
#include "net/link.h"
#include "env/namesvc.h"
#include "net/network_interface.h"

// Trie implementations
#include "util/tries/trie0.h"
#include "util/tries/trie1.h"
#include "util/tries/trie2.h"
#include "util/tries/trie3.h"
#include "util/tries/trie4.h"

// Cache implementations
#include "net/route_caches/route_cache0.h"
#include "net/route_caches/route_cache1.h"
#include "net/route_caches/route_cache2.h"

namespace s3f {
namespace s3fnet {

#ifdef FWT_DEBUG
#define FWT_DUMP(x) printf("FWT: "); x
#else
#define FWT_DUMP(x)
#endif

//#define DEFAULT_TRIE_VERSION Trie::ORIGINAL
#define DEFAULT_TRIE_VERSION Trie::SIMPLE
//#define DEFAULT_TRIE_VERSION Trie::PREFIX
//#define DEFAULT_TRIE_VERSION Trie::UNORDERED_PREFIX
//#define DEFAULT_TRIE_VERSION Trie::ARRAY_BACKED

#define DEFAULT_CACHE_VERSION RouteCache::SINGLE_ENTRY
//#define DEFAULT_CACHE_VERSION RouteCache::DIRECT_MAPPED
//#define DEFAULT_CACHE_VERSION RouteCache::ASSOCIATIVE
//#define DEFAULT_CACHE_VERSION RouteCache::NONE

// dml configuration
/*
#define FORWARDING_TABLE "forwarding_table"
#define NODE_NHI         "node_nhi"
#define INTERFACE        "interface"
#define ROUTE            "route"
#define DMLROUTE         "dml_route"
#define ID               "id"
#define IP_ADDR          "ip_addr"
#define DEST		 "dest"
#define DEST_DEFAULT	 "default"
#define DEST_IP          "dest_ip"
#define NEXT_HOP         "next_hop"
#define COST             "cost"
#define PROTOCOL         "protocol"
*/

char* RouteInfo::routingProtocol2Str[] = {
  "UNSPEC", "STATIC", "IGP", "EGP", "BGP", "OSPF", "XORP", "PAO", "REVERSE"
};

void RouteInfo::display(FILE* fp)
{
  if(!fp) fp = stderr;
  fprintf(fp, "\troute [\n");
  fprintf(fp, "\t\tdest_ip ");  
  destination.display(fp);
  fprintf(fp, "\n\t\tnext_hop %s", IPPrefix::ip2txt(next_hop));
  fprintf(fp, "\n\t\tinterface [%d] %s", (int)nic->id, IPPrefix::ip2txt(nic->getIP()));
  fprintf(fp, "\n\t\tcost %d", cost);
  fprintf(fp, "\n\t]\n");
}

void RouteInfo::config(Host* h, s3f::dml::Configuration* cfg)
{
  char* str = (char*)cfg->findSingle("dest_ip");
  if(!str || s3f::dml::dmlConfig::isConf(str))
    error_quit("ERROR: RouteInfo::config(), missing or invalid DEST_IP attribute.\n");
  if(!strcasecmp(str, "default"))
  {
    destination.addr = 0;
    destination.len = 0;
  }
  else destination.addr = IPPrefix::txt2ip(str, &(destination.len));

  str = (char*)cfg->findSingle("interface");
  if(!str || s3f::dml::dmlConfig::isConf(str))
    error_quit("ERROR: RouteInfo::config(), missing or invalid INTERFACE attribute.\n");
  int iface = atoi(str);
  nic = h->getNetworkInterface(iface);
  if(!nic)
  {
    error_quit("ERROR: RouteInfo::config(), nic %d on host \"%s\" non-exist.\n", iface, h->nhi.toString());
  }

  str = (char*)cfg->findSingle("next_hop");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: RouteInfo::config(), invalid NEXT_HOP attribute.\n");
    int len=0;
    next_hop = IPPrefix::txt2ip(str, &len);
    // the prefix length is useless here
  }
  else next_hop = IPADDR_INVALID;

  str = (char*)cfg->findSingle("cost");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: RouteInfo::config(), invalid COST attribute.\n");
    cost = atoi(str);
  }
  else
  {
    if(destination.addr == 0 && destination.len == 0)
      cost = 0x7ffffff; // default route has highest cost
    else cost = 1;
  }

  str = (char*)cfg->findSingle("protocol");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: RouteInfo::config(), invalid PROTOCOL attribute.\n");
    if(!strncmp(str, "STATIC", 6)) {
      protocol = STATIC;
    } else if (!strncmp(str, "IGP", 3)) {
      protocol = IGP;
    } else if (!strncmp(str, "EGP", 3)) {
      protocol = EGP;
    } else if (!strncmp(str, "BGP", 3)) {
      protocol = BGP;
    } else if (!strncmp(str, "OSPF", 4)) {
      protocol = OSPF;
    } else if (!strncmp(str, "PAO", 3)) {
      protocol = PAO;
    } else {
      error_quit("ERROR: RouteInfo::config(), invalid PROTOCOL: \"%s\".\n", str);
    }
  }
  else protocol = STATIC;
}

RouteInfo* RouteInfo::resolve(Host* machine)
{
  if(resolved) return this;

  Link* link = (Link*)nic->getLink();
  if(!link)
  {
    char buf1[50]; char buf2[50];
    error_retn("WARNING: RouteInfo::resolve() interface %s is not attached at host %s.\n", 
	       nic->nhi.toString(buf1), machine->nhi.toString(buf2));
    return 0;
  }

  IPADDR local_ip = nic->getIP();
  if(next_hop == local_ip) next_hop = IPADDR_INVALID;

  if(IPADDR_INVALID == next_hop)
  {
    // nic must have just 1 peer so that the next_hop is the only peer.
    int num_link_itrfs = 0;
    //link->getInterfaces(machine->getCommunity(), num_link_itrfs);
    num_link_itrfs = link->getNumOfNetworkInterface();
    // if the link is a LAN link, we must specify next hop address in
    // the dml file. Otherwise, we use the ip address of the only
    // interface
    if(num_link_itrfs > 2)
    {
      link->display();
      error_retn("WARNING: RouteInfo::resolve(), next_hop cannot be determined for LAN links on host %s.\n",
		 machine->nhi.toString());
      return 0;
    }
    
    // find the other attachment point to be the next hop
    //S3FNET_STRING_VECTOR& v = link->getIfaceNhis();
    vector<NetworkInterface*> ifaces = link->getNetworkInterfaces();

    for(unsigned i = 0; i < ifaces.size(); i++)
    {
      next_hop = ifaces[i]->getIP();
      if(next_hop != local_ip) break;
    }
  }
  else
  { // if next hop is specified, make sure it's one of the attachment points
    bool found = false;
    //S3FNET_STRING_VECTOR& v = link->getIfaceNhis();
    vector<NetworkInterface*> ifaces = link->getNetworkInterfaces();
    for(unsigned i = 0; i < ifaces.size(); i++)
    {
      IPADDR attached = ifaces[i]->getIP();
      if(attached == next_hop)
      { // note that next hop cannot be local ip (see above)
    	found = true;
    	break;
      }
    }
    if(!found)
    {
      error_retn("WARNING: RouterInfo::resolve(), invalid next_hop \"%s\" for host \"%s\"\n",
		 IPPrefix::ip2txt(next_hop), machine->nhi.toString());
      return 0;
    }
  }

  resolved = true;
  return this;
}

ForwardingTable::ForwardingTable(ProtocolSession* ipsess, int trie_version, int cache_version) :
		ip_sess(ipsess) {
  if(trie_version == -1) {
	trie_version = DEFAULT_TRIE_VERSION;
  }

  if(cache_version == -1) {
	cache_version = DEFAULT_CACHE_VERSION;
  }

  // TODO: Make Trie and Cache configurable via dml (based on version)
  tvstart:
	switch(trie_version) {
	case Trie::ORIGINAL:
		mTrie = (Trie*)new trie0::Trie0();
		break;
	case Trie::SIMPLE:
		mTrie = (Trie*)new trie1::Trie1();
		break;
	case Trie::PREFIX:
		mTrie = (Trie*)new trie2::Trie2();
		break;
	case Trie::UNORDERED_PREFIX:
		mTrie = (Trie*)new trie3::Trie3();
		break;
	case Trie::ARRAY_BACKED:
		mTrie = (Trie*)new trie4::Trie4();
		break;
	default:
		fprintf(stderr, "Invalid forwarding table version (%d). Using default (%d).",trie_version,
				DEFAULT_TRIE_VERSION);
		trie_version = DEFAULT_TRIE_VERSION;
		goto tvstart;
	}
	assert(mTrie);

  cvstart:
	switch(cache_version) {
	case RouteCache::SINGLE_ENTRY:
		mCache = (RouteCache*)new routecache0::RouteCache0();
		break;
	case RouteCache::DIRECT_MAPPED:
		mCache = (RouteCache*)new routecache1::RouteCache1();
		break;
	case RouteCache::ASSOCIATIVE:
		mCache = (RouteCache*)new routecache2::RouteCache2();
		break;
	case RouteCache::NONE:
		mCache = NULL;
		break;
	default:
		fprintf(stderr, "Invalid cache version (%d). Using default (%d).",cache_version,
				DEFAULT_CACHE_VERSION);
		cache_version = DEFAULT_CACHE_VERSION;
		goto cvstart;
	}
}

ForwardingTable::~ForwardingTable() {
	if(mTrie) {
		delete mTrie;
	}
	if(mCache) {
		delete mCache;
	}
}

int ForwardingTable::addRoute(RouteInfo* route, bool replace)
{
  if(mCache) {
	mCache->invalidate(); // TODO: only invalidate if necessary
  }

  // if returns a conflict route; it could be the original or the new
  // route depending on whether 'replace' is true or not
  RouteInfo* conflict = (RouteInfo*)
    mTrie->insert(route->destination.addr, route->destination.len, route, replace);
  if(conflict) {
    if(replace) {
      for(S3FNET_FIB_PROTO_SET::iterator iter = listeners.begin();
	  iter != listeners.end(); iter++) {
	(*iter)->control(IP_CTRL_FIB_DELROUTE, conflict, ip_sess);
	(*iter)->control(IP_CTRL_FIB_ADDROUTE, route, ip_sess);
      }
      delete conflict; // this is the old route
      return FT_ROUTE_OVERWRITTEN;
    } else {
      assert(conflict == route);
      delete conflict;  // this is the new route
      return FT_ROUTE_NOT_REPLACED;
    }
  } else {
    for(S3FNET_FIB_PROTO_SET::iterator iter = listeners.begin();
	iter != listeners.end(); iter++) {
      (*iter)->control(IP_CTRL_FIB_ADDROUTE, route, ip_sess);
    }
    return FT_ROUTE_SUCCESS;
  }
}

int ForwardingTable::removeRoute(RouteInfo* route)
{
  if(mCache) {
  	mCache->invalidate(); // TODO: only invalidate if necessary
  }

  RouteInfo* old = (RouteInfo*)mTrie->remove(route->destination.addr, route->destination.len);
  if(old) {
    // we need to compare the existing route with the one we want to
    // remove; if they don't match, we reinstall the existing one;
    // otherwise, we remove it
    if(*route != *old) addRoute(old);
    else {
      for(S3FNET_FIB_PROTO_SET::iterator iter = listeners.begin();
	  iter != listeners.end(); iter++) {
	(*iter)->control(IP_CTRL_FIB_DELROUTE, old, ip_sess);
      }
      delete old;
    }
    delete route;
    return FT_ROUTE_SUCCESS;
  } else {
    delete route;
    return FT_ROUTE_NOT_FOUND;
  }
}

RouteInfo* ForwardingTable::getRoute(IPADDR ipaddr) {
  RouteInfo* pData;
  if(mCache) {
  	pData = mCache->lookup(ipaddr);
	if(pData != NULL) {
		return pData;
	}
  }

  // find out the route for the given ip address
  pData = (RouteInfo*)mTrie->lookup(ipaddr);

  if(mCache && pData) {
	mCache->update(ipaddr,pData);
  }

  return pData;
}

/* TODO: Reenable dump in each Trie (or as part of the Trie base-class)
void ForwardingTable::dump(FILE* fp)
{
  if(0 == fp) fp = stdout;
  dump_helper(this->root, 0, 0, fp);
}

void ForwardingTable::dump_helper(TrieNode* root, uint32 sofar, int n, FILE* fp)
{
  if(!fp) fp = stdout;

  // Recurse on children, if any.
  for (int i=0; i < TRIE_KEY_SPAN; i++) {
    if(root->children[i]) {
      dump_helper(root->children[i], (sofar << 1) | i, n+1, fp);
    }
  }
  
  if(root->data) {
    RouteInfo* route = (RouteInfo*)root->data;
    fprintf(fp, "\troute [\n");
    fprintf(fp, "\t\tdest_ip ");
    route->destination.display(fp);
    fprintf(fp, "\n\t\tnext_hop %s", IPPrefix::ip2txt(route->next_hop)); 
    fprintf(fp, "\n\t\tinterface [%d] %s", (int)route->nic->id, 
	    IPPrefix::ip2txt(route->nic->getIP()));
    fprintf(fp, "\n\t\tcost %d", route->cost);
    fprintf(fp, "\n\t\tprotocol %s",
	    RouteInfo::routingProtocol2Str[route->protocol]);
    fprintf(fp, "\n\t]\n");
  }
}
*/

/* TODO: Implement within each Trie or as part of the Trie base-class
 * Currently never called in s3fnet.
void ForwardingTable::invalidateAllRoutes(int protocol)
{
  for (Trie::iterator rit = begin(); rit != end(); ++rit) {
    RouteInfo* ri = (RouteInfo*) rit->data;
    if(RouteInfo::UNSPEC == protocol || ri->protocol == protocol) { // right protocol?
      removeRoute(ri);
      FWT_DUMP(char sdst[30]; ri->destination.toString(sdst); 
	       std::cout << "ForwardingTable: removing dest= " << sdst << std::endl);
    }
  }
}
*/

RouteInfo* ForwardingTable::getDefaultRoute()
{
  if(mTrie) return (RouteInfo*)(mTrie->getDefault());
  return 0;
}

void ForwardingTable::addListener(ProtocolSession* sess)
{
  listeners.insert(sess);
}

void DMLRouteInfo::config(s3f::dml::Configuration* cfg)
{
  // read destination
  char* dest_string = (char*)cfg->findSingle("dest");
  if(!dest_string) {
	dest_string = (char*)cfg->findSingle("destip");
	if(!dest_string) {
		error_quit("ERROR: DMLRouteInfo::config(), "
	           "missing or invalid DEST attribute.\n");
	}
	dst_ip = true;
  }
  if(s3f::dml::dmlConfig::isConf(dest_string)) {
    error_quit("ERROR: DMLRouteInfo::config(), "
	       "missing or invalid DEST attribute.\n");
  }
  dst_str = sstrdup(dest_string);

  // read interface
  char* itrf_string = (char*)cfg->findSingle("interface");
  if(!itrf_string || s3f::dml::dmlConfig::isConf(itrf_string))
    error_quit("ERROR: DMLRouteInfo::config(), "
	       "missing or invalid INTERFACE attribute.\n");
  iface_id = atoi(itrf_string);

  // read next hop
  char* nexthop_string = (char*)cfg->findSingle("next_hop");
  if(nexthop_string) {
    if(s3f::dml::dmlConfig::isConf(nexthop_string))
      error_quit("ERROR: DMLRouteInfo::config(), "
		 "invalid NEXT_HOP attribute.\n");
    nexthop_str = sstrdup(nexthop_string);
  }

  char* str = (char*)cfg->findSingle("cost");
  if(str) {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: DMLRouteInfo::config(), "
		 "invalid COST attribute.\n");
    cost = atoi(str);
  } else {
    if(!strcasecmp(dst_str, "default"))
      cost = 0x7ffffff; // default route has highest cost
    else cost = 1;
  }

  str = (char*)cfg->findSingle("protocol");
  if(str) {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: DMLRouteInfo::config(), "
		 "invalid PROTOCOL attribute.\n");
    if(!strncmp(str, "STATIC", 6)) {
      protocol = RouteInfo::STATIC;
    } else if (!strncmp(str, "IGP", 3)) {
      protocol = RouteInfo::IGP;
    } else if (!strncmp(str, "EGP", 3)) {
      protocol = RouteInfo::EGP;
#ifdef S3FNET_PAO_ROUTING
    } else if (!strncmp(str, "PAO", 3)) {
      protocol = RouteInfo::PAO;
#endif
    } else {
      error_quit("ERROR: DMLRouteInfo::config(), "
		 "invalid PROTOCOL: \"%s\".\n", str);
    }
  } else protocol = RouteInfo::STATIC;
}

RouteInfo* DMLRouteInfo::resolve(Host* machine)
{
  RouteInfo* route = new RouteInfo;
 
  // resolve destination.
  if(!strcasecmp(dst_str, "default")) {
    // make this a default route.
    route->destination.addr = 0;
    route->destination.len = 0;
  } else if(!dst_ip) { // we only assume that dst_str is an nic nhi address
    // start with an invalid address
    route->destination.addr = IPADDR_INVALID;

    // if it's not started with a colon, we first assume we are given
    // a relative nhi address
    if(dst_str[0] != ':') {
      S3FNET_STRING abs_addr = machine->myParent->nhi.toStlString();
      if(abs_addr.size() > 0) abs_addr += ":";
      abs_addr += dst_str;
      NameService* namesvc = machine->inNet()->getNameService();
      route->destination.addr = namesvc->nhi2ip(abs_addr);
    }

    // if it's still unresolved, we assume it's an absolute address
    if(IPADDR_INVALID == route->destination.addr) {
      if(dst_str[0] != ':')
      {
        NameService* namesvc = machine->inNet()->getNameService();
	    route->destination.addr = namesvc->nhi2ip(S3FNET_STRING(dst_str));
      }
      else
      {
	    // we need to strip off the first colon if necessary
    	NameService* namesvc = machine->inNet()->getNameService();
	    route->destination.addr = namesvc->nhi2ip(S3FNET_STRING(&dst_str[1]));
      }
    }

    // if it's still unresolved, it's an invalid address
    if(IPADDR_INVALID == route->destination.addr) {
      error_retn("WARNING: DMLRouteInfo::resolve(), "
		 "invalid destination nhi: \"%s\".\n", dst_str);
      delete route;
      return 0;
    }

    route->destination.len = 32; // what else should be here?
  } else { // dst_str is a CIDR IP
	// TODO: Check that this is a valid destination IP (properly resolve at least 1 address in the mask)
	route->destination.addr = IPPrefix::txt2ip(dst_str,&(route->destination.len));
  }

  route->cost = cost;
  route->protocol = protocol;

  // resolve the outgoing interface 
  route->nic = machine->getNetworkInterface(iface_id);
  if(!route->nic) {
    error_retn("WARNING: DMLRouteInfo::resolve(), "
	       "interface %d cannot be resolved at host %s.\n", 
	       iface_id, machine->nhi.toStlString().c_str());
    delete route;
    return 0;
  }

  Link* link = (Link*)route->nic->getLink();
  if(!link) {
    error_retn("WARNING: DMLRouteInfo::resolve() interface %d is not attached at host %s.\n", 
	       iface_id, machine->nhi.toStlString().c_str());
    delete route;
    return 0;
  }
  
  // resolve the nexthop address
  if(!nexthop_str) {
    // nic must have just 1 peer so that the next_hop is only the peer
    int num_link_itrfs = 0;
    //link->getInterfaces(machine->getCommunity(), num_link_itrfs);
    num_link_itrfs = link->getNumOfNetworkInterface();

    // Currently, if the link is a LAN link, we must specify next hop address in the
    // dml file. Otherwise, we always use a default value:
    if(num_link_itrfs > 2) {
      link->display();
      error_retn("WARNING: DMLRouteInfo::resolve(), "
		 "next_hop must be specified for interfaces on LAN links!\n");
      delete route;
      return 0;
    }
    
    // get the other ip address to be the next hop address
    //S3FNET_STRING_VECTOR& v = link->getIfaceNhis();
    vector<NetworkInterface*> ifaces = link->getNetworkInterfaces();

    IPADDR local_ip = route->nic->getIP();
    for(unsigned i = 0; i < ifaces.size(); i++)
    {
      route->next_hop = ifaces[i]->getIP();
      if(route->next_hop != local_ip) break;
    }
  }
  else {
    // translate the next_hop nhi string to ip address; note that
    // next_hop must be linked with this interface

    // we use abs_addr to be the absolution nhi address and the
    // nexthop_str to be the relative one
    S3FNET_STRING abs_addr;
    if(nexthop_str[0] == ':') {
      // next hop starts with a colon; it's an absolute nhi address
      abs_addr = S3FNET_STRING(&nexthop_str[1]);
    } else {
      // we assume nexthop_str is the relative nhi address
      abs_addr = machine->myParent->nhi.toStlString();
      if(abs_addr.size() > 0) abs_addr += ":";
      abs_addr += nexthop_str;
    }

    //S3FNET_STRING_VECTOR& v = link->getIfaceNhis();
    vector<NetworkInterface*> ifaces = link->getNetworkInterfaces();
    bool found = false; int i;

    // we first assume it's relative
    for(i=0; i<(int)ifaces.size(); i++)
    {
      if(ifaces[i]->nhi.toStlString() == abs_addr)
      {
    	  route->next_hop = ifaces[i]->getIP();
    	  found = true;
    	  break;
      }
    }

    // if it's still unresolved, we assume it's absolute (unless it
    // starts with colon, in which case we already tested at the
    // previous step
    if(!found && nexthop_str[0] != ':')
    {
      for(i=0; i<(int)ifaces.size(); i++)
      {
    	  if(ifaces[i]->nhi.toStlString() == S3FNET_STRING(nexthop_str))
    	  {
    		  route->next_hop = ifaces[i]->getIP();
    		  found = true;
    		  break;
    	  }
      }
    }
    if(!found || IPADDR_INVALID == route->next_hop)
    {
      //for(i=0; i<v.size(); i++) printf("%s\n", v[i].c_str());
      error_retn("ERROR: DMLRouterInfo::resolve(), invalid next_hop \"%s\" for host \"%s\"\n",
		 nexthop_str, machine->nhi.toString());
      delete route;
      return 0;
    }
  }

  route->resolved = true;
  return route;
}

}; // namespace s3fnet
}; // namespace s3f

/**
 * \file host.cc
 * \brief Source file for the Host class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "net/host.h"
#include "util/errhandle.h"
#include "os/base/protocols.h"
#include "os/ipv4/ip_session.h"
#include "net/network_interface.h"
#include "net/forwardingtable.h"
#include "env/namesvc.h"

namespace s3f {
namespace s3fnet {

#ifdef HOST_DEBUG
#define HOST_DUMP(x) printf("HOST: "); x
#else
#define HOST_DUMP(x)
#endif

Host::Host(Timeline* tl, Net* parent, long hostid) : Entity(tl), DmlObject(parent, hostid),
		host_seed(0), is_router(false), is_switch(false), network_prot(0)
{
  HOST_DUMP(printf("[id=%ld] new host.\n", id));
  int i;
  for(i=0; i<31; i++)
  {
	  disp_now_buf[i] = '0';
  }
  disp_now_buf[31]='\0';
}

Host::~Host() 
{
  HOST_DUMP(printf("[nhi=\"%s\"] delete host.\n", nhi.toString()));

  if(rng) delete rng;

  S3FNET_HOST_IFACE_MAP::iterator iter;
  for(iter = ifaces.begin(); iter != ifaces.end(); iter++)
    delete (*iter).second;
}

void Host::config(s3f::dml::Configuration* cfg)
{
  assert(cfg);

  nhi = myParent->nhi;
  nhi += id;
  nhi.type = Nhi::NHI_MACHINE;

  HOST_DUMP(printf("[nhi=\"%s\"] config().\n", nhi.toString()));

  NameService* namesvc = inNet()->getNameService();
  assert(namesvc);

  HOST_DUMP(printf("[nhi=\"%s\"] register host.\n", nhi.toString()));
  dynamic_cast<Net*>(myParent)->register_host(this, S3FNET_STRING(nhi.toStlString()));

  char* str = (char*)cfg->findSingle("rng_level");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: Host::config(), invalid HOST.RNG_LEVEL attribute.\n");
    if(!strcasecmp(str, "host"))
      host_seed = 0;
    else if(!strcasecmp(str, "protocol"))
      host_seed = -1;
    else
      error_quit("ERROR: Host::config(), unknown HOST.RNG_LEVEL \"%s\".\n", str);
  }
  else //default is host-level
	host_seed = 0;

  HOST_DUMP(printf("[nhi=\"%s\"] host_seed=%d.\n", nhi.toString(), host_seed));

  s3f::dml::Configuration* gcfg = (s3f::dml::Configuration*)cfg->findSingle("graph");
  if(!gcfg || !s3f::dml::dmlConfig::isConf(gcfg))
    error_quit("ERROR: host::config(), invalid or missing HOST.GRAPH attribute.\n");
  ProtocolGraph::config(gcfg);

  ProtocolSession* ip = sessionForName(NET_PROTOCOL_NAME);
  if(!ip) ip = sessionForName(IP_PROTOCOL_NAME);
  if(!ip)
  {
    // if the network protocol (IP) is undefined, we create a default ip session
    ip = Protocols::newInstance(IP_PROTOCOL_CLASSNAME, this);
    if(!ip) error_quit("ERROR: unregistered protocol \"%s\".\n", IP_PROTOCOL_CLASSNAME);
  
    // no need to configure the default ip protocol session
    ip->name = sstrdup(NET_PROTOCOL_NAME);
    ip->use  = sstrdup(IP_PROTOCOL_CLASSNAME);
    ip->stage = ProtocolSession::PSESS_CONFIGURED;
    insert_session(ip);
  }
  set_network_layer_protocol(ip);

  s3f::dml::Enumeration* ss = cfg->find("interface");
  while(ss->hasMoreElements())
  {
    s3f::dml::Configuration* icfg = (s3f::dml::Configuration*)ss->nextElement();
    if(!s3f::dml::dmlConfig::isConf(icfg))
      error_quit("ERROR: host::config(), invalid HOST.INTERFACE attribute.\n");
    int low, high;
    Net::getIdrangeLimits(icfg, low, high);
    for(int i = low; i <= high; i++)
      config_network_interface(icfg, i);
  }
  delete ss;
  if(ifaces.empty())
    error_quit("ERROR: host::config(), no interface on host \"%s\".\n", nhi.toString());

  ss = cfg->find("route");
  while(ss->hasMoreElements())
  {
    s3f::dml::Configuration* rcfg = (s3f::dml::Configuration*)ss->nextElement();
    if(!s3f::dml::dmlConfig::isConf(rcfg))
      error_quit("ERROR: host::config(), invalid HOST.ROUTE attribute.\n");
    RouteInfo* route = new RouteInfo;
    route->config(this, rcfg);
    ip->control(IP_CTRL_INSERT_ROUTE, route, NULL);
    HOST_DUMP(printf("[nhi=\"%s\"] install route.\n", nhi.toString()));
  }
  delete ss;
  
  ss = cfg->find("nhi_route");
  while(ss->hasMoreElements())
  {
    s3f::dml::Configuration* rcfg = (s3f::dml::Configuration*)ss->nextElement();
    if(!s3f::dml::dmlConfig::isConf(rcfg))
      error_quit("ERROR: host::config(), invalid HOST.NHI_ROUTE attribute.\n");
    DMLRouteInfo* dml_route = new DMLRouteInfo;
    dml_route->config(rcfg);
    ip->control(IP_CTRL_INSERT_DMLROUTE, dml_route, NULL);
    HOST_DUMP(printf("[nhi=\"%s\"] install dml route.\n", nhi.toString()));
  }
  delete ss;
}

void Host::init()
{
  HOST_DUMP(printf("[nhi=\"%s\"] init().\n", nhi.toString()));

  if(ifaces.empty())
    error_retn("WARNING: Host nhi=\"%s\" has no active interface.\n", nhi.toString());

  rng = new Random::RNG();

  ProtocolGraph::init();

  listen_proc = new Process( (Entity *)this, (void (s3f::Entity::*)(s3f::Activation))&Host::listen);

  //init all network interfaces, and binding the "listen" process to the InChannels
  S3FNET_HOST_IFACE_MAP::iterator iter;
  for(iter = ifaces.begin(); iter != ifaces.end(); iter++)
  {
    (*iter).second->init();
    (*iter).second->ic->bind(listen_proc);
  }

  int nDigits;
  if(nhi.ids[1] == 0) nDigits = 1;
  else nDigits = floor(log10(abs(nhi.ids[1]))) + 1;
  tie_breaking_seed = nhi.ids[0]*pow(10, max(6, nDigits)) + nhi.ids[1];
}

void Host::config_network_interface(s3f::dml::Configuration* cfg, int id)
{
  HOST_DUMP(printf("[nhi=\"%s\"] new interface %d.\n", nhi.toString(), id));
  NetworkInterface* iface = new NetworkInterface(this, id);
  iface->config(cfg);
  ifaces.insert(S3FNET_MAKE_PAIR(id,iface));
}

void Host::listen(Activation pkt)
{
	NetworkInterface* rcv_iface = 0;
	S3FNET_HOST_IFACE_MAP::iterator iter;
	for(iter=ifaces.begin(); iter!=ifaces.end(); ++iter)
	{
	  if( (*iter).second->ic->s3fid() == listen_proc->activeChannel()->s3fid() )
	  {
		  rcv_iface = (*iter).second;
		  break;
	  }
	}

	if(rcv_iface)
	  rcv_iface->receivePacket(pkt);
	else
	  error_quit("ERROR: received an activation not belong to any InChannels of this host.\n");
}

Net* Host::inNet()
{
  return dynamic_cast<Net*>(myParent);
}

NetworkInterface* Host::getNetworkInterface(int id)
{
  S3FNET_HOST_IFACE_MAP::iterator iter;
  iter = ifaces.find(id);
  if(iter != ifaces.end()) return (*iter).second;
  else return 0;
}

void Host::loadForwardingTable(s3f::dml::Configuration* cfg)
{
  HOST_DUMP(printf("[nhi=\"%s\"] load forwarding table.\n", nhi.toString()));

  getNetworkLayerProtocol()->control(IP_CTRL_LOAD_FORWARDING_TABLE, cfg, NULL);
}

IPADDR Host::getDefaultIP()
{
  NameService* namesvc = inNet()->getNameService();
  return namesvc->nhi2ip(nhi.toStlString());
}

NetworkInterface* Host::getNetworkInterfaceByIP(IPADDR ipaddr)
{
  for(S3FNET_HOST_IFACE_MAP::iterator iter = ifaces.begin();
      iter != ifaces.end(); iter++)
  {
    if((*iter).second->getIP() == ipaddr) 
      return (*iter).second;
  }
  return 0;
}

void Host::display(int indent)
{
  printf("%*cHost %lu", indent, ' ', id);
}

ProtocolSession* Host::getNetworkLayerProtocol()
{ 
  if(!network_prot)
  {
    // this is impossible since a default ip protocol is always
    // created if it's undefined in DML
    error_quit("ERROR: network protocol session undefined.\n");
  }
  return network_prot;
}

void Host::set_network_layer_protocol(ProtocolSession* np)
{
  if(network_prot)
  {
    error_quit("ERROR: duplicate network protocol session defined.\n");
  }
  network_prot = np;
}

char* Host::getNowWithThousandSeparator()
{
	char* buffer = disp_now_buf;
	char buffer_old[32]="";
	ltime_t r = 0;
	ltime_t t = now();

	r = t%1000;
	t = t/1000;
	sprintf(buffer, "%03ld", r);
	sprintf(buffer_old, "%s", buffer);
	//printf("buffer = %s buffer_old = %s, t = %ld, r = %ld\n", buffer, buffer_old, t, r);

    while (t != 0)
	{
		r = t%1000;
		t = t/1000;
		if(t!=0)
			sprintf(buffer, "%03ld,%s", r, buffer_old);
		else
			sprintf(buffer, "%ld,%s", r, buffer_old);
		sprintf(buffer_old, "%s", buffer);
		//printf("buffer = %s buffer_old = %s, t = %ld, r = %ld\n", buffer, buffer_old, t, r);
	}

	return disp_now_buf;
}

}; // namespace s3fnet
}; // namespace s3f

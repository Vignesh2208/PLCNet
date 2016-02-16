/**
 * \file net.cc
 * \brief Source file for the Net class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "net/net.h"
#include "net/host.h"
#include "net/traffic.h"
#include "util/errhandle.h"
#include "env/namesvc.h"
#include "net/link.h"
#include "net/network_interface.h"
#include "os/lxcemu/lxcemu_session.h"
#include <sys/types.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

namespace s3f {
namespace s3fnet {

#ifdef NET_DEBUG
#define NET_DUMP(x) printf("NET: "); x
#else
#define NET_DUMP(x)
#endif

Net::Net(SimInterface* sim_interface) :
		DmlObject(0), ip_prefix(0, 0), is_top_net(true), namesvc(0),
		sim_iface(sim_interface), top_net(this), traffic(0), netacc(0)
{
  NET_DUMP(printf("new topnet.\n"));
}

Net::Net(SimInterface* sim_interface, Net* parent, long myid):
		DmlObject(parent, myid), ip_prefix(0, 0), is_top_net(false), netacc(0),
		namesvc(parent->namesvc), sim_iface(sim_interface), top_net(parent->top_net), traffic(0)
{
  NET_DUMP(printf("new subnet: id=%ld.\n", myid));
}

Net::~Net()
{
  NET_DUMP(printf("delete net: nhi=\"%s\".\n", nhi.toString()));

  if(netacc) delete netacc;

  for(S3FNET_INT2PTR_MAP::iterator iter = nets.begin(); iter != nets.end(); iter++)
  {
    assert(iter->second);
    delete (Net*)(iter->second);
  }

  for(S3FNET_INT2PTR_MAP::iterator iter = hosts.begin(); iter != hosts.end(); iter++) {
    assert(iter->second);
    delete (Host*)(iter->second);
  }

  for(unsigned i=0; i<links.size(); i++)
  {
    assert(links[i]);
    delete (Link*)(links[i]);
  }

}

void Net::config(s3f::dml::Configuration* cfg)
{
  NET_DUMP(printf("config().\n"));

  s3f::dml::Configuration* topcfg;
  char* str;
  set<long> uniqset;

  if(!myParent) //topnet
  {
    config_top_net(cfg);
    topcfg = cfg;
    cfg = (s3f::dml::Configuration*)cfg->findSingle("net");
    if(!cfg || !s3f::dml::dmlConfig::isConf(cfg))
      error_quit("ERROR: missing or invalid (top) NET attribute.\n");
  }
  else
  {
    nhi = myParent->nhi;
    nhi += id;
  }
  nhi.type = Nhi::NHI_NET;

  // config alignment
  // now timeline is assigned to the net, default one is 0,
  // maybe should provide option to assign to individual host

  str = (char*)cfg->findSingle("alignment");
  if(!str) alignment = 0;
  else
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: Net::config(), invalid NET.ALIGNMENT attribute.\n");

    alignment = atoi(str);
  }
  NET_DUMP(printf("config(): nhi=\"%s\", alignment=\"%d\".\n", nhi.toString(), alignment));

  //config subnetworks, hosts and links in the sequence in dml, deepest first
  // config sub networks
  NET_DUMP(printf("net \"%s\" config sub networks.\n", nhi.toString()));
  s3f::dml::Enumeration* nenum = cfg->find("net");
  while(nenum->hasMoreElements())
  {
    s3f::dml::Configuration* ncfg = (s3f::dml::Configuration*)nenum->nextElement();
    if(!s3f::dml::dmlConfig::isConf(ncfg))
      error_quit("ERROR: Net::config(), invalid NET.NET attribute.\n");
    config_net(ncfg, uniqset);
  }
  delete nenum;

  // config hosts/routers
  NET_DUMP(printf("net \"%s\" config hosts.\n", nhi.toString()));
  s3f::dml::Enumeration* henum = cfg->find("host");
  while (henum->hasMoreElements())
  {
	s3f::dml::Configuration* hcfg = (s3f::dml::Configuration*)henum->nextElement();
    if(!s3f::dml::dmlConfig::isConf(hcfg))
      error_quit("ERROR: Net::config(), invalid NET.HOST attribute.\n");
    config_host(hcfg, uniqset, false);
  }
  delete henum;

  henum = cfg->find("router");
  while (henum->hasMoreElements())
  {
    s3f::dml::Configuration* hcfg = (s3f::dml::Configuration*)henum->nextElement();
    if(!s3f::dml::dmlConfig::isConf(hcfg))
      error_quit("ERROR: Net::config(), invalid NET.ROUTER attribute.\n");
    config_router(hcfg, uniqset);
  }
  delete henum;

  // config links
  NET_DUMP(printf("net \"%s\" config links.\n", nhi.toString()));
  s3f::dml::Enumeration* lenum = cfg->find("link");
  while(lenum->hasMoreElements())
  {
    s3f::dml::Configuration* lcfg = (s3f::dml::Configuration*)lenum->nextElement();
    if(!s3f::dml::dmlConfig::isConf(lcfg))
      error_quit("ERROR: Net::config(), invalid NET.LINK attribute.\n");
    config_link(lcfg);
  }
  delete lenum;

  if(!myParent) finish_config_top_net(topcfg);

  if (myParent)  configLxcCommands(cfg);
  if (!myParent) configLxcCommands(topcfg);
}

void Net::connect_links()
{
  NET_DUMP(printf("net \"%s\" connect links.\n", nhi.toString()));

  for(S3FNET_INT2PTR_MAP::iterator iter = nets.begin(); iter != nets.end(); iter++)
  {
    Net* nn = (Net*)(iter->second);
    nn->connect_links();
  }

  for(unsigned i=0; i < links.size(); i++)
  {
    Link* pLink = (Link*)links[i];
    pLink->connect(this);
  }
}

/*NameService* Net::getNameService()
{
	if(!myParent) //topNet
		return namesvc;
	else
	{
		(dynamic_cast<Net*>(myParent))->getNameService();
	}
}*/

NameService* Net::getNameService()
{
	return namesvc;
}

IPADDR Net::nicnhi2ip(S3FNET_STRING nhi_str)
{
  NameService* namesvc = getNameService();
  assert(namesvc);
  return namesvc->nhi2ip(nhi_str);
}

/*const char* Net::ip2nicnhi(IPADDR addr)
{
  assert(alignsvc);
  return alignsvc->ip2nicnhi(addr);
}*/

DmlObject* Net::nhi2obj(Nhi* pNhi)
{
  // if it's a network interface, use nicnhi_to_obj_map in the topnet
  if(pNhi->type == Nhi::NHI_INTERFACE)
  {
    return top_net->nicnhi_to_obj(pNhi->toStlString());
  }

  // if it's host or net NHI, search in its nets / hosts
  S3FNET_VECTOR(long)* pIDs = &(pNhi->ids);
  int origStart = pNhi->start;
  int len = pIDs->size() - origStart;;
  int type = pNhi->type;
  // The maximum length for the nhi to be in this net.
  int maxlength = (type == Nhi::NHI_INTERFACE)?2:1;

  // If the NHI is in some other Net, let that net do the work
  if(len > maxlength)
  {
    int id = (*pIDs)[origStart];
    // Find the Net with this id.
    S3FNET_INT2PTR_MAP::iterator iter = nets.find(id);
    if(iter == nets.end()) return 0;
    Net* pNet = (Net*)iter->second;
    pNhi->start++;
    DmlObject* pObject = pNet->nhi2obj(pNhi);
    pNhi->start = origStart;
    return pObject;
  }
  
  // The net/machine is in this net.
  int id = (*pIDs)[origStart];
  if(type == Nhi::NHI_NET)
  {
    S3FNET_INT2PTR_MAP::iterator iter = nets.find(id);
    if(iter == nets.end()) return 0;
    return (Net*)iter->second;
  }
  else
  {
    S3FNET_INT2PTR_MAP::iterator iter = hosts.find(id);
    if(iter == hosts.end()) return 0;
    return  (Host*)iter->second;
  }
}

void Net::getIdrangeLimits(s3f::dml::Configuration* cfg,
         int& low, int& high)
{
  // get id start and end
  char* str = (char*)cfg->findSingle("id");
  if(str) {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: Net::getIdrangeLimits(), invalid ID attribute.\n");
    low = high = atoi(str);
    return;
  }

  s3f::dml::Configuration* rcfg = (s3f::dml::Configuration*)
    cfg->findSingle("idrange");
  if(!rcfg)
    error_quit("Error: Net::getIdrangeLimits(), "
               "neither ID nor IDRANGE is specified.\n");
  if(!s3f::dml::dmlConfig::isConf(rcfg))
    error_quit("ERROR: Net::getIdrangeLimits(), invalid IDRANGE attribute.\n");

  str = (char*)rcfg->findSingle("from");
  if(!str  || s3f::dml::dmlConfig::isConf(str))
    error_quit("ERROR: Net::getIdrangeLimits(), "
               "missing or invalid IDRANGE.FROM attribute.\n");
  low = atoi(str);

  str = (char*)rcfg->findSingle("to");
  if(!str  || s3f::dml::dmlConfig::isConf(str))
    error_quit("ERROR: Net::getIdrangeLimits(), "
               "missing or invalid IDRANGE.TO attribute.\n");
  high = atoi(str);

  if(low > high)
    error_quit("ERROR: Net::getIdrangeLimits(), "
               "IDRANGE FROM %d is greater then TO %d.\n", low, high);
  if(low < 0)
    error_quit("ERROR: Net::getIdrangeLimits(), "
               "IDRANGE.FROM must be non-negative.\n");
}

void Net::init()
{
  NET_DUMP(printf("net \"%s\" init.\n", nhi.toString()));

  if(!myParent) // top net
  {
    if(netacc)
    {
      // find the traffic attribute; initialize the traffic
      Traffic* traffic = 0;
      if(netacc->requestAttribute(NET_ACCESSORY_TRAFFIC, (void*&)traffic))
      {
        if(traffic) traffic->init();
      }
    }
  }

  // initialize nets
  for(S3FNET_INT2PTR_MAP::iterator iter = nets.begin(); iter != nets.end(); iter++)
  {
    ((Net*)iter->second)->init();
  }

  // initialize links.
  // Links must be initialized before hosts because
  // protocols which are initialized with each host
  // might want links initialized.
  for(unsigned i=0; i < links.size(); i++)
  {
      ((Link*)links[i])->init();
  }
}

void Net::display(int indent)
{
  char strNhi[50];
 
  nhi.toString(strNhi);
  printf("%*cNet [\n", indent, ' ');
  indent += DML_OBJECT_INDENT;
  printf("%*cnhi %s\n", indent, ' ', strNhi);
  printf("%*cip_prefix ", indent, ' ');
  ip_prefix.display();
  printf("\n");
  
  // display hosts/routers
  for(S3FNET_INT2PTR_MAP::iterator iter = hosts.begin();
      iter != hosts.end(); iter++)
    ((Host*)(iter->second))->display(indent);

  // display nets
  for(S3FNET_INT2PTR_MAP::iterator iter = nets.begin();
      iter != nets.end(); iter++)
    ((Net*)(iter->second))->display(indent);

  // display Links
  for(unsigned i = 0; i < links.size(); i++) 
    ((Link*)links[i])->display(indent);

  printf("%*c]\n", indent-DML_OBJECT_INDENT, ' ');
}

void Net::config_host(s3f::dml::Configuration* cfg, S3FNET_LONG_SET& uniqset, bool is_router)
{
  // get id start and end
  int id_start, id_end;
  getIdrangeLimits(cfg, id_start, id_end);

  // now the id range is fixed. config them one by one.
  for(int i = id_start; i <= id_end; i++)
  {
    if(!uniqset.insert((long)i).second)
      error_quit("ERROR: duplicate ID %d used for NET.HOST/ROUTER.\n", i);

    NET_DUMP(printf("config_host(), created host %d on timeline %d\n", i, alignment));

    Timeline* tl = sim_iface->get_Timeline(alignment);
    Host* h = new Host(tl, this, (long)i);
    hosts.insert(S3FNET_MAKE_PAIR(i, h));
    if(is_router) h->is_router = true;
    h->config(cfg);
  }
}

void Net::config_router(s3f::dml::Configuration* cfg, S3FNET_LONG_SET& uniqset)
{
  config_host(cfg, uniqset, true);
}

void Net::config_net(s3f::dml::Configuration* cfg, S3FNET_LONG_SET& uniqset)
{
  //CidrBlock* self_cidr = curCidr;

  // get id start and end
  int id_start, id_end;
  getIdrangeLimits(cfg, id_start, id_end);

  // now the id range is fixed. config them one by one.
  for(int i = id_start; i <= id_end; i++) {
    if(!uniqset.insert((long)i).second)
      error_quit("ERROR: duplicate ID %d used for NET.NET.\n", i);

    Net* nn = new Net(sim_iface, this, (long)i);
    assert(nn);

    assert(curCidr);
    CidrBlock* subcidr = curCidr->getSubCidrBlock(i);
    assert(subcidr);
    nn->curCidr = subcidr;
    nn->ip_prefix = subcidr->getPrefix();

    nets.insert(S3FNET_MAKE_PAIR(i, nn));
    nn->config(cfg);
  }

  // before quit, assign self_cidr back.
  //curCidr = self_cidr;
}


void Net::config_link(s3f::dml::Configuration* cfg)
{
  NET_DUMP(printf("config_link().\n"));
  // construct default link
  Link* newlink = Link::createLink(this, cfg);
  links.push_back(newlink);
}

void Net::config_top_net(s3f::dml::Configuration* cfg)
{
  NET_DUMP(printf("config_top_net().\n"));

  id = 0;
  
  // start name resolution service
  s3f::dml::Configuration* acfg = (s3f::dml::Configuration*)cfg->findSingle("environment_info");
  if (!acfg)
    error_quit("ERROR: Net::config_top_net(), missing ENVIRONMENT_INFO attribute; run dmlenv first!\n");
  else if(!s3f::dml::dmlConfig::isConf(acfg))
    error_quit("ERROR: Net::config_top_net(), invalid ENVIRONMENT_INFO attribute.\n");

  namesvc = new NameService;
  namesvc->config(acfg);
  assert(namesvc);

  curCidr = &namesvc->top_cidr_block;
}

void Net::finish_config_top_net(s3f::dml::Configuration* cfg)
{
  NET_DUMP(printf("finish_config_top_net().\n"));

  // configure traffic
  s3f::dml::Configuration* tcfg = (s3f::dml::Configuration*)cfg->findSingle("net.traffic");
  if(tcfg)
  {
    if(!s3f::dml::dmlConfig::isConf(tcfg))
      error_quit("ERROR: Net::config(), invalid NET.TRAFFIC attribute.\n");
    traffic = new Traffic(this);
    assert(traffic);
    traffic->config(tcfg);
    if(!netacc) netacc = new NetAccessory;
    netacc->addAttribute(NET_ACCESSORY_TRAFFIC, (void*)traffic);
  }
  else
	traffic = 0;

  // connect the links in the net
  connect_links();

  // load the forwarding tables if specified
  s3f::dml::Enumeration* fenum = cfg->find("forwarding_table");
  while(fenum->hasMoreElements())
  {
    s3f::dml::Configuration* fcfg = (s3f::dml::Configuration*)fenum->nextElement();
    if(!s3f::dml::dmlConfig::isConf(fcfg))
      error_quit("ERROR: Net::finish_config_top_net(), invalid FORWARDING_TABLE attribute.\n");
    load_fwdtable(fcfg);
  }
  delete fenum;
}

void Net::configLxcCommands(s3f::dml::Configuration* cfg)
{
	double TDF      = 0;
	long delay      = 0;

	string proxyNHI = "";
	string command  = "";

	s3f::dml::Configuration* lxcConfig;

	if (!myParent) lxcConfig = (s3f::dml::Configuration*)cfg->findSingle("net.lxcConfig");
	else           lxcConfig = (s3f::dml::Configuration*)cfg->findSingle("lxcConfig");

	if(lxcConfig)
	{
		//cout << "--" << nhi.toString() << "--" << id << "---\n";

		if(!s3f::dml::dmlConfig::isConf(lxcConfig))
			error_quit("ERROR: Net::configLxcCommands(), invalid NET.lxcConfig attribute.\n");

		// for each settings
		s3f::dml::Enumeration* ss = lxcConfig->find("settings");
		while(ss->hasMoreElements())
		{
			s3f::dml::Configuration* pcfg = (s3f::dml::Configuration*)ss->nextElement();
			if(!s3f::dml::dmlConfig::isConf(pcfg))
				error_quit("ERROR: illegal lxcConfig.settings attribute.\n");

			// ========================= NHI ============================
			char* lxcNHI = (char*)pcfg->findSingle("lxcNHI");
			if(lxcNHI)
			{
				if(s3f::dml::dmlConfig::isConf(lxcNHI))
					error_quit("ERROR: Net::configLxcCommands(), invalid traffic settings.lxcNHI attribute.\n");

				std::stringstream tempSS;
				if (myParent) tempSS << id << ":" << lxcNHI;	// non-topnet
				else          tempSS <<              lxcNHI;	// topnet

				proxyNHI = tempSS.str();
				//cout << proxyNHI << "\n";
			}
			else error_quit("need nhi\n");

			LXC_Proxy* proxy = sim_iface->get_timeline_interface()->lm->getLXCProxyWithNHI(proxyNHI);
			if (proxy == NULL)
			{
				cout << " NO PROXY\n";
				continue;
			}

			// ========================= TDF ============================
			char* lxcTDF = (char*)pcfg->findSingle("TDF");
			if(lxcTDF)
			{
				if(s3f::dml::dmlConfig::isConf(lxcTDF))
					error_quit("ERROR: Net::configLxcCommands(), invalid traffic settings.lxcNHI attribute.\n");
				TDF = atof(lxcTDF);
			}
			else error_quit("need TDF (though this can set default to 0.0)\n");

			// ========================= Command ============================
			char* lxcCmd = (char*)pcfg->findSingle("cmd");
			if(lxcCmd)
			{
				if(s3f::dml::dmlConfig::isConf(lxcCmd))
					error_quit("ERROR: Net::configLxcCommands(), invalid traffic settings.lxcNHI attribute.\n");
				command = string(lxcCmd);
			}
			else error_quit("need nhi\n");

			proxy->TDF = TDF;
			proxy->cmndToExec = command;

			// Check to see if command has NHI instead of IP. If so, figure out the IP and send the command
			// accordingly

			regmatch_t matches[1];
			regex_t exp;
			char* regularExpression = "([0-9]+:[0-9]+)";
			char left[1000];
			char right[1000];
			char nhi[30];
			const char* origCommand = command.c_str();

			regcomp(&exp, regularExpression, REG_EXTENDED);

			if (regexec(&exp, origCommand, 1, matches, 0) == 0)
			{
				int nhiBegin  = matches[0].rm_so;
				int nhiEnd    = matches[0].rm_eo;
				int nhiLength = matches[0].rm_eo - matches[0].rm_so;
				int leftLen   = matches[0].rm_so;
				int rightLen  = strlen(origCommand) - matches[0].rm_eo;

				memcpy(left, origCommand, leftLen);
				left[leftLen] = '\0';

				memcpy(right, origCommand + nhiEnd, rightLen);
				right[rightLen] = '\0';

				memcpy(nhi, origCommand + nhiBegin, nhiLength);
				nhi[nhiLength] = '\0';

				IPADDR peer = getNameService()->nhi2ip(string(nhi));
				char buffer[20];
				unsigned int convertedIP = htonl(peer);
				const char* result = inet_ntop(AF_INET, &convertedIP, buffer, sizeof(buffer));

				char newCommand[1000];
				sprintf(newCommand, "%s%s%s", left, result, right);

				proxy->cmndToExec = string(newCommand);
			}
		}
	}
}

void Net::load_fwdtable(s3f::dml::Configuration* cfg)
{
  char* str = (char*)cfg->findSingle("node_nhi");
  if (!str || s3f::dml::dmlConfig::isConf(str))
    error_quit("ERROR: Net::load_fwdtable(), missing or invalid FORWARDING_TABLE.NODE_NHI attribute.\n");
  S3FNET_STRING hostnhi = str;

  Host* host = namesvc->hostnhi2hostobj(hostnhi);
  assert(host);
  host->loadForwardingTable(cfg);
}

void Net::register_host(Host* host, const S3FNET_STRING& name)
{
  assert(host);

  //more host init activity can be put here
  NET_DUMP(printf("register_host(): name=\"%s\", nhi=\"%s\".\n", name.c_str(), host->nhi.toString()));
}

void Net::register_interface(NetworkInterface* iface, IPADDR ip_addr)
{
  assert(iface);

  NET_DUMP(printf("register_interface(): ip=\"%s\".\n", IPPrefix::ip2txt(ip_addr)));

  if( !(namesvc->get_ip2iface_map()->insert(S3FNET_MAKE_PAIR(ip_addr, iface))).second )
  {
	  error_quit("ERROR: Net::register_interface(), "
			  "duplicate registered network interface: IP=\"%s\"", IPPrefix::ip2txt(ip_addr));
  }
}

NetworkInterface* Net::nicnhi_to_obj(S3FNET_STRING nhi_str)
{
	return namesvc->nicnhi2iface(nhi_str);
}

int Net::control(int ctrltyp, void* ctrlmsg)
{
  switch(ctrltyp) {

  case NET_CTRL_GET_TRAFFIC:
    // only the top net has the right to specify traffic.
    if(myParent) return ((Net*)myParent)->control(NET_CTRL_GET_TRAFFIC, ctrlmsg);
    else
    {
      if(netacc)
      {
	      netacc->requestAttribute(NET_ACCESSORY_TRAFFIC, *((void**)ctrlmsg));
	      return 1;
      }
      else
      {
	      *((void**)ctrlmsg) = 0;
	      return 0;
      }
    }

  default:
    error_quit("ERROR: Unknown Net::control() command!\n");
    return 0;
  }
}

void NetAccessory::addAttribute(byte at, bool value)
{
  NetAttribute* new_attrib = new NetAttribute;
  new_attrib->name = at;
  new_attrib->value.bool_val = value;

  // put it in the list
  netAttribList.push_back(new_attrib);
}

void NetAccessory::addAttribute(byte at, int value)
{
  NetAttribute* new_attrib = new NetAttribute;
  new_attrib->name = at;
  new_attrib->value.int_val = value;

  // put it in the list
  netAttribList.push_back(new_attrib);
}

void NetAccessory::addAttribute(byte at, double value)
{
  NetAttribute* new_attrib = new NetAttribute;
  new_attrib->name = at;
  new_attrib->value.double_val = value;

  // put it in the list
  netAttribList.push_back(new_attrib);
}

void NetAccessory::addAttribute(byte at, void* value)
{
  NetAttribute* new_attrib = new NetAttribute;
  new_attrib->name = at;
  new_attrib->value.pointer_val = value;

  // put it in the list
  netAttribList.push_back(new_attrib);
}

bool NetAccessory::requestAttribute(byte at, bool& value)
{
  int size = netAttribList.size();
  for(int i = 0; i < size; i++)
  {
    if(netAttribList[i]->name == at)
    {
      value = netAttribList[i]->value.bool_val;
      return true;
    }
  }

  // if the attribute cannot be found, return 0.
  return false;
}

bool NetAccessory::requestAttribute(byte at, int& value)
{
  int size = netAttribList.size();
  for(int i = 0; i < size; i++)
  {
    if(netAttribList[i]->name == at)
    {
      value = netAttribList[i]->value.int_val;
      return true;
    }
  }

  // if the attribute cannot be found, return 0.
  return false;
}

bool NetAccessory::requestAttribute(byte at, double& value)
{
  int size = netAttribList.size();
  for(int i = 0; i < size; i++)
  {
    if(netAttribList[i]->name == at)
    {
      value = netAttribList[i]->value.double_val;
      return true;
    }
  }

  // if the attribute cannot be found, return 0.
  return false;
}

bool NetAccessory::requestAttribute(byte at, void*& value)
{
  int size = netAttribList.size();
  for(int i = 0; i < size; i++)
  {
    if(netAttribList[i]->name == at)
    {
      value = netAttribList[i]->value.pointer_val;
      return true;
    }
  }

  // if the attribute cannot be found, return 0.
  return false;
}

NetAccessory::~NetAccessory()
{
  unsigned size= netAttribList.size();
  for(unsigned i=0; i<size; i++)
    delete netAttribList[i];
}

NetAccessory::NetAttribute::~NetAttribute()
{
  switch (name)
  {
    case NET_ACCESSORY_TRAFFIC:
      delete (Traffic*)value.pointer_val;
      break;
    default:
      break; // do nothing for other types
  }
}

void Net::injectEmuEvent(Host* destinationHost, EmuPacket* pkt, unsigned int destIP)
{
	// Currently, S3Fnet does not need to specify a SRC IP. As a result, it is currently set to 1.
	// See dummy_session.cc

	LxcemuSession* sess = (LxcemuSession*)destinationHost->sessionForName(LXCEMU_PROTOCOL_NAME);
	sess->injectEvent(pkt, 1, destIP);
}

}; // namespace s3fnet
}; // namespace s3f

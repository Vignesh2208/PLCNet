/**
 * \file namesvc.cc
 * \brief Source file for the NameService class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "env/namesvc.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "net/host.h"
#include "net/network_interface.h"
#include "net/ip_prefix.h"
#include "util/errhandle.h"

namespace s3f {
namespace s3fnet {

#ifdef NAMESVC_DEBUG
#define NAMESVC_DUMP(x) printf("NAMESVC: "); x
#else
#define NAMESVC_DUMP(x)
#endif

NameService::NameService() :
  hostname2nicnhi_map(0), hostnhi2hostname_map(0), 
  hostnhi2align_map(0), hostnhi2id_map(0), 
  nhi2ip_map(0), ip2nicnhi_map(0), ip2iface_map(0)
{
  NAMESVC_DUMP(printf("new global name service\n"));
  ip2iface_map = new IP2IFACE_MAP;
}

NameService::~NameService()
{
  NAMESVC_DUMP(printf("delete global name service\n"));

  if(hostname2nicnhi_map) delete hostname2nicnhi_map;
  if(hostnhi2hostname_map) delete hostnhi2hostname_map;
  if(hostnhi2align_map) delete hostnhi2align_map;
  if(hostnhi2id_map) delete hostnhi2id_map;
  if(nhi2ip_map) delete nhi2ip_map;
  if(ip2nicnhi_map) delete ip2nicnhi_map;
  if(ip2iface_map) delete ip2iface_map;
}

void NameService::config(s3f::dml::Configuration* cfg)
{
  NAMESVC_DUMP(printf("config().\n"));

  assert(cfg);

  s3f::dml::Configuration* ncfg = (s3f::dml::Configuration*)cfg->findSingle("Net");
  if(ncfg)
  {
    if(!s3f::dml::dmlConfig::isConf(ncfg))
      error_quit("ERROR: NameService::config(), invalid ENVIRONMENT_INFO.NET attribute.\n");
    config_topnet_cidr(ncfg);
  }

  s3f::dml::Enumeration* henum = cfg->find("host");
  while(henum->hasMoreElements())
  {
    s3f::dml::Configuration* hcfg = (s3f::dml::Configuration*)henum->nextElement();
    if(!s3f::dml::dmlConfig::isConf(hcfg))
      error_quit("ERROR: NameService::config(), invalid ENVIRONMENT_INFO.HOST attribute.\n");
    config_hosts(hcfg);
  }
  delete henum;

  s3f::dml::Enumeration* aenum = cfg->find("alignment");
  while(aenum->hasMoreElements())
  {
    s3f::dml::Configuration* acfg = (s3f::dml::Configuration*)aenum->nextElement();
    if(!s3f::dml::dmlConfig::isConf(acfg))
      error_quit("ERROR: NameService::config(), invalid ENVIRONMENT_INFO.ALIGNMENT attribute.\n");
    config_nhi_addr(acfg);
  }
  delete aenum;
}

void NameService::config_topnet_cidr(s3f::dml::Configuration* cfg)
{
  NAMESVC_DUMP(printf("config_topnet_cidr().\n"));
  assert(cfg);

  s3f::dml::Enumeration* nenum = (s3f::dml::Enumeration*)cfg->find("Net");
  while(nenum->hasMoreElements())
  {
    s3f::dml::Configuration* ncfg = (s3f::dml::Configuration*)nenum->nextElement();
    if(!s3f::dml::dmlConfig::isConf(ncfg))
      error_quit("ERROR: NameService::config(), invalid ENVIRONMENT_INFO.NET.NET attribute.\n");

    char* str = (char*)ncfg->findSingle("id");
    if(!str || s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: NameService::config_topnet_cidr(), missing or invalid NET.NET.ID attribute in ENVIRONMENT_INFO.\n");
    long id = atol(str);
    NAMESVC_DUMP(printf("config_topnet_cidr(): subnet %ld.\n", id));

    CidrBlock* cidr = new CidrBlock();
    top_cidr_block.addSubCidrBlock(id, cidr);

    // do it recursively for all subnets
    config_net_cidr(ncfg, cidr);
  }
  delete nenum;
}

void NameService::config_net_cidr(s3f::dml::Configuration* cfg, CidrBlock* mycidr)
{
  NAMESVC_DUMP(printf("config_net_cidr().\n"));
  assert(cfg);

  char* str = (char*)cfg->findSingle("prefix_int");
  if(!str || s3f::dml::dmlConfig::isConf(str))
    error_quit("ERROR: NameService::config_net_cidr(), missing NET::PREFIX_INT attribute in ENVIRONMENT_INFO.\n");
  unsigned addr = (unsigned)atoi(str);

  str = (char*)cfg->findSingle("prefix_len");
  if(!str || s3f::dml::dmlConfig::isConf(str))
    error_quit("ERROR: NameService::config_net_cidr(), missing NET::PREFIX_LEN attribute in ENVIRONMENT_INFO.\n");
  int len = atoi(str);
  if(len < 1 || len > 32)
    error_quit("ERROR: NameService::config_net_cidr(), invalid NET::PREFIX_LEN (%d) in ENVIRONMENT_INFO\n", len);

  mycidr->setPrefix(addr, len);
  NAMESVC_DUMP(printf("config_net_cidr(): "); mycidr->getPrefix().display(); printf(".\n"));

  // do it recursively for all subnets
  s3f::dml::Enumeration* nenum = (s3f::dml::Enumeration*)cfg->find("Net");
  while (nenum->hasMoreElements())
  {
    s3f::dml::Configuration* ncfg = (s3f::dml::Configuration*)nenum->nextElement();
    if(!s3f::dml::dmlConfig::isConf(ncfg))
      error_quit("ERROR: NameService::config(), invalid ENVIRONMENT_INFO.NET...NET attribute.\n");

    str = (char*)ncfg->findSingle("id");
    if(!str || s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: NameService::config_topnet_cidr(), missing NET...NET.ID attribute in ENVIRONMENT_INFO.\n");
    long id = atol(str);
    NAMESVC_DUMP(printf("config_net_cidr(): subnet %ld.\n", id));

    CidrBlock* cidr = new CidrBlock();
    mycidr->addSubCidrBlock(id, cidr);

    config_net_cidr(ncfg, cidr);
  }
  delete nenum;
}

void NameService::config_hosts(s3f::dml::Configuration* cfg)
{
  assert(cfg);

  char* str = (char*)cfg->findSingle("name");
  if(!str || s3f::dml::dmlConfig::isConf(str))
    error_quit("ERROR: NameService::config_hosts(), missing or invalid HOST.NAME attribute in ENVIRONMENT_INFO.\n");
  S3FNET_STRING name = str; // copied implicitly

  str = (char*)cfg->findSingle("mapto");
  if(!str || s3f::dml::dmlConfig::isConf(str))
    error_quit("ERROR: NameService::config_hosts(), missing or invalid HOST.MAPTO attribute in ENVIRONMENT_INFO.\n");
  S3FNET_STRING nicnhi = str; // copied implicitly

  // take the NH portion of the NHI address
  int pos = nicnhi.find_first_of('(');
  if(pos <= 0)
    error_quit("ERROR: NameService::config_hosts(), invalid HOST.MAPTO attribute in ENVIRONMENT_INFO.\n");
  int pos2 = nicnhi.find_first_of(')', pos+1);
  if(pos2 <= 0)
    error_quit("ERROR: NameService::config_hosts(), invalid HOST.MAPTO attribute in ENVIRONMENT_INFO.\n");
  S3FNET_STRING hostnhi = nicnhi.substr(0, pos);

  int hostnicidx = atoi(nicnhi.substr(pos+1, pos2-pos+1).c_str());
  if(!hostnhi2id_map) hostnhi2id_map = new NHI2IDMAP;
  if(!(hostnhi2id_map->insert(S3FNET_MAKE_PAIR(hostnhi, hostnicidx))).second)
    error_quit("ERROR: NameService::config_host(),  duplicate host nhi \"%s\" in ENVIRONMENT_INFO.\n", hostnhi.c_str());

  // we only remember named hosts (and provide corresponding mapping)
  if(name != "no_name")
  {
    if(!hostname2nicnhi_map)
    {
      hostname2nicnhi_map = new STR2STRMAP;
      hostnhi2hostname_map = new STR2STRMAP;
    }

    if(!(hostname2nicnhi_map->insert(S3FNET_MAKE_PAIR(name, nicnhi))).second)
      error_quit("ERROR: NameService::config_host(), duplicate host name \"%s\" in ENVIRONMENT_INFO.\n", name.c_str());
    if(!(hostnhi2hostname_map->insert(S3FNET_MAKE_PAIR(hostnhi, name))).second)
      error_quit("ERROR: NameService::config_host(), duplicate nic nhi \"%s\" in ENVIRONMENT_INFO.\n", nicnhi.c_str());

    NAMESVC_DUMP(printf("config_host(): host \"%s\" <=> host nhi \"%s\", nic nhi \"%s\".\n", name.c_str(), hostnhi.c_str(), nicnhi.c_str()));
  }
  else
  {
    NAMESVC_DUMP(printf("config host: anonymous nhi \"%s\".\n", nicnhi.c_str()));
  }

  str = (char*)cfg->findSingle("alignment");
  if(!str || s3f::dml::dmlConfig::isConf(str))
    error_quit("ERROR: NameService::config_host(), missing or invalid HOST.ALIGNMENT attribute in ENVIRONMENT_INFO.\n");
  S3FNET_STRING align = str; // implicit copy

  if(!hostnhi2align_map) hostnhi2align_map = new STR2STRMAP;
  if(!(hostnhi2align_map->insert(S3FNET_MAKE_PAIR(hostnhi, align))).second)
    error_quit("ERROR: NameService::config_host(),  duplicate host nhi \"%s\" in ENVIRONMENT_INFO.\n", hostnhi.c_str());

  NAMESVC_DUMP(printf("config_host(): host nhi \"%s\" <=> alignment \"%s\".\n", hostnhi.c_str(), align.c_str()));
}

void NameService::config_nhi_addr(s3f::dml::Configuration* cfg)
{
  assert(cfg);

  if(!nhi2ip_map) nhi2ip_map = new NHI2IPMAP();
  if(!ip2nicnhi_map) ip2nicnhi_map = new IP2NHIMAP();

  s3f::dml::Enumeration* ienum = cfg->find("interface");
  while(ienum->hasMoreElements())
  {
    s3f::dml::Configuration* icfg = (s3f::dml::Configuration*)ienum->nextElement();
    if(!s3f::dml::dmlConfig::isConf(icfg))
      error_quit("ERROR: NameService::config_nhi_addr(), invalid ALIGNMENT.INTERFACE attribute in ENVIRONMENT_INFO.\n");

    char* str = (char*)icfg->findSingle("nhi");
    if(!str || s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: NameService::config_nhi_addr(), missing or invalid ALIGNMENT.INTERFACE.NHI attribute in ENVIRONMENT_INFO.\n");
    S3FNET_STRING nicnhi = str;
    int pos = nicnhi.find_first_of('(');
    if(pos <= 0)
      error_quit("ERROR: NameService::config_nhi_addr(), invalid ALIGNMENT_INTERFACE.NHI attribute in ENVIRONMENT_INFO.\n");
    int pos2 = nicnhi.find_first_of(')', pos+1);
    if(pos2 <= 0)
      error_quit("ERROR: NameService::config_nhi_addr(), invalid ALIGNMENT_INTERFACE.NHI attribute in ENVIRONMENT_INFO.\n");
    S3FNET_STRING hostnhi = nicnhi.substr(0, pos);
    int hostnicidx = atoi(nicnhi.substr(pos+1, pos2-pos+1).c_str());

    IPADDR ip;
    str = (char*)icfg->findSingle("ip");
    if(str)
    {
      if(s3f::dml::dmlConfig::isConf(str))error_quit("ERROR: NameService::config_nhi_addr(), invalid ALIGNMENT.INTERFACE.IP attribute in ENVIRONMENT_INFO.\n");
      ip = IPPrefix::txt2ip(str);
    }

    NHI2IPMAP::iterator iter = nhi2ip_map->find(nicnhi);
    if(iter != nhi2ip_map->end())
    {
      error_quit("ERROR: NameService::config_nhi_addr(), duplicate nic nhi \"%s\" in ENVIRONMENT_INFO.\n", nicnhi.c_str());
    }
    nhi2ip_map->insert(S3FNET_MAKE_PAIR(nicnhi, ip));
    ip2nicnhi_map->insert(S3FNET_MAKE_PAIR(ip, nicnhi));

    NAMESVC_DUMP(printf("config_nhi_addr(): nhi \"%s\" <=> ip \"%s\".\n", nicnhi.c_str(), IPPrefix::ip2txt(ip)));

    if(hostnhi2id_map)
    {
      NHI2IDMAP::iterator iter = hostnhi2id_map->find(hostnhi);
      if(iter != hostnhi2id_map->end() && hostnicidx == (*iter).second)
      {
        nhi2ip_map->insert(S3FNET_MAKE_PAIR(hostnhi, ip));
        NAMESVC_DUMP(printf("config_nhi_addr(): nhi \"%s\" <=> ip \"%s\".\n", hostnhi.c_str(), IPPrefix::ip2txt(ip)));
	  }
    }
  }
  delete ienum;
}

const char* NameService::hostname2align(const S3FNET_STRING& str)
{
  if(!hostname2nicnhi_map) return 0;
  else
  {
    STR2STRMAP::iterator iter = hostname2nicnhi_map->find(str);
    if(iter == hostname2nicnhi_map->end()) return 0;
    else return nhi2align((*iter).second);
  }
}

const char* NameService::nhi2align(const S3FNET_STRING& nhi)
{
  if(!hostnhi2align_map) return 0;

  // remove the interface specification in nhi
  S3FNET_STRING hostnhi;
  int pos = nhi.find_first_of('(');
  if(pos > 0) hostnhi = nhi.substr(0, pos);
  else hostnhi = nhi;

  STR2STRMAP::iterator iter = hostnhi2align_map->find(hostnhi);
  if(iter == hostnhi2align_map->end()) return 0;
  return (*iter).second.c_str();
}

IPADDR NameService::hostname2ip(const S3FNET_STRING& str)
{
  if(!hostname2nicnhi_map) return 0;
  else
  {
    STR2STRMAP::iterator iter = hostname2nicnhi_map->find(str);
    if(iter == hostname2nicnhi_map->end()) return 0;
    else return nhi2ip((*iter).second);
  }
}

IPADDR NameService::nhi2ip(const S3FNET_STRING& nhi)
{
  if(!nhi2ip_map) return IPADDR_INVALID;

  NHI2IPMAP::iterator iter = nhi2ip_map->find(nhi);
  if(iter == nhi2ip_map->end()) return IPADDR_INVALID;
  else return (*iter).second;
}

const char* NameService::nhi2hostname(const S3FNET_STRING& nhi)
{
  if(!hostnhi2hostname_map) return 0;

  // remove the interface specification in nhi
  S3FNET_STRING hostnhi;
  int pos = nhi.find_first_of('(');
  if(pos > 0) hostnhi = nhi.substr(0, pos);
  else hostnhi = nhi;

  STR2STRMAP::iterator iter = hostnhi2hostname_map->find(hostnhi);
  if(iter == hostnhi2hostname_map->end()) return 0;
  else return (*iter).second.c_str();
}

const char* NameService::ip2nicnhi(IPADDR addr)
{
  if(!ip2nicnhi_map) return 0;

  IP2NHIMAP::iterator iter = ip2nicnhi_map->find(addr);
  if(iter == ip2nicnhi_map->end()) return 0;
  else return (*iter).second.c_str();
}

const char* NameService::ip2align(IPADDR addr)
{
  if(!ip2nicnhi_map) return 0;

  IP2NHIMAP::iterator iter = ip2nicnhi_map->find(addr);
  if(iter == ip2nicnhi_map->end()) return 0;
  else return nhi2align((*iter).second);
}

bool NameService::netnhi2prefix(const char* str, IPPrefix& prefix)
{
  if(!str) return false;

  char* p= (char*)str;
  char* end = p;

  if(*end == '\0') return false;

  CidrBlock* cur_cidr = &top_cidr_block;
  for(;;)
  {
    long cur_id = strtol(p, &end, 10);
    CidrBlock *sub_cidr = cur_cidr->getSubCidrBlock(cur_id);
    if(!sub_cidr) return false;

    // move down.
    cur_cidr = sub_cidr;

    // check end condition
    if(*end == '\0')
    {
      prefix = cur_cidr->getPrefix();
      return true;
    }
    else if(*end != ':')
    {
      return false; // wrong format.
    }
    else
    {
      p = end+1;
      if(*p == '\0') return false; // wrong format.
    }
  }
}

bool NameService::netnhi2prefix(const S3FNET_STRING& nhi, IPPrefix& prefix)
{
  if(nhi.length() == 0) return false;
  else return netnhi2prefix(nhi.c_str(), prefix);
}

Mac48Address* NameService::ip2mac48(IPADDR addr)
{
	Mac48Address* mac48_addr = 0;

	NetworkInterface* nw_inf = ip2iface(addr);
	if(nw_inf != 0)
	{
		mac48_addr = nw_inf->getMac48Addr();
	}

	return mac48_addr;
}

NetworkInterface* NameService::ip2iface(IPADDR addr)
{
	if(!ip2iface_map) return 0;

	IP2IFACE_MAP::iterator iter = ip2iface_map->find(addr);
	if(iter == ip2iface_map->end()) return 0;
	else return ((*iter).second);
}

Host* NameService::ip2hostobj(IPADDR addr)
{
	NetworkInterface* iface = ip2iface(addr);

	if(!iface) return 0;
	else return iface->getHost();
}

NetworkInterface* NameService::nicnhi2iface(const S3FNET_STRING& nhi)
{
	IPADDR ip = nhi2ip(nhi);
	if(ip == IPADDR_INVALID) return 0;
	else return ip2iface(ip);
}

Host* NameService::hostnhi2hostobj(const S3FNET_STRING& nhi)
{
	IPADDR ip = nhi2ip(nhi);
	if(ip == IPADDR_INVALID) return 0;
	else return ip2hostobj(ip);
}

void NameService::print_ip2iface_map()
{
	printf("print_ip2iface_map()\n");
	IP2IFACE_MAP::iterator iter;
	for (iter = ip2iface_map->begin(); iter != ip2iface_map->end(); iter++)
	{
		printf("ip = %s, iface = %p\n",
				IPPrefix::ip2txt((IPADDR)((*iter).first)),
				(NetworkInterface*)((*iter).second));
	}
}

}; // namespace s3fnet
}; // namespace s3f

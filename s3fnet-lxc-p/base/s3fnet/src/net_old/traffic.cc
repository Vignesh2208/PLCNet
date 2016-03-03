/**
 * \file traffic.cc
 * \brief Source file for the Traffic class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "net/traffic.h"
#include "util/errhandle.h"
#include "net/host.h"
#include "net/net.h"

namespace s3f {
namespace s3fnet {

#define DEFAULT_PATTERNS 2
#define DEFAULT_SERVERS  4
#define DEFAULT_PORT     (-1)

#ifdef TRAFFIC_DEBUG
#define TRAFFIC_DUMP(x) printf("TRAFFIC: "); x
#else
#define TRAFFIC_DUMP(x)
#endif

Traffic::Traffic(Net* net) : topnet(net), patterns(0)
{
    TRAFFIC_DUMP(printf("new traffic \n"));
}

Traffic::~Traffic()
{
  // release traffic patterns when Traffic object is destroyed. 
  release_patterns();
}

void Traffic::release_patterns()
{
  if(!patterns) return;
  while(!patterns->empty()) {
    TrafficPattern* p = patterns->front();
    patterns->pop_front();
    delete p;
  }
  
  // finally, delete patterns
  delete patterns; patterns = 0;
}

void Traffic::config(s3f::dml::Configuration* cfg)
{
  // find out traffic patterns defined
  s3f::dml::Enumeration* ss = cfg->find(TRAFFIC_PATTERN);
  while(ss->hasMoreElements())
  {
    // create and populate a traffic pattern
    TrafficPattern* pattern = new TrafficPattern;
    assert(pattern);
    s3f::dml::Configuration* pcfg = (s3f::dml::Configuration*)ss->nextElement();
    if(!s3f::dml::dmlConfig::isConf(pcfg))
      error_quit("ERROR: illegal TRAFFIC.PATTERN attribute.\n");
    pattern->config(pcfg);
    if(!patterns)
    {
      patterns = new TRAFFIC_PATTERN_LIST;
      assert(patterns); 
    }
    // put the new pattern in the list
    patterns->push_back(pattern);
  }
  delete ss;
}

void Traffic::init()
{
  resolve_nhi();
}

void Traffic::resolve_nhi()
{
  // Now we will resolve each of the nhi's in the servers vector of
  // each pattern. This must be done after IP addresses have been
  // assigned to network interfaces.

  if(!patterns) return;

  for(TRAFFIC_PATTERN_LIST_ITERATOR it = patterns->begin(); it != patterns->end(); it++)
  {
    TrafficPattern* pattern = *it;
    
    // first, try to resolve the client's nhi address; we first try to
    // interpret the client address as a network nhi, if no, we try
    // machine nhi.
    bool is_success = false;

    int nhi_type = pattern->client.type;
    if(pattern->client.type & Nhi::NHI_NET)
    {
      pattern->client.type = Nhi::NHI_NET;
      if(topnet->nhi2obj(&pattern->client))
    	is_success = true;
    }

    if(!is_success)
    {
      pattern->client.type = nhi_type;
      if(pattern->client.type & Nhi::NHI_MACHINE)
      {
    	pattern->client.type = Nhi::NHI_MACHINE;
    	if(topnet->nhi2obj(&pattern->client))
    	  is_success = true;
      }
    }
  }
}

void Traffic::display(int indent)
{
  printf("%*cTraffic [\n", indent, ' ');
  indent += DML_OBJECT_INDENT;
  
  // Display Patterns
  if(patterns) {
    for(TRAFFIC_PATTERN_LIST_ITERATOR it = patterns->begin(); 
	it != patterns->end(); it++) {
      if(*it) (*it)->display(indent);
    }
  }
  
  printf("%*c]\n", indent-DML_OBJECT_INDENT, ' ');  
}

bool Traffic::getServers(Host* host, S3FNET_VECTOR(TrafficServerData*)& servers,
			 const char* list)
{
  if(!patterns) return false;

  bool found_match = false;
  for(TRAFFIC_PATTERN_LIST_ITERATOR it = patterns->begin(); 
      it != patterns->end(); it++) {
    TrafficPattern* pattern = *it;
    if(!pattern) continue;
    if(pattern->client.contains(host->nhi)) {
      for(unsigned j=0; j<pattern->servers.size(); j++) {
	if(!list || !pattern->servers[j]->list || 
	   !strcasecmp(list, pattern->servers[j]->list)) {
	  found_match = true;
	  servers.push_back(pattern->servers[j]);
	}
      }
    }
  }
  return found_match;
}

TrafficPattern::TrafficPattern() {}

TrafficPattern::~TrafficPattern()
{ 
  deallocate_servers();
}

void TrafficPattern::deallocate_servers()
{
  for(unsigned i=0; i<servers.size(); i++)
    delete servers[i];
  servers.clear();
}

void TrafficPattern::config(s3f::dml::Configuration* cfg)
{
  // configure the client for a traffic pattern. this is a must-set
  // attribute.
  char* client_string = (char*)cfg->findSingle(TRAFFIC_CLIENT);
  if(!client_string) {
    error_quit("ERROR: TrafficPattern::config(), "
	       "missing TRAFFIC.PATTERN.CLIENT attribute!\n");
  } else{
    if(s3f::dml::dmlConfig::isConf(client_string))
      error_quit("ERROR: TrafficPattern::config(), "
		 "invalid TRAFFIC.PATTERN.CLIENT attribute.\n");
    //client.convert(client_string, Nhi::NHI_MACHINE | Nhi::NHI_NET);
    client.convert(client_string, Nhi::NHI_MACHINE); //removed the Nhi:NHI_NET here, not sure if I missed anything
    TRAFFIC_DUMP(printf("client \"%s\":\n", client.toString()));
  }

  // configure the servers in a traffic pattern
  s3f::dml::Enumeration* ss = cfg->find(TRAFFIC_SERVERS);
  while(ss->hasMoreElements()) {
    s3f::dml::Configuration* scfg =
      (s3f::dml::Configuration*)ss->nextElement();
    if(!s3f::dml::dmlConfig::isConf(scfg))
      error_quit("ERROR: TrafficPattern::config(), "
		 "invalid TRAFFIC.PATTERN.SERVERS attribute.\n");
    configure_servers(scfg);
  }
  delete ss;
}

void TrafficPattern::configure_servers(s3f::dml::Configuration* cfg)
{
  // read servers like:
  //   servers [port 10 nhi 1:2(0)]
  //   servers [port 10 nhi_range [from 1:2(0) to 1:5(0)]]

  char* server_list_string = (char*)cfg->findSingle(TRAFFIC_SERVER_LIST);
  if(server_list_string)
  {
    if(s3f::dml::dmlConfig::isConf(server_list_string))
      error_quit("ERROR: TrafficPattern::configure_servers(), invalid traffic SERVERS.LIST attribute.\n");
    server_list_string = sstrdup(server_list_string);
  }

  int new_port = 0;
  char* server_port_string = (char*)cfg->findSingle(TRAFFIC_SERVER_PORT);
  if(server_port_string)
  {
    if(s3f::dml::dmlConfig::isConf(server_port_string))
      error_quit("ERROR: TrafficPattern::configure_servers(), invalid traffic SERVERS.PORT attribute.\n");
    new_port = atoi(server_port_string);
  }

  int new_sessions = 1;
  char* sessions_string = (char*)cfg->findSingle(TRAFFIC_SERVER_SESSIONS);
  if(sessions_string)
  {
    if(s3f::dml::dmlConfig::isConf(sessions_string))
      error_quit("ERROR: TrafficPattern::configure_servers(), invalid traffic SERVERS.SESSIONS attribute.\n");
    new_sessions = atoi(sessions_string);
  }

  s3f::dml::Enumeration* ss = cfg->find(TRAFFIC_SERVER_NHI);
  while(ss->hasMoreElements())
  {
    char* nhi_string = (char*)ss->nextElement();
    if(s3f::dml::dmlConfig::isConf(nhi_string))
      error_quit("ERROR: TrafficPattern::configure_servers(), invalid traffic SERVERS.NHI attribute.\n");
    Nhi* nhi = new Nhi;
    if(0 != nhi->convert(nhi_string, Nhi::NHI_INTERFACE))
    {
      error_quit("ERROR: TrafficPattern::configure_servers(), bad server nhi address: \"%s\".\n", nhi_string);
    }
    TrafficServerData* sd = new TrafficServerData(nhi, new_port, sstrdup(server_list_string),new_sessions);
    servers.push_back(sd);
  }
  delete ss;

  ss = cfg->find(TRAFFIC_SERVER_NHI_RANGE);
  while(ss->hasMoreElements())
  {
    s3f::dml::Configuration* rcfg = (s3f::dml::Configuration*)ss->nextElement();
    if(!s3f::dml::dmlConfig::isConf(rcfg))
      error_quit("ERROR: TrafficPattern::configure_servers(), invalid traffic SERVERS.NHI_RANGE attribute.\n");
    S3FNET_POINTER_VECTOR nhivector;
    if(0 != Nhi::readNhiRange(nhivector, rcfg))
    {
      error_quit("ERROR: TrafficPattern::configure_servers(), bad server nhi_range.\n");
    }
    for(unsigned i=0; i < nhivector.size(); i++)
    {
      TrafficServerData* sd = new TrafficServerData((Nhi*)nhivector[i], new_port, sstrdup(server_list_string), new_sessions);
      servers.push_back(sd);
    }
  }
  delete ss;

  if(server_list_string) delete[] server_list_string;
}

void TrafficPattern::display(int indent)
{
  char str[18];

  client.toString(str);

  printf("%*cPattern [\n", indent, ' ');
  indent += DML_OBJECT_INDENT;
  printf("%*cclient %s\n", indent, ' ', str);
  
  // Display servers
  printf("%*cservers [", indent, ' ');
  for(unsigned i = 0; i < servers.size(); i++)
  {
    printf("port %d nhi ", servers[i]->port);
    printf(" ");
  }

  printf("]\n%*c]\n", indent-DML_OBJECT_INDENT, ' ');
}

}; // namespace s3fnet
}; // namespace s3f

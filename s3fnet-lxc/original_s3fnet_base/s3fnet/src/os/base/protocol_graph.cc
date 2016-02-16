/**
 * \file protocol_graph.cc
 * \brief Source file for the ProtocolGraph class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "os/base/protocol_graph.h"
#include "util/errhandle.h"
#include "os/base/protocol_session.h"
#include "os/base/protocols.h"

namespace s3f {
namespace s3fnet {

void ProtocolGraph::config(s3f::dml::Configuration* cfg)
{
  // find 'description'
  char* desc = (char*)cfg->findSingle("description");
  if(desc) description = sstrdup(desc);

  // create & configure each 'ProtocolSession'
  s3f::dml::Enumeration* ss = cfg->find("ProtocolSession");
  while(ss->hasMoreElements())
  {
    s3f::dml::Configuration* scfg = (s3f::dml::Configuration*)ss->nextElement();
    if(!s3f::dml::dmlConfig::isConf(scfg))
      error_quit("ERROR: invalid GRAPH.ProtocolSession attribute.\n");
    config_session(scfg);
  }
  delete ss;

  // create & configure each '.global.session' if not yet created
  ss = cfg->find(".global.session");
  while(ss->hasMoreElements())
  {
    s3f::dml::Configuration* ds = (s3f::dml::Configuration*)ss->nextElement();
    if(!s3f::dml::dmlConfig::isConf(ds))
      error_quit("ERROR: invalid .GLOBAL.SESSION attribute.\n");
    char* dn = (char*)ds->findSingle("name");
    if(dn && s3f::dml::dmlConfig::isConf(dn))
      error_quit("ERROR: invalid .GLOBAL.SESSION.NAME attribute.\n");
    if(dn && pname_map.find(dn) != pname_map.end())
    {
      config_session(ds);
    }
  }
  delete ss;
}

void ProtocolGraph::init() 
{
  // Initialize each protocol session: the order in which protocol
  // sessions are initialized in this case is that the last 'session' 
  // defined in 'graph' attribute goes first, although it's after
  // those protocol sessions defined with '.global.session'.
  // The ordering should not be used by any protocol session!
  for(S3FNET_GRAPH_PSESS_VECTOR::reverse_iterator iter = protocol_list.rbegin();
      iter != protocol_list.rend(); iter++)
  {
    if(!(*iter)->initialized()) (*iter)->init();
  }
}

void ProtocolGraph::wrapup() 
{
  // wrapup each protocol session
  for(S3FNET_GRAPH_PSESS_VECTOR::reverse_iterator iter = protocol_list.rbegin();
      iter != protocol_list.rend(); iter++)
  {
    if(!(*iter)->wrapped_up()) (*iter)->wrapup();
  }
}

ProtocolSession* ProtocolGraph::sessionForName(char* pname) 
{
  // return protocol session of the given name or NULL if not found
  S3FNET_GRAPH_PROTONAME_MAP::iterator iter;
  iter = pname_map.find(pname);
  if(iter != pname_map.end()) return (*iter).second;
  else return 0;
}

ProtocolSession* ProtocolGraph::sessionForNumber(int pno)
{
  // return protocol session of the given name or NULL if not found
  S3FNET_GRAPH_PROTONUM_MAP::iterator iter = pno_map.find(pno);
  if(iter != pno_map.end()) return (*iter).second;
  else return 0;
}

ProtocolGraph::ProtocolGraph() : 
  // initialize the object accordingly
  description(0)
{}

ProtocolGraph::~ProtocolGraph() 
{
  // reclaim everything
  if(description) delete description;
  for(int i=protocol_list.size()-1; i>=0; i--)
  {
    ProtocolSession* ps = protocol_list[i];
    delete ps;
  }
}

ProtocolSession* ProtocolGraph::config_session(s3f::dml::Configuration* cfg)
{
  // find out session name from 'name' attribute
  char* str= (char*)cfg->findSingle("name");
  if(!str)
    error_quit("ERROR: session missing NAME attribute");
  if(s3f::dml::dmlConfig::isConf(str))
    error_quit("ERROR: invalid ProtocolSession.NAME.\n");
  S3FNET_STRING sessname = str;

  // find out class name from 'use' attribute
  char* classname = (char*)cfg->findSingle("use");
  if(!classname)
    error_quit("ERROR: session missing USE attribute");
  if(s3f::dml::dmlConfig::isConf(classname))
    error_quit("ERROR: invalid ProtocolSession.USE.\n");
  // classname is not copied from dml library, so it's temporary
  // valid until the next dml query!

  // create a new protocol session
  ProtocolSession* psess = Protocols::newInstance(classname, this);
  if(!psess)
  {
    error_quit("ERROR: failed to create protocol \"%s\".\n", classname);
  }

  // configure the protocol session
  psess->config(cfg);

  insert_session(psess);
  return psess;
}

void ProtocolGraph::insert_session(ProtocolSession* psess)
{
  S3FNET_PAIR(S3FNET_GRAPH_PROTONAME_MAP::iterator, bool) ret0 =
    pname_map.insert(S3FNET_MAKE_PAIR(S3FNET_STRING(psess->name), psess));
  if(!ret0.second)
  {
    if(psess->instantiation_type() == ProtocolSession::PROT_UNIQUE_INSTANCE)
      error_quit("ERROR: protocol session \"%s\" only allows unique instance.\n", psess->use);
    else if(psess->instantiation_type() == ProtocolSession::PROT_MULTIPLE_INSTANCES &&
	    strcasecmp((*ret0.first).second->use, psess->use))
      error_quit("ERROR: protocol session \"%s\" allows multiple instances of the same implementation.\n", psess->use);
  }

  int sno = psess->getProtocolNumber();
  S3FNET_PAIR(S3FNET_GRAPH_PROTONUM_MAP::iterator, bool) ret1 = pno_map.insert(S3FNET_MAKE_PAIR(sno, psess));
  if(!ret1.second)
  {
    if(psess->instantiation_type() == ProtocolSession::PROT_UNIQUE_INSTANCE)
      error_quit("ERROR: protocol session \"%s\" only allows unique instance.\n", psess->use);
    else if(psess->instantiation_type() == ProtocolSession::PROT_MULTIPLE_INSTANCES &&
	    strcasecmp((*ret1.first).second->use, psess->use))
      error_quit("ERROR: protocol session \"%s\" allows multiple instances of the same implementation.\n", psess->use);
  }

  protocol_list.push_back(psess);
}

}; // namespace s3fnet
}; // namespace s3f

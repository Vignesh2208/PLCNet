/**
 * \file protocol_session.cc
 * \brief Source file for the ProtocolSession class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "os/base/protocol_session.h"
#include "util/errhandle.h"
#include "net/host.h"

namespace s3f {
namespace s3fnet {

ProtocolCallbackActivation::ProtocolCallbackActivation(ProtocolSession* sess){
	session = sess;
}

ProtocolCallbackActivation::~ProtocolCallbackActivation(){}

void ProtocolSession::config(s3f::dml::Configuration* cfg)
{
  if(configured())
    error_quit("ERROR: duplicate calls to ProtocolSession::config(): "
	       "session name is \"%s\".\n", name);
  else stage = PSESS_CONFIGURED;

  char* myname = (char*)cfg->findSingle("name");
  if(myname && s3f::dml::dmlConfig::isConf(myname))
    error_quit("ERROR: ProtocolSession::config(), invalid NAME attribute.\n");
  if(myname) name = sstrdup(myname);
  else name = 0; // cannot do assertion because it may happen for a "default" interface

  char* myuse  = (char*)cfg->findSingle("use");
  if(myuse && s3f::dml::dmlConfig::isConf(myuse))
    error_quit("ERROR: ProtocolSession::config(), invalid USE attribute.\n");
  if(myuse)  use = sstrdup(myuse);
  else use = 0; // cannot do assertion because it may happen for a "default" interface
}

void ProtocolSession::init()
{
  // Make sure this init function at the base class should be called
  // by the derived function!
  if(!configured()) 
    error_quit("ERROR: ProtocolSession \"%s\" init() called before config(), "
	       "forgot to call base class's config() in your derived class?\n", name);
  else if(initialized())
    error_quit("ERROR: duplicate calls to ProtocolSession::init(): "
	       "session name is \"%s\".\n", name);
  else stage = PSESS_INITIALIZED;

  if(inHost()->getHostSeed() == 0) //host-level rng, i.e. same rng within one host
	  sess_rng = inHost()->getRandom();
  else
	  sess_rng = new Random::RNG();
}

void ProtocolSession::wrapup()
{
  // Make sure this init function at the base class should be called
  // by the derived function!
  if(!initialized()) 
    error_quit("ERROR: ProtocolSession \"%s\" wrapup() called before init(), "
	       "forgot to call base class's init() in your derived class?\n", name);
  else if(wrapped_up())
    error_quit("ERROR: duplicate calls to ProtocolSession::wrapup(): "
	       "session name is \"%s\".\n", name);
  else stage = PSESS_WRAPPED_UP;
}

int ProtocolSession::pushdown(Activation msg, ProtocolSession* hi_sess, void* extinfo, size_t extinfo_size)
{
  return push(msg, hi_sess, extinfo, extinfo_size);
}

int ProtocolSession::popup(Activation msg, ProtocolSession* lo_sess, void* extinfo, size_t extinfo_size)
{
  return pop(msg, lo_sess, extinfo, extinfo_size);
}

int ProtocolSession::push(Activation msg, ProtocolSession* hi_sess, void* extinfo, size_t extinfo_size)
{
  error_quit("ERROR: called ProtocolSession::push(), that should be overridden.\n");
}

int ProtocolSession::pop(Activation msg, ProtocolSession* lo_sess, void* extinfo, size_t extinfo_size)
{ 
  error_quit("ERROR: called ProtocolSession::pop(), that should be overridden.\n");
}

int ProtocolSession::control(int ctrltype, void* ctrlmsg, ProtocolSession* sess)
{
  switch(ctrltype) {
  case PSESS_CTRL_SESSION_IS_LOWEST:
    assert(ctrlmsg);
    *((bool*)ctrlmsg) = false;
    return 0;

  case PSESS_CTRL_SESSION_GET_INTERFACE:
    assert(ctrlmsg);
    *((NetworkInterface**)ctrlmsg) = 0;
    return 0;

  case PSESS_CTRL_SESSION_SET_PARENT:
  case PSESS_CTRL_SESSION_SET_CHILD:
    return 0;

  default:
    // all control types should be handled at this point; if not, we
    // should report the problem
    error_quit("ERROR: ProtocolSession::control(), "
	       "unknown control type: %d.\n", ctrltype);
  }
  return 0;
}

Host* ProtocolSession::inHost()
{
  return dynamic_cast<Host*>(ingraph);
}

ltime_t ProtocolSession::getNow()
{
  return inHost()->now();
}

char* ProtocolSession::getNowWithThousandSeparator()
{
	return inHost()->getNowWithThousandSeparator();
}


ProtocolSession::ProtocolSession(ProtocolGraph* graph) :
  name(0), use(0), ingraph(graph), sess_rng(0), stage(PSESS_CREATED)
{
  assert(graph);
}

ProtocolSession::~ProtocolSession()
{
  /* FIXME: we can't call inHost() here; what can we do?
  if(sess_rng) {
    int seed = inHost()->getHostSeed();
    if(seed) delete sess_rng; 
  }
  */
  if(name) delete[] name;
  if(use) delete[] use;
}

}; // namespace s3fnet
}; // namespace s3f

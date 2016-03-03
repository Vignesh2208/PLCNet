/**
 * \file dummy_session.cc
 * \brief Source file for the DummySession class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "os/dummy/dummy_session.h"
#include "os/dummy/dummy_message.h"
#include "os/ipv4/ip_session.h"
#include "util/errhandle.h" // defines error_quit() method
#include "net/host.h" // defines Host class
#include "os/base/protocols.h" // defines S3FNET_REGISTER_PROTOCOL macro
#include "os/ipv4/ip_interface.h" // defines IPPushOption and IPOptionToAbove classes
#include "net/net.h"
#include "env/namesvc.h"

#ifdef DUMMY_DEBUG
#define DUMMY_DUMP(x) printf("DUMMY: "); x
#else
#define DUMMY_DUMP(x)
#endif

namespace s3f {
namespace s3fnet {

S3FNET_REGISTER_PROTOCOL(DummySession, DUMMY_PROTOCOL_CLASSNAME);

DummySession::DummySession(ProtocolGraph* graph) : ProtocolSession(graph)
{
  // create your session-related variables here
  DUMMY_DUMP(printf("A dummy protocol session is created.\n"));
}

DummySession::~DummySession()
{
  // reclaim your session-related variables here
  DUMMY_DUMP(printf("A dummy protocol session is reclaimed.\n"));
  if(callback_proc) delete callback_proc;
}

void DummySession::config(s3f::dml::Configuration* cfg)
{
  // the same method at the parent class must be called
  ProtocolSession::config(cfg);

  // parameterize your protocol session from DML configuration
  char* str = (char*)cfg->findSingle("hello_message");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: DummySession::config(), invalid HELLO_MESSAGE attribute.");
    else hello_message = str;
  }
  else hello_message = "hello there";

  double hello_interval_double;
  str = (char*)cfg->findSingle("hello_interval");
  if(!str) hello_interval_double = 0;
	  //error_quit("ERROR: DummySession::config(), missing HELLO_INTERVAL attribute.");
  else if(s3f::dml::dmlConfig::isConf(str))
    error_quit("ERROR: DummySession::config(), invalid HELLO_INTERVAL attribute.");
  else hello_interval_double = atof(str);
  if(hello_interval_double < 0)
    error_quit("ERROR: DummySession::config(), HELLO_INTERVAL needs to be non-negative.");
  hello_interval = inHost()->d2t(hello_interval_double, 0);

  if(hello_interval > 0)
  {
	  str = (char*)cfg->findSingle("hello_peer_nhi");
	  if(str)
	  {
		if(s3f::dml::dmlConfig::isConf(str))
		  error_quit("ERROR: DummySession::config(), invalid HELLO_PEER_NHI attribute.");
		else hello_peer_nhi = str;
	  }
	  str = (char*)cfg->findSingle("hello_peer_ip");
	  if(str)
	  {
		if(s3f::dml::dmlConfig::isConf(str))
		  error_quit("ERROR: DummySession::config(), invalid HELLO_PEER_IP attribute.");
		else hello_peer_ip = str;
	  }
	  if(!hello_peer_nhi.empty() && !hello_peer_ip.empty())
		error_quit("ERROR: DummySession::config(), HELLO_PEER_NHI and HELLO_PEER_IP both defined.");
	  else if(hello_peer_nhi.empty() && hello_peer_ip.empty())
		error_quit("ERROR: DummySession::config(), missing HELLO_PEER_NHI and HELLO_PEER_IP attribute.");

	  DUMMY_DUMP(printf("A dummy session is configured in host of NHI=\"%s\":\n", inHost()->nhi.toString()););
	  DUMMY_DUMP(printf("=> hello message: \"%s\".\n", hello_message.c_str()););
	  if(!hello_peer_nhi.empty())
	  {
		  DUMMY_DUMP(printf("=> hello peer: nhi=\"%s\".\n", hello_peer_nhi.c_str()););
	  }
	  else
	  {
		  DUMMY_DUMP(printf("=> hello peer: ip=\"%s\".\n", hello_peer_ip.c_str()););
	  }

	  double jitter_double;
	  str = (char*)cfg->findSingle("jitter");
	  if(!str) jitter_double = 0;
	  else if(s3f::dml::dmlConfig::isConf(str))
	    error_quit("ERROR: DummySession::config(), invalid jitter attribute.");
	  else jitter_double = atof(str);
	  if(jitter_double < 0)
	    error_quit("ERROR: DummySession::config(), jitter needs to be non-negative.");
	  jitter = inHost()->d2t(jitter_double, 0);
	  DUMMY_DUMP(printf("jitter = %ld\n", jitter););
  }
  DUMMY_DUMP(printf("=> hello interval: %ld.\n", hello_interval););
}

void DummySession::init()
{
  // the same method at the parent class must be called
  ProtocolSession::init();

  // initialize the session-related variables here
  DUMMY_DUMP(printf("Dummy session is initialized.\n"));
  pkt_seq_num = 0;

  // we couldn't resolve the NHI to IP translation in config method,
  // but now we can, since name service finally becomes functional
  if(!hello_peer_nhi.empty())
  {
	Host* owner_host = inHost();
	Net* owner_net = owner_host->inNet();
	hello_peer = owner_net->getNameService()->nhi2ip(hello_peer_nhi);
  }
  else hello_peer = IPPrefix::txt2ip(hello_peer_ip.c_str());

  // similarly we couldn't resolve the IP layer until now
  ip_session = (IPSession*)inHost()->getNetworkLayerProtocol();
  if(!ip_session) error_quit("ERROR: can't find the IP layer; impossible!");

  Host* owner_host = inHost();
  callback_proc = new Process( (Entity *)owner_host, (void (s3f::Entity::*)(s3f::Activation))&DummySession::callback);

  //hello interval = 0 means the dummy session only listens to messages
  if(hello_interval == 0) return;

  ltime_t wait_time = (ltime_t)getRandom()->Uniform(hello_interval-jitter, hello_interval+jitter);

  //schedule the first hello message
  dcac = new ProtocolCallbackActivation(this);
  Activation ac (dcac);
  HandleCode h = owner_host->waitFor( callback_proc, ac, wait_time, owner_host->tie_breaking_seed);
}

void DummySession::callback(Activation ac)
{
  DummySession* ds = (DummySession*)((ProtocolCallbackActivation*)ac)->session;
  ds->callback_body(ac);
}

void DummySession::callback_body(Activation ac)
{
  DummyMessage* dmsg = new DummyMessage(hello_message);
  Activation dmsg_ac (dmsg);

  IPPushOption ipopt;
  ipopt.dst_ip = hello_peer;
  ipopt.src_ip = IPADDR_INADDR_ANY;
  ipopt.prot_id = S3FNET_PROTOCOL_TYPE_DUMMY;
  ipopt.ttl = DEFAULT_IP_TIMETOLIVE;

  DUMMY_DUMP(char s[32]; printf("[%s] Dummy session sends hello: \"%s\" to %s \n",
		  getNowWithThousandSeparator(), hello_message.c_str(),
		  IPPrefix::ip2txt(hello_peer, s)));

  ip_session->pushdown(dmsg_ac, this, (void*)&ipopt, sizeof(IPPushOption));

  // next hello interval time
  ltime_t wait_time = (ltime_t)getRandom()->Uniform(hello_interval - jitter, hello_interval + jitter);
  HandleCode h = inHost()->waitFor( callback_proc, ac, wait_time, inHost()->tie_breaking_seed );
}

int DummySession::control(int ctrltyp, void* ctrlmsg, ProtocolSession* sess)
{
  switch(ctrltyp)
  {
  	case DUMMY_CTRL_COMMAND1:
  	case DUMMY_CTRL_COMMAND2:
  	  return 0; // dummy control commands, we do nothing here
  	default:
  	  return ProtocolSession::control(ctrltyp, ctrlmsg, sess);
  }
}

int DummySession::push(Activation msg, ProtocolSession* hi_sess, void* extinfo, size_t extinfo_size)
{
  error_quit("ERROR: a message is pushed down to the dummy session from protocol layer above; it's impossible.\n");
  return 0;
}

int DummySession::pop(Activation msg, ProtocolSession* lo_sess, void* extinfo, size_t extinfo_size)
{
  DUMMY_DUMP(printf("A message is popped up to the dummy session from the IP layer.\n"));
  char* pkt_type = "dummy";

  //check if it is a DummyMessage
  ProtocolMessage* message = (ProtocolMessage*)msg;
  if(message->type() != S3FNET_PROTOCOL_TYPE_DUMMY)
  {
	  error_quit("ERROR: the message popup to dummy session is not S3FNET_PROTOCOL_TYPE_DUMMY.\n");
  }

  // the protocol message must be of DummyMessage type, and the extra info must be of IPOptionToAbove type
  DummyMessage* dmsg = (DummyMessage*)msg;
  IPOptionToAbove* ipopt = (IPOptionToAbove*)extinfo;

  DUMMY_DUMP(char buf1[32]; char buf2[32];
  	  	  	 printf("[%s] Dummy session receives hello: \"%s\" (ip_src=%s, ip_dest=%s)\n",
		     getNowWithThousandSeparator(), dmsg->hello_message.c_str(),
	         IPPrefix::ip2txt(ipopt->src_ip, buf1), IPPrefix::ip2txt(ipopt->dst_ip, buf2)); );

  dmsg->erase_all();

  // returning 0 indicates success
  return 0;
}

}; // namespace s3fnet
}; // namespace s3f

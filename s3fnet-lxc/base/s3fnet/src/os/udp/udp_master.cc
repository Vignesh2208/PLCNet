/**
 * \file udp_master.cc
 * \brief Source file for the UDPMaster class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "os/udp/udp_master.h"
#include "util/errhandle.h"
#include "net/host.h"
#include "os/base/protocols.h"
#include "os/udp/udp_message.h"
#include "os/udp/udp_session.h"
#include "os/ipv4/ip_message.h"
#include "os/ipv4/ip_interface.h"

#ifdef UDP_DEBUG
#define UDP_DUMP(x) printf("UDP: "); x
#else
#define UDP_DUMP(x)
#endif

namespace s3f {
namespace s3fnet {

S3FNET_REGISTER_PROTOCOL_WITH_ALIAS(UDPMaster, "S3F.OS.UDP", "S3F.OS.UDP.udpSessionMaster");

UDPMaster::UDPMaster(ProtocolGraph* graph) : SessionMaster(graph), ip_sess(0), max_datagram_size(INT_MAX)
{
  UDP_DUMP(printf("[host=\"%s\"] new udp master session.\n", inHost()->nhi.toString()));
}

UDPMaster::~UDPMaster()
{
  for(S3FNET_SET(UDPSession*)::iterator iter = udp_sessions.begin(); iter != udp_sessions.end(); iter++)
  {
    delete (*iter);
  }
}

void UDPMaster::config(s3f::dml::Configuration *cfg)
{
  SessionMaster::config(cfg);

  s3f::dml::Configuration* udpcfg = (s3f::dml::Configuration*)cfg->findSingle("udpinit");
  if(udpcfg)
  {
    if(!s3f::dml::dmlConfig::isConf(udpcfg))
      error_quit("ERROR: UDPMaster::config(), invalid UDPINIT attribute.\n");

    char* str = (char*)udpcfg->findSingle("max_datagram_size");
    if(str)
    {
      if(s3f::dml::dmlConfig::isConf(str))
    	  error_quit("ERROR: UDPMaster::config(), invalid UDPINIT.MAX_DATAGRAM_SIZE attribute.\n");

      max_datagram_size = atoi(str);
    }
  }

  UDP_DUMP(printf("[host=\"%s\"] config(): max_datagram_size=%d.\n",
		  inHost()->nhi.toString(), max_datagram_size));
}

void UDPMaster::init()
{ 
  UDP_DUMP(printf("[host=\"%s\"] init().\n", inHost()->nhi.toString()));

  SessionMaster::init();
  
  if(strcasecmp(name, UDP_PROTOCOL_NAME))
    error_quit("ERROR: UDPMaster::init(), unmatched protocol name: \"%s\", expecting \"UDP\".\n", name);

  // find out the socket layer protocol session
  sock_master = (SessionMaster*)inHost()->sessionForName(SOCKET_PROTOCOL_NAME);
  if(!sock_master) 
    error_quit("ERROR: UDPMaster::init(), missing socket layer.\n"); 

  // find out the IP layer protocol session
  ip_sess = inHost()->getNetworkLayerProtocol();
  assert(ip_sess);
}

int UDPMaster::control(int ctrltyp, void* ctrlmsg, ProtocolSession* sess)
{
  UDP_DUMP(printf("[host=\"%s\"] control(): type=%d.\n", inHost()->nhi.toString(), ctrltyp));
  switch(ctrltyp)
  {
  	case UDP_CTRL_GET_MAX_DATAGRAM_SIZE:
  	{
  	  *(int*)ctrlmsg = max_datagram_size;
  	  return 0;
  	}

    default:
      return ProtocolSession::control(ctrltyp, ctrlmsg, sess);
  }
}

int UDPMaster::push(Activation msg, ProtocolSession* hi_sess, void* extinfo, size_t extinfo_size)
{
  UDP_DUMP(printf("[host=\"%s\"] %s: push().\n", inHost()->nhi.toString(), getNowWithThousandSeparator()));

  return ip_sess->pushdown(msg, this, extinfo, extinfo_size);
}

int UDPMaster::pop(Activation msg, ProtocolSession* lo_sess, void* extinfo, size_t extinfo_size)
{
  IPOptionToAbove* ops  = (IPOptionToAbove*)extinfo;
  UDPMessage* udphdr = (UDPMessage*)msg;

  S3FNET_SET(UDPSession*)::iterator iter;
  for(iter = udp_sessions.begin(); iter != udp_sessions.end(); iter++)
  {
    if((*iter)->dst_ip   == ops->src_ip &&
       (*iter)->src_port == udphdr->dst_port &&
       (*iter)->dst_port == udphdr->src_port)
    {
      UDP_DUMP(char buf1[50]; char buf2[50];
	  printf("[host=\"%s\"] %s: pop(), found matching socket (src=\"%s:%d\", dst=\"%s:%d\").\n",
		      inHost()->nhi.toString(), getNowWithThousandSeparator(),
		      IPPrefix::ip2txt((*iter)->src_ip, buf1), (*iter)->src_port,
		      IPPrefix::ip2txt((*iter)->dst_ip, buf2), (*iter)->dst_port));

      (*iter)->receive(udphdr);

      return 0;
    }
  }

  // check if there are unbound sessions; an unbounded session has an
  // source ip of IPADDR_ANYDEST; all we need to do is to match the
  // port number
  for(iter = udp_sessions.begin(); iter != udp_sessions.end(); iter++)
  {
    if((*iter)->src_ip   == IPADDR_ANYDEST && (*iter)->src_port == udphdr->dst_port)
    {
      (*iter)->dst_ip = ops->src_ip;
      (*iter)->dst_port = udphdr->src_port;

      UDP_DUMP(char buf1[50]; char buf2[50];
	       printf("[host=\"%s\"] %s: pop(), found unbound socket (src=\"%s:%d\", dst=\"%s:%d\").\n",
		      inHost()->nhi.toString(), getNowWithThousandSeparator(),
		      IPPrefix::ip2txt((*iter)->src_ip, buf1), (*iter)->src_port,
		      IPPrefix::ip2txt((*iter)->dst_ip, buf2), (*iter)->dst_port));

      (*iter)->receive(udphdr);
      return 0;
    }
  }

  // create and send the destination unreachable ICMP message
  /*ProtocolSession* icmp_sess = inHost()->sessionForName("icmp");
  if(icmp_sess)
  {
    IPMessage* iphdr = (IPMessage*)packet->getProtocolMessageByType(S3FNET_PROTOCOL_TYPE_IPV4);
    assert(iphdr);
    // port unreachable error
    ICMPMessage* icmph = ICMPMessage::makeDestUnreach(3, iphdr);
    assert(icmph);
    ICMPCtrlSendOption sndopt(iphdr->dst_ip, iphdr->src_ip, icmph);
    icmp_sess->control(ICMP_CTRL_SEND_ICMP_MESSAGE, &sndopt, this);
    UDP_DUMP(char buf1[50]; char buf2[50];
    printf("[host=\"%s\"] %s: pop(), unmatched udp port (src=\"%s:%d\", dst=\"%s:%d\"); "
		    "sent icmp (destination unreachable).\n",
		  inHost()->nhi.toString(), getNowWithThousandSeparator(),
		  IPPrefix::ip2txt(ops->src_ip, buf1), udphdr->src_port,
		  IPPrefix::ip2txt(ops->dst_ip, buf2), udphdr->dst_port));
  }
  else
  {*/
    UDP_DUMP(char buf1[50]; char buf2[50];
    printf("[host=\"%s\"] %s: pop(), unmatched udp message (src=\"%s:%d\", dst=\"%s:%d\").\n",
		    inHost()->nhi.toString(), getNowWithThousandSeparator(),
		    IPPrefix::ip2txt(ops->src_ip, buf1), udphdr->src_port,
		    IPPrefix::ip2txt(ops->dst_ip, buf2), udphdr->dst_port));
  //}

  return 0;
}

SocketSession* UDPMaster::createSession(int sock)
{
  UDPSession* sess = new UDPSession(this, sock);
  udp_sessions.insert(sess);
  return sess;
}

void UDPMaster::deleteSession(UDPSession* session)
{
  assert(session);
  udp_sessions.erase(session);
  delete session;
}

}; // namespace s3fnet
}; // namespace s3f

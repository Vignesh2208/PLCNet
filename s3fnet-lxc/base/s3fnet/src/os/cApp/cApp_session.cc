#include "os/cApp/cApp_session.h"
#include "os/cApp/cApp_message.h"
#include "os/lxcemu/lxcemu_message.h"
#include "os/lxcemu/lxcemu_session.h"
#include "os/ipv4/ip_session.h"
#include "util/errhandle.h" // defines error_quit() method
#include "net/host.h" // defines Host class
#include "os/base/protocols.h" // defines S3FNET_REGISTER_PROTOCOL macro
#include "os/ipv4/ip_interface.h" // defines IPPushOption and IPOptionToAbove classes
#include "net/net.h"
#include "env/namesvc.h"

#ifdef CAPP_DEBUG
#define CAPP_DUMP(x) printf("CAPP: "); x
#else
#define CAPP_DUMP(x)
#endif


namespace s3f {
namespace s3fnet {


S3FNET_REGISTER_PROTOCOL(cAppSession, CAPP_PROTOCOL_CLASSNAME);

cAppSession::cAppSession(ProtocolGraph* graph) :
    ProtocolSession(graph) {
  // create your session-related variables here
  CAPP_DUMP(printf("A CAPP protocol session is created.\n"));
}

cAppSession::~cAppSession() {
  // reclaim your session-related variables here
  CAPP_DUMP(printf("A CAPP protocol session is reclaimed.\n"));
  if (callback_proc)
    delete callback_proc;
}

void cAppSession::config(s3f::dml::Configuration* cfg) {
  // the same method at the parent class must be called
  ProtocolSession::config(cfg);
  // parameterize your protocol session from DML configuration
}

void cAppSession::init() {
  // the same method at the parent class must be called
  ProtocolSession::init();

  // initialize the session-related variables here
  CAPP_DUMP(printf("cApp session is initialized.\n"));
  pkt_seq_num = 0;

  ip_session = (IPSession*) inHost()->getNetworkLayerProtocol();
  if (!ip_session)
    error_quit("ERROR: can't find the IP layer; impossible!");

  Host* owner_host = inHost();
  callback_proc = new Process((Entity *) owner_host,
      (void (s3f::Entity::*)(s3f::Activation))&cAppSession::callback);

  inject_attack_init();

}

void cAppSession::callback(Activation ac)
{
  //cAppSession* ds = (cAppSession*) ((ProtocolCallbackActivation*) ac)->session;
  //ds->callback_body(ac);

  cAppSession* sess = (cAppSession*)((cAppSessionCallbackActivation*)ac)->session;
  EmuPacket* packet = ((cAppSessionCallbackActivation*)ac)->packet;
  IPADDR dst_ip =   ((cAppSessionCallbackActivation*)ac)->dst_ip;
  IPADDR src_ip =   ((cAppSessionCallbackActivation*)ac)->src_ip;
  sess->callback_body(packet, src_ip, dst_ip);
}

void cAppSession::callback_body(EmuPacket* packet, IPADDR srcIP, IPADDR destIP)
{
  LxcemuMessage* dmsg = new LxcemuMessage();
  dmsg->ppkt = packet;

  Activation dmsg_ac(dmsg);

  IPPushOption ipopt;
  ipopt.dst_ip = destIP;
  ipopt.src_ip = srcIP;
  ipopt.prot_id = S3FNET_PROTOCOL_TYPE_LXCEMU;
  ipopt.ttl = DEFAULT_IP_TIMETOLIVE;
  ip_session->pushdown(dmsg_ac, this, (void*) &ipopt, sizeof(IPPushOption));

}

int cAppSession::control(int ctrltyp, void* ctrlmsg, ProtocolSession* sess) {
  
  return ProtocolSession::control(ctrltyp, ctrlmsg, sess);
}

int cAppSession::push(Activation msg, ProtocolSession* hi_sess, void* extinfo, size_t extinfo_size) {
  error_quit("ERROR: a message is pushed down to the cApp session from protocol layer above; it's impossible.\n");
  return 0;
}

void cAppSession::sendPacket(EmuPacket* packet, IPADDR srcIP, IPADDR destIP)
{
  cAppSessionCallbackActivation* oac = new cAppSessionCallbackActivation(this, destIP, srcIP, packet);
  Activation ac (oac);

  Host* owner_host = inHost();
  ltime_t wait_time = 0;  
  HandleCode h = owner_host->waitFor( callback_proc, ac, wait_time, owner_host->tie_breaking_seed);
}

int cAppSession::analyzePacket(char* pkt_ptr, int len, u_short * ethT, unsigned int* srcIP, unsigned int* dstIP)
{
  assert(ethT != NULL);
  sniff_ethernet* ether = (sniff_ethernet*)pkt_ptr;
  u_short ether_type    = ntohs(ether->ether_type);
  unsigned short sum;

  int ether_offset     = 0;
  int dstIPAddr_offset = 0;
  int srcIPAddr_offset = 0;
  struct icmphdr* hdr;
  struct iphdr* tk_ip_header;
  struct udphdr* tk_udp_header;
  struct tcphdr* tk_tcp_header;
  int is_tcp = 0;
  u_short udp_src_port;
  int parse_packet_type = PARSE_PACKET_SUCCESS_IP;

  (*ethT) = ether_type;

  switch(ether_type)
  {
    case ETHER_TYPE_IP:
      ether_offset = 14;
      dstIPAddr_offset = 16;
      srcIPAddr_offset = 12;
      hdr  = (struct icmphdr*)(pkt_ptr + ether_offset + 20);
      assert(hdr != NULL);
      
      tk_ip_header = (struct iphdr*)(pkt_ptr + ether_offset);
      if (tk_ip_header->protocol == 0x11) // UDP
      {
        tk_udp_header = (struct udphdr*)(pkt_ptr + ether_offset + 20);
        udp_src_port = ntohs(tk_udp_header->source);
        sum = (unsigned short)tk_udp_header->check;
        unsigned short udpLen = htons(tk_udp_header->len);
        
        if (udp_src_port == 68)
          return PACKET_PARSE_IGNORE_PACKET;
      }
      if (tk_ip_header->protocol == 0x06) // TCP
      {
        is_tcp = 1;
        tk_tcp_header = (struct tcphdr*)(pkt_ptr + ether_offset + 20);
      }
      parse_packet_type = PARSE_PACKET_SUCCESS_IP;
      break;

    case ETHER_TYPE_8021Q:
      return PACKET_PARSE_IGNORE_PACKET;      
      break;

    case ETHER_TYPE_ARP:
      ether_offset = 14;
      dstIPAddr_offset = 24;
      srcIPAddr_offset = 14;
      parse_packet_type = PARSE_PACKET_SUCCESS_ARP;
      break;

    case ETHER_TYPE_IPV6:
      return PACKET_PARSE_IGNORE_PACKET;
      break;

    default:
      fprintf(stderr, "Unknown ethernet type, %04x %d, skipping...\n", ether_type, ether_type );
      return PACKET_PARSE_IGNORE_PACKET;
      break;
  }

  char* ptrToIPAddr = (pkt_ptr + ether_offset + dstIPAddr_offset);
  *dstIP = *(unsigned int*)(ptrToIPAddr);
  char* ptrToSrcIPAddr = (pkt_ptr + ether_offset + srcIPAddr_offset);
  *srcIP = *(unsigned int*)(ptrToSrcIPAddr);   
}


int cAppSession::pop(Activation msg, ProtocolSession* lo_sess, void* extinfo,
    size_t extinfo_size) {

  
  CAPP_DUMP(printf("A message is popped up to the CAPP session from the IP layer.\n"));
  char* pkt_type = "CAPP";
  unsigned int srcIP;
  unsigned int dstIP;
  u_short ethType;

  cAppMessage* dmsg = (cAppMessage*) msg;
  IPOptionToAbove* ipopt = (IPOptionToAbove*) extinfo;
  EmuPacket* pkt = dmsg->ppkt;
  ProtocolMessage* message = (ProtocolMessage*) msg;
  /*if (message->type() != S3FNET_PROTOCOL_TYPE_CAPP) {
    error_quit(
        "ERROR: the message popup to cApp session is not S3FNET_PROTOCOL_TYPE_CAPP.\n");
  }*/

  //if(analyzePacket((char *)pkt->data,pkt->len,&ethType,&srcIP,&dstIP) != PACKET_PARSE_IGNORE_PACKET){
  //  inject_attack(pkt,srcIP,dstIP);
  //}

	inject_attack(pkt,ipopt->src_ip,ipopt->dst_ip);

  
  return 0;
}

cAppSessionCallbackActivation::cAppSessionCallbackActivation
(ProtocolSession* sess, IPADDR _dst_ip, IPADDR _src_ip, EmuPacket* ppkt) :
  ProtocolCallbackActivation(sess),
  dst_ip(_dst_ip),
  src_ip(_src_ip),
  packet(ppkt)
{}

};// namespace s3fnet
};// namespace s3f

/**
 * \file lxcemu_session.h
 * \brief Header file for the cAppSession class. Adapted from dummy_session.h
 *
 * authors : Vladimir Adam
 */

#ifndef __CAPP_SESSION_H__
#define __CAPP_SESSION_H__

#include "os/base/protocol_session.h"
#include "util/shstl.h"
#include "net/ip_prefix.h"
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

namespace s3f {
namespace s3fnet {

class IPSession;




/* Ethernet addresses are 6 bytes */

#define ETHER_ADDR_LEN  6
/* ethernet headers are 14 bytes */
#define SIZE_ETHERNET 14
#define ETHER_TYPE_IP    (0x0800)
#define ETHER_TYPE_ARP   (0x0806)
#define ETHER_TYPE_8021Q (0x8100)
#define ETHER_TYPE_IPV6  (0x86DD)
#define PACKET_SIZE 1600
#define PACKET_PARSE_SUCCESS        0
#define PACKET_PARSE_UNKNOWN_PACKET 1
#define PACKET_PARSE_IGNORE_PACKET  2
#define PARSE_PACKET_SUCCESS_IP     3
#define PARSE_PACKET_SUCCESS_ARP    4

/* Ethernet header */
typedef struct sniff_ethernet {
  u_char ether_dhost[ETHER_ADDR_LEN]; // Destination host address
  u_char ether_shost[ETHER_ADDR_LEN]; // Source host address
  u_short ether_type;             // IP? ARP? RARP? etc
} sniff_ethernet;

/**
 *  Storing the data for the callback function.
 *  In this case, the CAPPEventSession object, destination IP address and the emulation packet are stored.
 */
class cAppSessionCallbackActivation : public ProtocolCallbackActivation
{
  public:
  cAppSessionCallbackActivation(ProtocolSession* sess, IPADDR _dst_ip, IPADDR _src_ip, EmuPacket* ppkt);
  virtual ~cAppSessionCallbackActivation(){}

  IPADDR dst_ip; ///< the destination IP address
  IPADDR src_ip; ///< the source IP address
  EmuPacket* packet; ///< pointer to the emulation packet
};

#define CAPP_PROTOCOL_CLASSNAME "S3F.OS.CAPP"

/**
 * \brief A compromised Application (cApp) protocol session for injecting attacks.
 */
class cAppSession : public ProtocolSession {

 public:

  /* Do not modify these*/
  cAppSession(ProtocolGraph* graph);
  virtual ~cAppSession();
  virtual int getProtocolNumber() { return S3FNET_PROTOCOL_TYPE_CAPP; }
  virtual void config(s3f::dml::Configuration* cfg);
  virtual void init();
  virtual int control(int ctrltyp, void* ctrlmsg, ProtocolSession* sess);
  virtual int push(Activation msg, ProtocolSession* hi_sess, void* extinfo = 0, size_t extinfo_size = 0);
  virtual int pop(Activation msg, ProtocolSession* lo_sess, void* extinfo = 0, size_t extinfo_size = 0);
  Process* callback_proc;
  void callback(Activation ac);
  void callback_body(EmuPacket* packet, IPADDR srcIP, IPADDR destIP);
  void sendPacket(EmuPacket* packet, IPADDR srcIP, IPADDR destIP);
  int analyzePacket(char* pkt_ptr, int len, u_short * ethT, unsigned int* srcIP, unsigned int* dstIP);
  void inject_attack(EmuPacket * pkt, unsigned int srcIP, unsigned int destIP);
  void inject_attack_init(void);
  ProtocolCallbackActivation* dcac;
  IPSession* ip_session;
  int pkt_seq_num;

  /* Define any attack specific variables here. These must be initialized in inject_attack_init()
     DO NOT MODIFY ANY OF THE ABOVE STATEMENTS      
   */
  /************************************************************************************************/


  /************************************************************************************************/  

};



}; // namespace s3fnet
}; // namespace s3f

#endif /*__CAPP_SESSION_H__*/

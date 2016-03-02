/**
 * \file dummy_mac.cc
 * \brief Source file for the DummyMac class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "os/dummy_mac/dummy_mac.h"
#include "util/errhandle.h"
#include "net/network_interface.h"
#include "net/mac48_address.h"
#include "os/base/protocols.h"
#include "os/ipv4/ip_interface.h"
#include "os/serial/serial_message.h"
#include "net/forwardingtable.h"
#include "os/ipv4/ip_message.h"
#include "os/dummy_mac/dummy_mac_message.h"
#include "net/mac48_address.h"
#include "net/host.h"
#include "env/namesvc.h"

namespace s3f {
namespace s3fnet {

#ifdef DMAC_DEBUG
#define DMAC_DUMP(x) printf("DMAC: "); x
#else
#define DMAC_DUMP(x)
#endif

S3FNET_REGISTER_PROTOCOL(DummyMac, DUMMY_MAC_PROTOCOL_CLASSNAME);

DummyMac::DummyMac(ProtocolGraph* graph) : NicProtocolSession(graph)
{
  DMAC_DUMP(printf("[nic=\"%s\"] new dummy_mac session.\n", ((NetworkInterface*)inGraph())->nhi.toString()));
}

DummyMac::~DummyMac() 
{
  DMAC_DUMP(printf("[nic=\"%s\"] delete dummy_mac session.\n", ((NetworkInterface*)inGraph())->nhi.toString()));
}

void DummyMac::init()
{  
  DMAC_DUMP(printf("dummy mac init().\n"));  
  NicProtocolSession::init(); 

  if(strcasecmp(name, MAC_PROTOCOL_NAME))
    error_quit("ERROR: DummyMac::init(), unmatched protocol name: \"%s\", expecting \"MAC\".\n", name);
}

int DummyMac::push(Activation msg, ProtocolSession* hi_sess, void* extinfo, size_t extinfo_size)
{

  DMAC_DUMP(printf("Dummy mac push\n"));
  
  
  DummyMacMessage* dummy_mac_header = new DummyMacMessage();
  dummy_mac_header->carryPayload((SerialMessage*)msg);
  Activation dummy_mac_hdr(dummy_mac_header);

  if(!child_prot)
    error_quit("ERROR: DummyMac::push(), child protocol session has not been set.\n");
  
  DMAC_DUMP(printf("Dummy mac pushed to simple phy\n"));

  return child_prot->pushdown(dummy_mac_hdr, this);
}

int DummyMac::pop(Activation msg, ProtocolSession* lo_sess, void* extinfo, size_t extinfo_size)
{  
  DMAC_DUMP(printf("Dummy mac pop\n"));
  
  DummyMacMessage* mac_hdr = (DummyMacMessage*)msg;

  
  if(!parent_prot)
  {
    error_quit("ERROR: DummyMac::pop(), parent protocol session has not been set.\n");
  }

  //send to upper layer, currently it is the serial layer
  SerialMessage* payload = (SerialMessage *)mac_hdr->dropPayload();
  assert(payload);
  Activation serial_msg(payload);
  mac_hdr->erase(); //delete MAC header

  return parent_prot->popup(serial_msg, this);
}

}; // namespace s3fnet
}; // namespace s3f

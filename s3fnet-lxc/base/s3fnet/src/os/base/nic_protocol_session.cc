/**
 * \file nic_protocol_session.cc
 * \brief Source file for the NicProtocolSession class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "os/base/nic_protocol_session.h"
#include "net/network_interface.h"

namespace s3f {
namespace s3fnet {

class Host;

int NicProtocolSession::control(int ctrltyp, void* ctrlmsg, ProtocolSession* sess)
{
  switch(ctrltyp) {
  case PSESS_CTRL_SESSION_GET_INTERFACE:
    assert(ctrlmsg);
    *((NetworkInterface**)ctrlmsg) = getNetworkInterface();
    return 0;

  case PSESS_CTRL_SESSION_SET_PARENT:
    parent_prot = sess;
    return 0;

  case PSESS_CTRL_SESSION_SET_CHILD:
    child_prot = sess;
    return 0;

  default:
    return ProtocolSession::control(ctrltyp, ctrlmsg, sess);
  }
}

NetworkInterface* NicProtocolSession::getNetworkInterface()
{
  return dynamic_cast<NetworkInterface*>(ingraph);
}

Host* NicProtocolSession::inHost()
{
  return getNetworkInterface()->getHost();
}

}; // namespace s3fnet
}; // namespace s3f

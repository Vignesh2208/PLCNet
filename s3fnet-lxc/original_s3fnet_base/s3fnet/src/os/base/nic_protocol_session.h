/**
 * \file nic_protocol_session.h
 * \brief Header file for the NicProtocolSession class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __NIC_PROTOCOL_SESSION_H__
#define __NIC_PROTOCOL_SESSION_H__

#include "os/base/protocol_session.h"

namespace s3f {
namespace s3fnet {

class NetworkInterface;
class Mac48Address;

/**
 * \brief A protocol session in the protocol stack of a network
 * interface.
 *
 * The NicProtocolSession class is the base class for protocol
 * sessions in a network interface. This class handles some
 * interface-specific control types.
 */
class NicProtocolSession : public ProtocolSession {
 public:
  /** The constructor. */
  NicProtocolSession(ProtocolGraph* graph) : 
    ProtocolSession(graph), parent_prot(0), child_prot(0) {}

  /** Handle the control messages. */
  virtual int control(int ctrltyp, void* ctrlmsg, ProtocolSession* sess);

  /** Return a pointer to the owner interface. */
  NetworkInterface* getNetworkInterface();

  /** Return a pointer to the host owning this protocol session. */
  virtual Host* inHost();

 protected:
  /** The pointer to the upper layer. */
  ProtocolSession* parent_prot;

  /** The pointer to the lower protocol session. */
  ProtocolSession* child_prot;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__NIC_PROTOCOL_SESSION_H__*/

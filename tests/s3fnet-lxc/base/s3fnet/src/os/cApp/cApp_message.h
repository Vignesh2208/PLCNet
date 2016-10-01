/**
 * \file cApp.h
 * \brief Header file for the compromised Application class
 *
 * authors : Vignesh Babu
 */

#ifndef __CAPP_MESSAGE_H__
#define __CAPP_MESSAGE_H__

#include "os/base/protocol_message.h"
#include "util/shstl.h"
#include "s3fnet.h"

namespace s3f {
namespace s3fnet {

/**
 * \brief Message for the lxcemu protocol (sending hello message to the specific destination).
 *
 */

class cAppMessage : public ProtocolMessage {
 public:
  /** the default constructor */
  cAppMessage();

  /** the copy constructor */
  cAppMessage(const cAppMessage& msg);

 protected:
  /** The destructor is protected to avoid direct deletion;
   * erase() should be called upon reclaiming a protocol message
   */
  virtual ~cAppMessage();

 public:
  /** Clone a cAppMessage */
  virtual ProtocolMessage* clone() { printf("cAppMessage cloned\n"); return new cAppMessage(*this); }

  /** The unique protocol type of the lxcemu protocol.
   *  Each protocol message must have a unique type.
   */
  virtual int type() { return S3FNET_PROTOCOL_TYPE_CAPP; }

  /** Return the buffer size needed to serialize this protocol message (not used yet) */
  virtual int packingSize();

  /** Return the number of bytes used by the protocol message on a real network */
  virtual int realByteCount();

  // Packet containing payload
  EmuPacket* ppkt;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__CAPP_MESSAGE_H__*/

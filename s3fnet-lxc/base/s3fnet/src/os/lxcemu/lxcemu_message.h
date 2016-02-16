/**
 * \file lxcemu_message.h
 * \brief Header file for the LxcemuMessage class. Adapted from dummy_message.h
 *
 * authors : Vladimir Adam
 */

#ifndef __LXCEMU_MESSAGE_H__
#define __LXCEMU_MESSAGE_H__

#include "os/base/protocol_message.h"
#include "util/shstl.h"
#include "s3fnet.h"

namespace s3f {
namespace s3fnet {

/**
 * \brief Message for the lxcemu protocol (sending hello message to the specific destination).
 *
 */

class LxcemuMessage : public ProtocolMessage {
 public:
  /** the default constructor */
  LxcemuMessage();

  /** the copy constructor */
  LxcemuMessage(const LxcemuMessage& msg);

 protected:
  /** The destructor is protected to avoid direct deletion;
   * erase() should be called upon reclaiming a protocol message
   */
  virtual ~LxcemuMessage();

 public:
  /** Clone a LxcemuMessage */
  virtual ProtocolMessage* clone() { printf("LxcemuMessage cloned\n"); return new LxcemuMessage(*this); }

  /** The unique protocol type of the lxcemu protocol.
   *  Each protocol message must have a unique type.
   */
  virtual int type() { return S3FNET_PROTOCOL_TYPE_LXCEMU; }

  /** Return the buffer size needed to serialize this protocol message (not used yet) */
  virtual int packingSize();

  /** Return the number of bytes used by the protocol message on a real network */
  virtual int realByteCount();

  // Packet containing payload
  EmuPacket* ppkt;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__LXCEMU_MESSAGE_H__*/

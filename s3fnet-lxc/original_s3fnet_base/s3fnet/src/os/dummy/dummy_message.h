/**
 * \file dummy_message.h
 * \brief Header file for the DummyMessage class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __DUMMY_MESSAGE_H__
#define __DUMMY_MESSAGE_H__

#include "os/base/protocol_message.h"
#include "util/shstl.h"
#include "s3fnet.h"

namespace s3f {
namespace s3fnet {

/**
 * \brief Message for the dummy protocol (sending hello message to the specific destination).
 *
 */

class DummyMessage : public ProtocolMessage {
 public:
  /** the default constructor */
  DummyMessage();

  /** a constructor with a given hello message */
  DummyMessage(const S3FNET_STRING& msg);

  /** the copy constructor */
  DummyMessage(const DummyMessage& msg);

 protected:
  /** The destructor is protected to avoid direct deletion;
   * erase() should be called upon reclaiming a protocol message
   */
  virtual ~DummyMessage();

 public:
  /** Clone a DummyMessage */
  virtual ProtocolMessage* clone() { printf("DummyMessage cloned\n"); return new DummyMessage(*this); }

  /** The unique protocol type of the dummy protocol.
   *  Each protocol message must have a unique type.
   */
  virtual int type() { return S3FNET_PROTOCOL_TYPE_DUMMY; }

  /** Return the buffer size needed to serialize this protocol message (not used yet) */
  virtual int packingSize();

  /** Return the number of bytes used by the protocol message on a real network */
  virtual int realByteCount();

  /** The hello message of the dummy protocol */
  S3FNET_STRING hello_message;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__DUMMY_MESSAGE_H__*/

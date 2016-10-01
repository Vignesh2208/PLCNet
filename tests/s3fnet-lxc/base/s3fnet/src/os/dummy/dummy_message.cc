/**
 * \file dummy_message.cc
 * \brief Source file for the DummyMessage class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "os/dummy/dummy_message.h"

namespace s3f {
namespace s3fnet {

S3FNET_REGISTER_MESSAGE(DummyMessage, S3FNET_PROTOCOL_TYPE_DUMMY);

DummyMessage::DummyMessage() {}

DummyMessage::DummyMessage(const S3FNET_STRING& msg) :
  hello_message(msg) {}

DummyMessage::DummyMessage(const DummyMessage& msg) :
  ProtocolMessage(msg), // the base class's copy constructor must be called
  hello_message(msg.hello_message) {}

DummyMessage::~DummyMessage(){}

int DummyMessage::packingSize()
{
  // must add the parent class packing size
  int mysiz = ProtocolMessage::packingSize();

  // add the length of the message string (including the terminating null)
  mysiz += hello_message.length()+1;
  return mysiz;
}

int DummyMessage::realByteCount()
{
	return hello_message.length();
}

}; // namespace s3fnet
}; // namespace s3f

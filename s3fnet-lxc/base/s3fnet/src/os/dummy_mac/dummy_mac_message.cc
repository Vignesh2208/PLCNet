/**
 * \file dummy_mac_message.cc
 * \brief Source file for the DummyMacMessage class.
 *
 * authors : Vignesh Babu
 */

#include "os/dummy_mac/dummy_mac_message.h"
#include "net/mac48_address.h"

namespace s3f {
namespace s3fnet {

DummyMacMessage::DummyMacMessage() : 
{}


DummyMacMessage::DummyMacMessage(const DummyMacMessage& msg) :
		  ProtocolMessage(msg) // important!!!	  
{}

ProtocolMessage* DummyMacMessage::clone()
{
	printf ("ProtocolMessage* DummyMacMessage::clone()\n");
	return new DummyMacMessage(*this);
}

DummyMacMessage::~DummyMacMessage()
{}

S3FNET_REGISTER_MESSAGE(DummyMacMessage, S3FNET_PROTOCOL_TYPE_DUMMY_MAC);

}; // namespace s3fnet
}; // namespace s3f

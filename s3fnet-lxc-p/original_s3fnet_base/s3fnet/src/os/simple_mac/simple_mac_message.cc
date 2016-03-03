/**
 * \file simple_mac_message.cc
 * \brief Source file for the SimpleMacMessage class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "os/simple_mac/simple_mac_message.h"
#include "net/mac48_address.h"

namespace s3f {
namespace s3fnet {

SimpleMacMessage::SimpleMacMessage() : 
		  src(0), dest(0), src48(NULL), dst48(NULL) {}

SimpleMacMessage::SimpleMacMessage(MACADDR _src, MACADDR _dst) :
		  src(_src), dest(_dst), src48(NULL), dst48(NULL) {}

SimpleMacMessage::SimpleMacMessage(Mac48Address* _src, Mac48Address* _dst) :
		  src(0), dest(0){
	src48 = new Mac48Address();
	src48->CopyFrom(_src);
	dst48 = new Mac48Address();
	dst48->CopyFrom(_dst);
}

SimpleMacMessage::SimpleMacMessage(const SimpleMacMessage& msg) :
		  ProtocolMessage(msg), // important!!!
		  src(msg.src), dest(msg.dest)
{
	src48 = new Mac48Address();
	src48->CopyFrom(msg.src48);
	dst48 = new Mac48Address();
	dst48->CopyFrom(msg.dst48);
}

ProtocolMessage* SimpleMacMessage::clone()
{
	printf ("ProtocolMessage* SimpleMacMessage::clone()\n");
	return new SimpleMacMessage(*this);
}

SimpleMacMessage::~SimpleMacMessage()
{
	if(src48) delete src48;
	if(dst48) delete dst48;
}

S3FNET_REGISTER_MESSAGE(SimpleMacMessage, S3FNET_PROTOCOL_TYPE_SIMPLE_MAC);

}; // namespace s3fnet
}; // namespace s3f

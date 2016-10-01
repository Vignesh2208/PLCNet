/**
 * \file cApp_message.cc
 * \brief Source file for the cAppMessage class.
 *
 * authors : Vignesh Babu
 */


#include "os/cApp/cApp_message.h"

namespace s3f {
namespace s3fnet {

S3FNET_REGISTER_MESSAGE(cAppMessage, S3FNET_PROTOCOL_TYPE_CAPP);

cAppMessage::cAppMessage()
{
	ppkt = NULL;
}

cAppMessage::cAppMessage(const cAppMessage& msg) :
  ProtocolMessage(msg)
{
	EmuPacket* original = msg.ppkt;
	ppkt = original->duplicate();
	printf("cAppMessage Unplanned Copy Constructor");
	assert(false);

}

cAppMessage::~cAppMessage(){}

int cAppMessage::packingSize()
{
  // must add the parent class packing size
  int mysiz = ProtocolMessage::packingSize();

  assert(false);

  return mysiz + sizeof(EmuPacket*);
}

int cAppMessage::realByteCount()
{
	assert(ppkt != NULL);
	assert(ppkt->ethernetType != 0);

	int realBytes = 0;

	// TODO:Double check this since S3FNet apparently does not take
	// first 14 bytes when calculating transmission time.
	switch(ppkt->ethernetType)
	{
		case 0x0800:	// IP
			realBytes = ppkt->len - 20 - 14 - 14;
			break;
		case 0x0806: // ARP
			realBytes = 8; // since s3f does not count MAC during transmission, ARP packet = 42 bytes - 14 = 28
			// since we are simulating IP layer, = 28. This is so that S3FNet thinks the packet is 42 bytes
			break;
		default:
			fprintf(stderr, "cAppMessage::realByteCount() - unhandled ethernet type\n");
			assert(false);
	}

	assert(realBytes >= 0);
	return realBytes;
}

}; // namespace s3fnet
}; // namespace s3f

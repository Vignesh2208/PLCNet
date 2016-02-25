/**
 * \file serial_message.cc
 * \brief Source file for the SerialMessage class.
 *
 * authors : Vignesh Babu
 */

#include "os/serial/serial_message.h"



namespace s3f {
namespace s3fnet {

SerialMessage::SerialMessage()
{
	int i = 0;
	for(i = 0; i < KERN_BUF_SIZE; i++){
		src_lxcName[i] = '\0';
	}
	data = NULL;
	command = 0;
	length = 0;

}

SerialMessage::SerialMessage(const SerialMessage& iph) :
  ProtocolMessage(iph)
{
	strcpy(src_lxcName, iph.src_lxcName);
	data = iph.data;
	command = iph.command;
	length = iph.length;
}

ProtocolMessage* SerialMessage::clone()
{
  printf("SerialMessage cloned()\n");
  return new SerialMessage(*this);
}

SerialMessage::~SerialMessage(){}

S3FNET_REGISTER_MESSAGE(SerialMessage, S3FNET_PROTOCOL_TYPE_SERIAL);

}; // namespace s3fnet
}; // namespace s3f

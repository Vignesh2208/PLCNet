/**
 * \file protocol_message.cc
 * \brief Source file for the ProtocolMessage class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "os/base/protocol_message.h"
#include "util/errhandle.h"

namespace s3f {
namespace s3fnet {

ProtocolMessage::ProtocolMessage() : next(0) {}

ProtocolMessage::ProtocolMessage(const ProtocolMessage& pmsg)
{
  // copy the payload as well
  if(pmsg.next)
	  next = pmsg.next->clone();
  else
	  next = 0;
}

ProtocolMessage::~ProtocolMessage(){}

void ProtocolMessage::carryPayload(ProtocolMessage* load)
{
  // adjust the links, warning if found dangling nodes
  if(next)
  {
    error_retn("WARNING: ProtocolMessage::carryPayload(), overwrite pre-exist payload (may cause memory leak)!\n");
    delete next;
  }
  next = load;
}

ProtocolMessage* ProtocolMessage::dropPayload() 
{
  // sever the link between this node and the payload and return it
  ProtocolMessage* pmsg = next;
  next = 0;
  return pmsg;
}

void ProtocolMessage::erase()
{
  //printf("ProtocolMessage erased, type = %d\n", this->type());
  delete this;
}

void ProtocolMessage::erase_all()
{
	if(this->next) this->next->erase_all();

    //printf("ProtocolMessage deleted, type = %d\n", this->type());
	delete this;
}

int ProtocolMessage::totalPackingSize()
{
  // add them all
  if(next)
	  return packingSize()+next->totalPackingSize();
  else
	  return packingSize();
}

int ProtocolMessage::packingSize()
{
  return sizeof(int32);
}

int ProtocolMessage::totalRealBytes()
{
  // add them all
  if(next)
	  return realByteCount()+next->totalRealBytes();
  else
	  return realByteCount();
}

ProtocolMessage* ProtocolMessage::getMessageByType(int t)
{
  ProtocolMessage* cur = this;
  while(cur)
  {
    if(cur->type() == t) return cur;
    cur = cur->next;
  }
  return 0;
}

S3FNET_INT2PTR_MAP* ProtocolMessage::registered_messages = 0;

int ProtocolMessage::registerMessage(ProtocolMessage* (*fct)(), int type)
{
  // allocate the global data structure if it's not there
  if(!registered_messages)
  {
    registered_messages = new S3FNET_INT2PTR_MAP;
    assert(registered_messages);
  }

  // insert the mapping; check if duplicated
  if(!registered_messages->insert(S3FNET_MAKE_PAIR(type, (void*)fct)).second)
    error_quit("ERROR: duplicate protocol message types: %d\n", type);

  // we need to return something so that S3FNET_REGISTER_MESSAGE macro works
  return 0;
}

}; // namespace s3fnet
}; // namespace s3f


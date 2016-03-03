/**
 * \file data_message.cc
 * \brief Source file for the DataMessage class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "os/base/data_message.h"

namespace s3f {
namespace s3fnet {

DataMessage::DataMessage() : payload(0), real_length(0) {}

DataMessage::DataMessage(int real_len, byte* byte_array, bool need_copying) : real_length(real_len)
{
  assert(real_len > 0);
  if(byte_array && need_copying)
  {
    payload = new byte[real_len]; assert(payload);
    memcpy((byte*)payload, byte_array, real_len);
  }
  else payload = byte_array;
}

DataMessage::DataMessage(DataChunk* datachk, bool need_copying) : real_length(0) // to identify the payload is a list of data chunks
{
  if(datachk && need_copying)
  {
    payload = datachk->clone();
  }
  else payload = datachk;
}

DataMessage::DataMessage(const DataMessage& dmsg) : ProtocolMessage(dmsg) // important !!!
{
  real_length = dmsg.real_length;
  if(dmsg.payload)
  {
    if(real_length > 0)
    {
      payload = new byte[real_length]; assert(payload);
      memcpy((byte*)payload, (byte*)dmsg.payload, real_length);
    }
    else
    {
      payload = ((DataChunk*)dmsg.payload)->clone();
    }
  }
  else payload = 0;
}

ProtocolMessage* DataMessage::clone()
{
  return new DataMessage(*this);
}

DataMessage::~DataMessage()
{
  if(payload)
  {
    if(real_length > 0) delete[] (byte*)payload;
    else delete (DataChunk*)payload;
  }
}

int DataMessage::packingSize()
{
  int base = ProtocolMessage::packingSize()+2*sizeof(int32);
  if(payload)
  {
    if(real_length > 0) return base+real_length;
    else return base+((DataChunk*)payload)->totalPackingSize();
  }
  else return base;
}

int DataMessage::realByteCount() 
{ 
  if(real_length > 0) return real_length; 
  else if(payload) return ((DataChunk*)payload)->totalRealBytes();
  else return 0;
}

S3FNET_REGISTER_MESSAGE(DataMessage, S3FNET_PROTOCOL_TYPE_OPAQUE_DATA);

}; // namespace s3fnet
}; // namespace s3f

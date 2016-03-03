/**
 * \file data_message.h
 * \brief Header file for the DataMessage (and DataChunk) class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __DATA_MESSAGE_H__
#define __DATA_MESSAGE_H__

#include "os/base/protocol_message.h"

namespace s3f {
namespace s3fnet {

/**
 * \brief List of data chunks to be carried by a protocol message.
 * 
 * Used for maintaining a list of data chunks, some of which could be
 * fake data (meaning that there's no real payload in simulation), and
 * some of which could contain real bytes. The data chunks are carried
 * by DataMessage, a special protocol message used in simulation to
 * represent generic payload.
 */
class DataChunk {
 public:
  /** The constructor without setting the fields. */
  DataChunk() : real_length(0), real_data(0), next(0) {}
  
  /** The constructor with specific fields. */
  DataChunk(uint32 len, byte* data = 0, DataChunk* nxt = 0) :
    real_length(len), real_data(data), next(nxt) {}

  /** The copy constructor; make a copy of this data chunk as well as
      all chunks following this one. */
  DataChunk(const DataChunk& chunk) : real_length(chunk.real_length)
  {
    if(chunk.real_data)
    {
      real_data = new byte[real_length]; assert(real_data);
      memcpy(real_data, chunk.real_data, real_length);
    }
    else real_data = 0;

    if(chunk.next) next = chunk.next->clone();
    else next = 0;
  }

  /** Deep cloning: create a copy of all data chunks following this node as well. */
  DataChunk* clone() { return new DataChunk(*this); }

  /** The destructor. */
  ~DataChunk()
  {
    if(real_data) delete[] real_data;
    if(next) delete next;
  }

  /** When packing this data node, how much space is needed to pack
      the data into a byte array. This is the size of the data
      eventually got transferred in the simulation system. */
  int packingSize() { return 2*sizeof(int32)+real_data?real_length:0; }
  
  /** Total packing size including this chunk and the followers. */
  int totalPackingSize()
  {
    if(next) return packingSize()+next->totalPackingSize();
    else return packingSize();
  }

  /** Number of bytes of the data in reality. */
  int realByteCount() { return real_length; }

  /** Total number of bytes of all data to be transferred in the
      target system in reality (rather than in simulation). */
  int totalRealBytes()
  {
    if(next) return real_length+next->totalRealBytes();
    else return real_length;
  }

 public:
  /** The size of this data block in the target system, despite the
      fact that one may choose not to have real data in this data
      block. */
  uint32 real_length;

  /** The real data. if it's NULL, this is a fake data block. */
  byte* real_data;
  
  /** Points to the next data chunk. */
  DataChunk* next;
};

/**
 * \brief A generic protocol message carrying a payload as a
 * continuous byte stream or a list of data chunks.
 * 
 * There are two types of the payload this message is carrying. The
 * payload data can be represented as a continuous byte stream or byte
 * array. If this is true, the payload pointer should be of type byte*
 * that points to a byte array or simply NULL. The latter case is when
 * the data is intentionally not included in simulation for better
 * efficiency. Also, the real_length should be the number of real
 * bytes of this payload in the target system (rather than in the
 * simulator). The data can also be a list of data chunks, in which
 * case the list could have both fake data and real data blocks. This
 * situation is identified by having real_length set to be zero. The
 * real length of the payload can be obtained by invoking the
 * DataChunk::totalRealBytes method.
 */
class DataMessage : public ProtocolMessage {
 public:
  /** The payload. The format of the payload can be a byte array
      (including NULL), or a list of data chunks. */
  void* payload;

  /** The real payload size, i.e., in the real target system rather
      than in the simulator. If this field is zero, it means the
      payload is a list of data chunks. */
  int real_length;

  /** Default constructor without any argument. */
  DataMessage();

  /**
   * Constructor with given fields. If need_copying is true, the
   * constructor will explicitly copy the byte_array to an internal
   * buffer. Otherwise, the ownership of byte_array is simply
   * transferred. DataMessage thereafter takes the responsibility of
   * reclaiming the buffer eventually. real_len is the real length of
   * the message (i.e., in the target system rather than in
   * simulation).
   */
  DataMessage(int real_len, byte* byte_array = 0, bool need_copying = false);

  /** 
   * This is the constructor that takes a list of data chunks as
   * payload. If need_copying is true, the constructor will explicitly
   * copy the data chunks.  Otherwise, the ownership of the list is
   * transferred.
   */
  DataMessage(DataChunk* data_chunks, bool need_copying = false);

  /** The copy constructor. */
  DataMessage(const DataMessage&);

  /** Cloning the protocol message (required by ProtocolMessage). */
  virtual ProtocolMessage* clone();

  /** The destructor. */
  virtual ~DataMessage();

  /** Return the protocol number that this message represents. */
  virtual int type() { return S3FNET_PROTOCOL_TYPE_OPAQUE_DATA; }

  /**
   * This method returns the number of bytes that are needed to pack
   * this protocol message in simulation. This is for serialization.
   */
  virtual int packingSize();

  /**
   * Return the number of bytes that this data message occupies in the
   * real world, even though we might not actually allocating the
   * buffer with this size in simulation.
   */
  virtual int realByteCount();

};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__DATA_MESSAGE_H__*/

/**
 * \file tcp_sndwnd.h
 * \brief Header file for TCPSendWindow class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __TCP_SNDWND_H__
#define __TCP_SNDWND_H__

#include "os/base/data_message.h"
#include "os/tcp/tcp_seqwnd.h"
#include "s3fnet.h"

namespace s3f {
namespace s3fnet {

/**
 * \brief The sliding window for TCP sender.
 */
class TCPSendWindow: public TCPSeqWindow {
 public:
  /** The constructor. */
  TCPSendWindow(uint32 initseq, uint32 bufsiz, uint32 wndsiz);

  /** The destructor. */
  ~TCPSendWindow();

  /** Generate a message with designated length and sequence number. */
  DataMessage* generate(uint32 seqno, uint32 length);

  /** Shift the send window and then release the buffer. */
  void release(uint32 length);

  /** Return how many bytes can be sent right now from the buffer. */
  uint32 canSend()  { return length_in_buffer - used(); }

  /** 
   * If all the requested data has been sent out, we need to request
   * more data from the upper layer.
   */
  bool needToRequest() { return length_in_request <= 0; }

  /** Request to send new data. */
  void requestToSend(byte* msg, uint32 length);

  /** Reset the window. */
  void reset();

 public:
  /** Returns how much of the buffer is used. */
  uint32 dataInBuffer()  { return length_in_buffer; }
  
  /** Returns how much of the buffer is free. */
  uint32 freeInBuffer()  { return buffer_size - length_in_buffer; }
  
  /** Returns buffer size. */
  uint32 bufferSize ()   { return buffer_size; }

  /** Returns how much data is still requesting to enter the
      buffer. */
  uint32 dataInRequest() { return length_in_request; }

  /** Returns how much data has been put into the buffer. */
  uint32 dataBuffered() { return length_buffered; }

  /** Clear how much data has been buffered. */
  void clearDataBuffered() { length_buffered = 0; }

 protected:
  /**
   * Remove the unused data from the head. because the data with lower
   * sequence number is at the front of the list, we just need to
   * remove from the head node.
   */
  void release_buffer(uint32 length);

  /** 
   * Move some data from the request to the buffer when the buffer has
   * some vacancy.
   */
  void add_to_buffer(uint32 length);

 protected:
  /** Buffer size. */
  uint32 buffer_size;

  /** How many bytes are in the buffer waiting to be sent out? */
  uint32 length_in_buffer;

  /** How many bytes are waiting to enter the buffer? */
  uint32 length_in_request;

  /** How many bytes have been put into the buffer. */
  uint32 length_buffered;

  /** Point to the head of the message list. */
  DataChunk* head_chunk;

  /** Point to the tail of the message list. */
  DataChunk* tail_chunk;

  /**
   * The pointer to the message block. When some data gets
   * acknowledged, if the data contains real data, we have to release
   * them. Obviously, a naive way is to new a smaller block data and
   * move the left unacknowledged data to it.  Instead, in order to
   * optimize the performance, we simply keep a pointer as the offset
   * to the raw message. If the first whole block is released, we can
   * delete this raw message.
   */
  byte* first_raw_data;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__TCP_SNDWND_H__*/

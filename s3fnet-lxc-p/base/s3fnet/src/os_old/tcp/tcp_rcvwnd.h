/**
 * \file tcp_rcvwnd.h
 * \brief Header file for TCPRecvWindow class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __TCP_RCVWND_H__
#define __TCP_RCVWND_H__

#include "os/tcp/tcp_seqwnd.h"

namespace s3f {
namespace s3fnet {

class DataMessage;

/**
 * \brief A TCP segment received by the TCP receiver.
 *
 * The received TCP segments are organized as a linked list in the
 * receive window.
 */
class TCPSegment {
 public:
  /** The default constructor. */
  TCPSegment() {}

  /** The constructor with given fields. */
  TCPSegment(uint32 sno, uint32 len, byte* m, TCPSegment* nxt = 0) :
    seqno(sno), length(len), msg(m), next(nxt) {}

  /** Set the fields. */
  void set(uint32 sno, uint32 len, byte* m, TCPSegment* nxt = 0) {
    seqno = sno; length = len; msg = m; next = nxt;
  }
    
 public:
  uint32 seqno; ///< The sequence number of the first byte of the segement.
  uint32 length; ///< The length of the segment.
  byte* msg; ///< Point to the data; NULL if it's fake data.
  TCPSegment* next; ///< Point to the next segment.
};
  
/**
 * \brief The sliding window for TCP receiver.
 * 
 * We use a linked list as the buffer to store received packets. When
 * the upper layer requests to receive data, it will pop up data from
 * the linked list.
 */
class TCPRecvWindow: public TCPSeqWindow {
 public:
  /** The constructor. */
  TCPRecvWindow(uint32 initseq, uint32 winsiz);

  /** The destructor. */
  ~TCPRecvWindow();

  /** Whether there is new data available. */
  bool available();

  /** Generate the message of the given length and submit it to the
      upper layer. */
  uint32 generate(uint32 length, byte* msg);

  /** Add the message to the receive buffer. */
  void addToBuffer(DataMessage* msg, uint32 seqno);

  /** The free space in the buffer. */
  uint32 freeInBuffer() { return win_size - used(); }

  /** The buffer size. */
  uint32 bufferSize() { return win_size; }

  /** Return the highest sequence number ever received. */
  uint32 getHighestSeqno() { return highest_seqno; }

  /** 
   * Get the expected sequence number from the buffer; this is
   * necessary because an ACK packet may be sent out before the buffer
   * is released.
   */
  uint32 getExpectedSeqno();

  // The following methods are used to interact (using signals) with
  // the protocol layer above.

  /** Set the application buffer to receive data. */
  void setRecvParameters(uint32 bufsiz, byte* buf)
  {
    assert(appl_rcvbuf_size == 0 && appl_rcvbuf == 0);
    appl_rcvbuf_size = bufsiz;
    appl_rcvbuf = buf;
  }

  /** The application buffer no longer used. */
  void resetRecvParameters()
  {
    appl_rcvbuf_size = 0; appl_rcvbuf = 0;
  }

  /** Return the number of bytes received last time. */
  uint32 dataReceived() { return appl_data_rcvd; }

 private:
  /** A helper function for addToBuffer. */
  bool add_to_buffer_helper(uint32 seqno, uint32 length, byte* msg);
  
  /**
   * Return a free segment. We use a list to keep old segments to save
   * time on memory allocation.
   */
  TCPSegment* get_a_segment();

  /** Add a segment to the free list. */
  void add_a_free_segment(TCPSegment* seg);

 private:
  /** A buffer to put received segments. */
  TCPSegment* seg_buffer;

  /** A list of unused segments. */
  TCPSegment* free_seg_list;

  /** The highest sequence number received. */
  uint32 highest_seqno;

  /** The buffer (owned by application) to receive data. */
  byte* appl_rcvbuf;

  /** Total number of bytes expected to receive. */
  uint32 appl_rcvbuf_size;

  /** The size of the received data (that has been put into the
      receive buffer). */
  uint32 appl_data_rcvd;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__TCP_RCVWND_H__*/

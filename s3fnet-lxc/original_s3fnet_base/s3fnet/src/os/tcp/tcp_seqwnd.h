/**
 * \file tcp_seqwnd.h
 * \brief Definition of the TCPSeqWindow class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __TCP_SEQWND_H__
#define __TCP_SEQWND_H__

#include "s3fnet.h"

namespace s3f {
namespace s3fnet {

/**
 * \brief Implements the sliding window in TCP. 
 *
 * This class provides basic operations on sequence numbers. One needs
 * to make sure that the window size is less than or equal to the
 * buffer size. This class does not maintain real data. Therefore, the
 * send window and receive window should extend this class to add more
 * functionalities.
 */
class TCPSeqWindow {
 public:
  /** The constructor. */
  TCPSeqWindow(uint32 init_seqno, uint32 wsize) :
    start_seqno(init_seqno), win_size(wsize), used_size(0),
    syn_included(0), fin_included(0) {}
  
  /*
   * Functions that return certain properties to the user.
   */
  
  /** Return the first sequence of the window. */
  uint32 start() { return start_seqno; }

  /** Return the last sequence of the window. */
  uint32 end() { return start_seqno + win_size - 1; }

  /** Return the first unused sequence in the window. */
  uint32 firstUnused() { return start_seqno + used_size; }

  /** Return the size of the window. */
  uint32 size() { return win_size; }

  /** Return how much of the window is used up in bytes. */
  uint32 used() { return used_size; }

  /** Return how much of the window is unused in bytes. */
  uint32 unused() { return (win_size - used_size); }

  /** Return the number of bytes between a give sequence number and
      the start of the window. */
  uint32 lengthTo(const uint32 seqno) { return seqno - start_seqno; }

  /*
   * Functions that affect sequence numbers only and not real data.
   */
   
  /** Set the start sequence number of the window. */
  void setStart(const uint32 seqno) { start_seqno = seqno; }
  
  /*
   * Functions that deal with physical window size/use
   */

  /** Increase the lower edge of the window by offset number of
      bytes. */
  void shift(uint32 offset)
  {
    start_seqno += offset;
    used_size -= mymin(offset, used_size);
  }

  /** Expand the window size by nbytes. Note that the new size must
      not be larger than the buffer size---the condition is not
      enforced by this method. */
  void expand(uint32 nbytes) { win_size += nbytes; }

  /** Shrink the size of the window by nbytes. One can only shrink the
      size of unused space. */
  void shrink(uint32 nbytes)
  {
    assert(nbytes <= (win_size - used_size));
    win_size -= nbytes;
  }

  /** Use up nbytes of the window with data. Returns true if success,
      i.e., there is available unused space for this operation. */
  bool use(uint32 nbytes)
  {
    if(used_size + nbytes > win_size) return false;
    used_size += nbytes;
    return true;
  }
  
  /*
   * Functions to handle TCP specific SYN & FIN segments.
   */

  /** set SYN bit. */
  void setSYN(bool syn)
  {
    if(syn) assert(used_size == 0);
    else seq_shift(1);
    syn_included = syn ? 1 : 0;
  }
  
  /** set FIN bit. */
  void setFIN(bool fin)
  {
    if(!fin) seq_shift(1);
    fin_included = fin ? 1 : 0;
  }
  
  /** Return true if SYN segment is in the window. */
  bool synAtFront() { return syn_included; }

  /** Return true if FIN segment is in the window. */
  bool finAtFront() { return (fin_included && used_size == 0); }

  /** Like firstUnused method, but also uses SYN & FIN segments; the
      method is used for sending window. */
  uint32 next() { return start_seqno+used_size+syn_included+fin_included; }

  /** Like start method, but also uses SYN & FIN segments; the method
      is used for receiving window. */
  uint32 expect() { return start_seqno+syn_included+fin_included; }

  /*
   * Functions that test various conditions.
   */
  
  /** Returns true iff 'seqno' is within the window. */
  bool within(uint32 seqno)
  {
    return (start_seqno+syn_included+fin_included <= seqno &&
	    seqno < start_seqno+syn_included+fin_included+win_size);
  }

  /** Returns true iff 'seqno' is used within the window. */
  bool isUsed(uint32 seqno)
  {
    return (start_seqno <= seqno && seqno < start_seqno + used_size);
  }

  /** Returns true iff the window is empty (nothing used). */
  bool empty() { return (used_size == 0); }

  /** Resets the window. */
  void reset() { used_size = 0; syn_included = fin_included = 0; }

 protected:
  /** Shift the window by the given offset permanently. */
  void seq_shift(uint32 offset) { start_seqno += offset; } 

 protected:
  /** Starting sequence number of the window. */
  uint32 start_seqno;

  /** Size of the window in bytes. */
  uint32 win_size;

  /** How many bytes are used in window. */
  uint32 used_size;

  /** Whether SYN is in window. */
  int syn_included;

  /** Whether FIN is in window. */
  int fin_included;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__TCP_SEQWND_H__*/

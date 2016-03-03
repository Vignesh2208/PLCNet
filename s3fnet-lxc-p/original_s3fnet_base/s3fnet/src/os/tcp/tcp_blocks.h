/**
 * \file tcp_blocks.h
 * \brief Header file for the TCPBlockList class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __TCP_BLOCKS_H__
#define __TCP_BLOCKS_H__

#include "s3fnet.h"

namespace s3f {
namespace s3fnet {

/**
 * \brief A consecutive block of sequence numbers.
 */
class TCPBlock {
 public:
  uint32 left_edge;  ///< The left edge of the block.
  uint32 right_edge; ///< The right edge of the block.
  TCPBlock *next; ///< Point to the next block.

  /** The constructor. */
  TCPBlock(uint32 low, uint32 high) :
    left_edge(low), right_edge(high), next(0) {}
};

/**
 * \brief A list of blocks of consecutive sequence numbers.
 *
 * The data structure is used by both sending and receiving windows to
 * keep track of data sent and received.
 */
class TCPBlockList {
 public:
  enum {
    PATTERN_UNSORTED = 0,
    PATTERN_INCREASE = 1
  };

  /** The constructor. */
  TCPBlockList(int pattern); 

  /** The destructor. */
  ~TCPBlockList() { clear_all_blocks(); delete head_block; }
  
  /** Insert a block with sequence numbers of length len starting from
      seqno into the list. */
  void insert_block(uint32 seqno, uint32 len);

  /** Clear all blocks less than a given seqno. */
  void clear_blocks(uint32 seqno);

  /** Clear all blocks. */
  void clear_all_blocks();

  /**
   * Fetch blocks. Put the the boundaries (left edge and right edge)
   * of each block in the given buffer. Therefore, the buffer length
   * is at least 2*num*sizeof(uint32) bytes. Return the number of
   * blocks that are really included.
   */
  int fetch_blocks(uint32* buffer, int num);

  /**
   * Remove the block with the lowest sequence number. The block is
   * removed from the list only if the remove parameter is true. The
   * function returns the length of the lowest block.
   */
  uint32 remove_lowest(bool remove);

  /**
   * Return whether the block delineated by the starting seqno and
   * length is a new block in the list; if not, the returned
   * parameters point to the non-overlapped region within the given
   * block.
   */
  bool is_new(uint32* seqno, uint32* length);

  /** Return the lowest sequence number of this list. */
  uint32 get_lowest() { return head_block->left_edge; }

  /** Return the highest sequence number of this list. */
  uint32 get_highest() { return head_block->right_edge; }

  /**
   * Used only when the pattern is PATTERN_INCREASE. The method
   * returns the first unavailable item in the list from the current
   * position.
   */
  uint32 unavailable(uint32 startno);

 protected:
  /** Coalesce the first block in the list with other blocks in the
      list; return true if further coalescing is needed. */
  bool overlap_blocks();

  /** Resort blocks (in increasing order): relocate the first block
      (newly inserted) in the list to the correct position. */
  void resort_blocks();
  
 private:
  /** Whether the list should be sorted (increasingly) or not. */
  int pattern;

  /** The head of the block list. */
  TCPBlock* head_block;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__TCP_BLOCKS_H__*/

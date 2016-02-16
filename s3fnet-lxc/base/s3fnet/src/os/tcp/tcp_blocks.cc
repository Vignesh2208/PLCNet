/**
 * \file tcp_blocks.cc
 * \brief Source file for the TCPBlockList class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "os/tcp/tcp_blocks.h"

namespace s3f {
namespace s3fnet {

TCPBlockList::TCPBlockList(int pat) : pattern(pat)
{
  head_block = new TCPBlock(UINT_MAX, 0);
}

void TCPBlockList::insert_block(uint32 seqno, uint32 len)
{
  TCPBlock *new_block = new TCPBlock(seqno, seqno+len);
  new_block->next = head_block->next;
  head_block->next = new_block;

  // if overlapped, we still have to check whether the first block 
  // overlaps with other blocks. it's tricky. For example, (0, 100), 
  // (200, 300), now (100, 200) is coming. Then the first time
  // when overlapping, (0,100) integrates with (100,200), that's 
  // (0,200), we still have to integrate (0,200) and (200, 300), 
  // that's (0, 300). However, 2 times is enough.
  if(overlap_blocks()) overlap_blocks();

  // update highest and lowest
  head_block->right_edge = mymax(head_block->right_edge, seqno + len);
  head_block->left_edge = mymin(head_block->left_edge, seqno);

  // if the block list needs to be sorted increasingly
  if(PATTERN_INCREASE == pattern) resort_blocks();
}

bool TCPBlockList::overlap_blocks()
{
  bool no_overlap = true;
  TCPBlock *real_head = head_block->next, *p = real_head;

  // not overlapped if the list is empty
  if(!p) return false;
  
  while(p->next)
  {
    // example: [100,200]->[0,100]
    if((real_head->left_edge >= p->next->left_edge) && 
       (real_head->left_edge <= p->next->right_edge))
    {
      no_overlap = false;
      
      // combine of the two blocks
      real_head->left_edge = p->next->left_edge;
      real_head->right_edge = mymax(real_head->right_edge, p->next->right_edge);
    }
    
    // example: [0,100]->[100,200]
    else if((real_head->right_edge >= p->next->left_edge) && 
	    (real_head->right_edge <= p->next->right_edge))
    {
      no_overlap = false;
      
      real_head->right_edge = p->next->right_edge;
      real_head->left_edge = mymin(real_head ->left_edge, p->next->left_edge);
    }
            
    if(no_overlap) p = p->next;
    else
    {
      // delete p->next, because it has been integrated to the first
      // block of the list
      TCPBlock *q = p->next;
      p->next = p->next->next;
      delete q;
      break;
    }
  }
  
  return !no_overlap;
}

void TCPBlockList::resort_blocks()
{
  if(!head_block->next) return;

  TCPBlock *p, *q = head_block->next;
  uint32 first_no = q->left_edge;

  // find the right position of first_no and then relocate the first
  // block between block q and block p.
  for(p = q->next; p && p->left_edge < first_no; p = p->next) 
    q = p;
  if(q != head_block->next)
  {
    q->next = head_block->next;
    q = head_block->next->next;
    head_block->next->next = p;
    head_block->next = q;
  }
}

void TCPBlockList::clear_blocks(uint32 seqno)
{
  TCPBlock *q = head_block, *p = head_block->next;
  while(p)
  {
    if(p->left_edge < seqno) // left edge below seqno
    {
      // we need to update lowest
      head_block->left_edge = mymax(head_block->left_edge, seqno);
      
      if(p->right_edge > seqno)
      {
    	// if seqno is contained in block, we need to cut partially
    	p->left_edge = seqno;
	
    	// if it's increasing sort, we are done
    	if(PATTERN_INCREASE == pattern) break;
    	else
    	{
    	  q = p;
    	  p = p->next;
    	}
      }
      else
      {
    	// we have to delete the whole block, we have to consider the next block
    	q->next = p->next;
    	delete p;
    	p = q->next;
      }
    }
    else // left edge above seqno
    {
      // if it's increasing sort, we're done
      if(PATTERN_INCREASE == pattern) break;
      else
      {
    	q = p;
    	p = p->next;
      }
    }
  }
  
  // if all blocks are cleared, we have to reset lowest and highest
  if(!head_block->next)
  {
    head_block->left_edge = UINT_MAX;
    head_block->right_edge = 0;
  }
}

void TCPBlockList::clear_all_blocks()
{
  clear_blocks(head_block->right_edge);
}

int TCPBlockList::fetch_blocks(uint32* buffer, int num)
{
  int i;
  TCPBlock *p = head_block->next;
  assert(buffer);
  for(i=0; p && i<num; i++, p=p->next)
  {
    buffer[2*i] = p->left_edge;
    buffer[2*i+1] = p->right_edge;
  }
  return i;
}

uint32 TCPBlockList::remove_lowest(bool remove)
{
  TCPBlock* p;

  uint32 ret_len = 0;
  for(p=head_block; p; p=p->next)
  {
    if(p->next && p->next->left_edge == head_block->left_edge)
    {
      ret_len = p->next->right_edge - p->next->left_edge;
      if(remove)
      {
    	TCPBlock* q = p->next->next;
    	delete p->next;
    	p->next = q;
      }
      break;
    }
  }

  // we should elect a new lowest seqno
  if(remove)
  {
    // elect a new lowest block
    if(head_block->next)
    {
      head_block->left_edge = head_block->next->left_edge;
      
      if(PATTERN_UNSORTED == pattern)
      {
    	// if unsorted, we have to traverse all blocks in the list to get the lowest no
    	for(p=head_block->next->next; p; p=p->next)
    		head_block->left_edge = mymin(head_block->left_edge, p->left_edge);
      }
    }
    else
    {
      head_block->left_edge = UINT_MAX;
      head_block->right_edge = 0;
    }
  }
  return ret_len;
}

bool TCPBlockList::is_new(uint32* seqno, uint32* length)
{
  assert(seqno && length);

  uint32 overlap_seqno;
  uint32 overlap_length;

  if(*length == 0) return false;

  for(TCPBlock *p = head_block->next; p; p=p->next)
  {
    if(pattern == PATTERN_INCREASE && p->left_edge >= *seqno + *length)
      return true;

    overlap_seqno  = mymax(*seqno, p->left_edge);
    overlap_length = mymin(*seqno + *length, p->right_edge) - overlap_seqno; 
    
    if(overlap_length > 0) // if overlap
    {
      // the new packet is contained in a block completely
      if(overlap_length == *length) return false;
      
      if(overlap_seqno > p->left_edge)
      {
    	// if the left part of the packet is overlapped we get rid of the overlapped part
    	*seqno  = p->right_edge;
    	*length = *seqno + *length - p->right_edge;
      }
      else
      {
    	// right part of the packet is overlapped
    	*length = p->left_edge - *seqno;
      }
    }
  }
  
  return true;
}

uint32 TCPBlockList::unavailable(uint32 startno)
{
  assert(pattern == PATTERN_INCREASE);

  TCPBlock *p, *q = head_block;
  for(p = q->next; p && startno >= p->left_edge; p = p->next)
    q = p;

  if(q == head_block) return startno;
  else return mymax(q->right_edge, startno);
}

}; // namespace s3fnet
}; // namespace s3f








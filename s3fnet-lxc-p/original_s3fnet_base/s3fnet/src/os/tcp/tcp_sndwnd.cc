/**
 * \file tcp_sndwnd.cc
 * \brief Source file for TCPSendWindow class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "os/tcp/tcp_sndwnd.h"
#include "util/errhandle.h"

namespace s3f {
namespace s3fnet {

TCPSendWindow::TCPSendWindow(uint32 initseq, uint32 bufsiz, uint32 wndsiz) :
  TCPSeqWindow(initseq, wndsiz), buffer_size(bufsiz), 
  length_in_buffer(0), length_in_request(0), length_buffered(0),
  head_chunk(0), tail_chunk(0), first_raw_data(0)
{
  // make sure window size must be less than buffer size
  if(wndsiz > bufsiz)
  {
    error_quit("ERROR: TCP window size (%u) must be no larger than buffer size (%u).\n",
	       wndsiz, bufsiz);
  }
}

TCPSendWindow::~TCPSendWindow() 
{
  if(first_raw_data)
  {
    delete[] first_raw_data;
    assert(head_chunk);
    head_chunk->real_data = 0;
  }
  delete head_chunk;
}

DataMessage* TCPSendWindow::generate(uint32 seqno, uint32 length)
{
  // make sure the sequence does not fall within the buffer
  assert(seqno >= start() && seqno + length <= start() + length_in_buffer);

  // calculate the offset from the start
  uint32 offset = seqno - start();

  // find the right block in the list
  DataChunk* node = head_chunk;
  while(node && offset)
  {
    // if the start position is at node->real_data + offset in the current
    // node, quit the loop.
    if(node->real_length > offset) break;
    offset -= node->real_length;
    node = node->next;
  }
  assert(node);

  // fetch data and put it into a generated payload message.
  if(node->real_length - offset >= length)
  {
    byte* payload;
    // the current node can satisfy the data length requested.
    if(node->real_data)
    {
      // if the current node carries real data, it is the first node
      // and, at the same time, the current node can satisfy the data
      // length required
      payload = new byte[length];
      memcpy((byte*)payload, node->real_data+offset, length);
    }
    else
    {
      // if the current node carries fake data, it is the first node
      // and, at the same time, the current node can satisfy the data
      // length required
      payload = 0;
    }
    return new DataMessage(length, payload);
  }
  else
  {
    // the current node cannot satisfy the data length requested; we
    // maintain a message list, since we need to attach each new block
    // at the tail of the list, we need to keep the tail pointer.
    DataChunk* msglist_head = 0, *msglist_tail = 0;
    uint32 cur_length = length;
    while(cur_length > 0)
    {
      // for the current block, how much data can be transmitted?
      int len = mymin(node->real_length-offset, cur_length);
      
      assert(node);
      if(node->real_data)
      {
    	// look ahead to see how many real data ahead; we want to consolidate the real data here...
    	int contiguous_len = len;
    	for(DataChunk* pnode = node->next; pnode && pnode->real_data; pnode = pnode->next)
    	{
    	  if(pnode->real_length+contiguous_len >= cur_length)
    	  {
    		contiguous_len += mymin(cur_length - contiguous_len, pnode->real_length);
	        break;
    	  }
    	  else contiguous_len += pnode->real_length;
    	}
	
    	// we allocate a block containing these contiguous data
    	byte* block = new byte[contiguous_len];
    	int copied_len = 0;
    	while(copied_len < contiguous_len)
    	{
    	  int new_len = mymin(contiguous_len-copied_len, (int)(node->real_length-offset));
    	  memcpy(block+copied_len, node->real_data+offset, new_len);
    	  copied_len += new_len;
    	  if(offset+new_len < node->real_length)
    	  {
    		offset += new_len;
    		assert(copied_len == contiguous_len);
    	  }
    	  else
    	  {
    		node = node->next;
    		offset = 0;
    	  }
    	}

    	cur_length -= contiguous_len;
    	assert(offset==0 || (offset>0 && cur_length==0));

    	// put the contiguous message block at the tail
    	DataChunk* msg_node = new DataChunk(contiguous_len, block);
    	if(!msglist_head)
    	{
    	  msglist_head = msglist_tail = msg_node;
    	}
    	else
    	{
    	  msglist_tail->next = msg_node;
    	  msglist_tail = msg_node;
    	}
      }
      else // if fake data
      {
    	if(!msglist_head)
    	{
    	  msglist_head = msglist_tail = new DataChunk(len);
    	}
    	else if(!msglist_tail->real_data)
    	{
    	  // the tail block also uses fake data, so we can merge them
	      // together by simply adding the lengths
	      msglist_tail->real_length += len;
    	}
    	else
    	{
    	  // the tail block uses real data, we have to create another
	      // message node to contain this fake data
	      msglist_tail->next = new DataChunk(len);
	      msglist_tail = msglist_tail->next;
	    }
	
	    // we have to visit the next message node if not enough data is fetched.
	    if((cur_length -= len) > 0)
	    {
	      node = node->next;
	      offset = 0;
	    }
      }
    } // while loop
    
    return new DataMessage(msglist_head);
  }
}

void TCPSendWindow::release(uint32 length)
{
  // shift the seq window
  shift(length);

  // release the buffer
  release_buffer(length);

  // because there are free buffer available, we need to add new data
  // to the buffer
  add_to_buffer(mymin(length_in_request, freeInBuffer()));
}

void TCPSendWindow::requestToSend(byte* msg, uint32 msg_len)
{ 
  DataChunk* datachk = new DataChunk(msg_len, msg);
  if(tail_chunk)
  {
    assert(head_chunk);
    tail_chunk->next = datachk;
    tail_chunk = datachk;
  }
  else
  {
    assert(!first_raw_data && !head_chunk);
    head_chunk = tail_chunk = datachk;
    first_raw_data = datachk->real_data;
  }

  // how much data is been requested to send currently
  length_in_request += msg_len;

  // see whether the new data can be put in the buffer.
  add_to_buffer(mymin(msg_len, freeInBuffer()));
}

void TCPSendWindow::reset()
{
  TCPSeqWindow::reset();

  length_in_request = length_in_buffer = 0;

  // restore the data pointer at the head chunk before destroying the
  // entire list
  if(head_chunk)
  {
    if(head_chunk->real_data) 
      head_chunk->real_data = first_raw_data;
    delete head_chunk;
  }

  head_chunk = tail_chunk = 0;
  first_raw_data = 0;
}

void TCPSendWindow::add_to_buffer(uint32 length)
{
  assert(length_in_buffer + length <= buffer_size);
  
  // update how many bytes in the buffer and being requested.
  length_in_buffer += length;
  length_in_request -= length;
  length_buffered += length;

  assert(length_in_request >= 0);
}

void TCPSendWindow::release_buffer(uint32 length)
{
  assert(length <= length_in_buffer);

  length_in_buffer -= length; 
  while(length > 0)
  {
    // check whether we can finish it in the current node.
    if(head_chunk->real_length > length)
    {
      head_chunk->real_length -= length;
      if(head_chunk->real_data)
    	head_chunk->real_data += length;
      length = 0;
    }
    else // we need to destroy the current node
    {
      length -= head_chunk->real_length;
      
      // release the node; note that the real data (present or not)
      // should not be reclaimed here and we should not chain the
      // deletion!
      DataChunk* node = head_chunk->next;
      head_chunk->real_data = 0;
      head_chunk->next = 0;
      delete head_chunk;
      head_chunk = node;

      if(first_raw_data) delete[] first_raw_data;
      if(head_chunk)
      {
    	first_raw_data = head_chunk->real_data;
      }
      else
      {
    	// in case that the whole list is deleted
    	tail_chunk = 0;
    	first_raw_data = 0;
      }
    }
  }
}

}; // namespace s3fnet
}; // namespace s3f

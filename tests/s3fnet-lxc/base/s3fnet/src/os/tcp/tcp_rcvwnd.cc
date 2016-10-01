/**
 * \file tcp_rcvwnd.cc
 * \brief Source file for TCPRecvWindow class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "os/tcp/tcp_rcvwnd.h"
#include "os/base/data_message.h"

namespace s3f {
namespace s3fnet {

TCPRecvWindow::TCPRecvWindow(uint32 initseq, uint32 winsiz) :
  TCPSeqWindow(initseq, winsiz), seg_buffer(0), 
  free_seg_list(0), highest_seqno(initseq), 
  appl_rcvbuf(0), appl_rcvbuf_size(0), appl_data_rcvd(0) {}

TCPRecvWindow::~TCPRecvWindow() 
{
  while(seg_buffer)
  {
    TCPSegment* seg = seg_buffer->next; 
    if(seg_buffer->msg) delete[] seg_buffer->msg;
    delete seg_buffer;
    seg_buffer = seg;
  }

  while(free_seg_list)
  {
    TCPSegment* seg = free_seg_list->next;
    delete free_seg_list;
    free_seg_list = seg;
  }
}

bool TCPRecvWindow::available()
{
  // return true if new data is put into the application's receive
  // buffer set previously when TCPSession::recv() is called
  if(appl_rcvbuf_size > 0)
  {
    if(seg_buffer && seg_buffer->seqno <= expect())
    {
      appl_data_rcvd = generate(appl_rcvbuf_size, appl_rcvbuf);
      return appl_data_rcvd > 0;
    }
    else return false;
  }
  else return false;
}

uint32 TCPRecvWindow::generate(uint32 length, byte* msg)
{
  // we always grab one block from the buffer and report it to the
  // upper layer.
  uint32 request_len = length;
  while(seg_buffer && length > 0 && (expect() == seg_buffer->seqno))
  {
    // we consider the situation in which the upper layer has
    // requested different data lengths.
    if(length >= seg_buffer->length)
    {
      // can submit whole data block to the upper layer.
      if(msg)
      {
    	if(seg_buffer->msg)
    	{
    	  memcpy(msg, seg_buffer->msg, seg_buffer->length);
    	  delete[] seg_buffer->msg;
    	}
    	else
    	{
    		memset(msg, 0, seg_buffer->length);
    	}
    	msg += seg_buffer->length;
      }
      else if(seg_buffer->msg)
      {
    	delete[] seg_buffer->msg;
      }
      
      // update the start sequence number
      shift(seg_buffer->length);  
      length -= seg_buffer->length;

      TCPSegment* seg = seg_buffer->next;
      add_a_free_segment(seg_buffer);
      seg_buffer = seg;
    }
    else
    {
      // we have to cut the current data block into two pieces.
      if(msg)
      {
    	if(seg_buffer->msg)
    	{
    	  memcpy(msg, seg_buffer->msg, length);
    	  byte* remain_msg = new byte[seg_buffer->length - length];
    	  memcpy(remain_msg, seg_buffer->msg+length, seg_buffer->length - length);
    	  delete [] seg_buffer->msg;
    	  seg_buffer->msg = remain_msg;
    	}
    	else
    	{
    		memset(msg, 0, length);
    	}
    	msg += seg_buffer->length;
      }
      else if(seg_buffer->msg)
      {
    	byte* remain_msg = new byte[seg_buffer->length - length];
    	memcpy(remain_msg, seg_buffer->msg+length, seg_buffer->length - length);
    	delete [] seg_buffer->msg;
    	seg_buffer->msg = remain_msg;
      }

      // shorten the length of the remaining message.
      seg_buffer->seqno += length;
      seg_buffer->length -= length;

      // update the start sequence number
      shift(length);
      length = 0;
    }
  }
  return request_len - length;
}

void TCPRecvWindow::addToBuffer(DataMessage* msg, uint32 seqno)
{
  assert(msg);
  if(msg->real_length > 0)
  {
    bool new_data = add_to_buffer_helper(seqno, msg->real_length, (byte*)msg->payload);
    if(new_data && msg->payload) msg->payload = 0;
    use(msg->real_length);
  }
  else
  {
    DataChunk* node = (DataChunk*)msg->payload;
    while(node)
    {
      DataChunk* next_node = node->next;
      bool new_data = add_to_buffer_helper(seqno, node->real_length, node->real_data);
      if(new_data && node->real_data)
    	  node->real_data = 0;

      seqno += node->real_length;
      use(node->real_length);
      node = next_node;
    }
  }
}

uint32 TCPRecvWindow::getExpectedSeqno()
{
  uint32 expect_no = expect();
  for(TCPSegment* seg = seg_buffer; seg; seg = seg->next)
  {
    // if the packet with the expect sequence number has already been
    // in the buffer, we find the next one
    if(expect_no >= seg->seqno) expect_no += seg->length;
    else return expect_no;
  }
  return expect_no;
}

bool TCPRecvWindow::add_to_buffer_helper(uint32 seqno, uint32 length, byte* msg)
{
  /* Copy the received message to the buffer, if msg is NULL, we will
    take it as fake data. Here we assume that the packet either
    overlaps completely with a packet already received, or is a
    totally new packet. In other words, the sender will never send out
    a partially overlapped packet. The method returns true if the data
    is new. Otherwise, it returns false. */

  if(length <= 0) return false;
  highest_seqno = mymax(highest_seqno, seqno + length - 1);

  // we use a linked list to put all segments.
  if(!seg_buffer)
  {
    // there is no segment in the buffer
    TCPSegment* seg = get_a_segment();
    seg->set(seqno, length, msg, 0);
    seg_buffer = seg;
    return true; // a new packet
  }
  else if(seqno < seg_buffer->seqno)
  {
    // if the newly arrived is a packet predates all received ones
    if(seqno + length == seg_buffer->seqno && !msg && !seg_buffer->msg)
    {
      // the new packet is just before the head packet in the buffer
      // and both of them use fake data. we can merge them together.
      seg_buffer->seqno = seqno;
      seg_buffer->length += length;
    }
    else
    {
      TCPSegment* seg = get_a_segment();
      seg->set(seqno, length, msg, seg_buffer);
      seg_buffer = seg;
    }
    return true; // a new packet
  }
  else if(seqno < seg_buffer->seqno + length)
  {
    // this must be an old packet; according to our assumption, there
    // must be a complete match
    return false; 
  }
  else
  {
    // the new packet is after the already-received packet.
    TCPSegment* seg;
    for(seg = seg_buffer; seg->next && seqno >= seg->next->seqno; seg = seg->next);
    // check whether there is overlapping. when the loop stops, it
    // must be either seg->next is NULL or seqno < seg->next->segno;
    // therefore, the new packet can only overlap with the segment
    // pointed by seg.
    if(seqno < seg->seqno + seg->length)
    {
      // overlapping with an old packet
      return false;
    }
    else if(seqno == seg->seqno + seg->length && !msg && !seg->msg)
    {
      // contiguous packets and they both use fake data.
      seg->length += length;
      return true;
    }
    else
    {
      // non-overlapping, non-contiguous, just put it behind seg.
      TCPSegment* new_seg = get_a_segment();
      new_seg->set(seqno, length, msg, seg->next);
      seg->next = new_seg;
      return true;
    }
  }
}

TCPSegment* TCPRecvWindow::get_a_segment()
{
  if(free_seg_list)
  {
    TCPSegment* ret = free_seg_list;
    free_seg_list = ret->next;
    return ret;
  }
  else
	return new TCPSegment();
}

void TCPRecvWindow::add_a_free_segment(TCPSegment* seg)
{
  assert(seg);
  seg->next = free_seg_list;
  free_seg_list = seg;
}

}; // namespace s3fnet
}; // namespace s3f

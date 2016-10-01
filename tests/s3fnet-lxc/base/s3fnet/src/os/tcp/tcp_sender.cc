/**
 * \file tcp_sender.cc
 * \brief Methods for the sender side of the TCPSession class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "os/tcp/tcp_session.h"
#include "util/errhandle.h"
#include "net/host.h"
#include "os/ipv4/ip_interface.h"
#include "os/ipv4/ip_message.h"

#ifdef TCP_DEBUG
#define TCP_DUMP(x) printf("TCPSESS: "); x
#else
#define TCP_DUMP(x)
#endif

namespace s3f {
namespace s3fnet {

uint32 TCPSession::appl_send(uint32 length, byte* msg)
{
  // clear how much data has been buffered.
  sndwnd->clearDataBuffered();

  // request to send out data
  if(msg)
  {
    byte* msgcpy = new byte[length]; assert(msgcpy);
    memcpy(msgcpy, msg, length);
    sndwnd->requestToSend(msgcpy, length);
  }
  else sndwnd->requestToSend(0, length);

  // check whether we can send out packets.
  uint32 len_to_send = sndwnd->canSend();
  if(len_to_send > 0)
  {
    // send out new data if possible
    segment_and_send(sndwnd->firstUnused(), len_to_send);
  }

  // this function will be triggered from the socket layer. If we do
  // it once, we don't need to do it again.
  clear_app_state(SocketSignal::OK_TO_SEND);

  // return number of bytes have been put into the buffer.
  uint32 ret = sndwnd->dataBuffered();
  sndwnd->clearDataBuffered();
  return ret;
}

void TCPSession::send_data(uint32 seqno, uint32 length, byte mask, uint32 ackno,
			   bool need_calc_rtt, bool need_set_rextimeout)
{
  // they will be used when SACK enabled.
  int added_len = 0;
  byte* options = NULL;
  DataMessage* data = NULL;

  // generate the payload for this TCP packet.
  if(length > 0) data = sndwnd->generate(seqno, length);
  
  // except the SYN and FIN packets, we set ACK anyway.
  if(!(mask & TCPMessage::TCP_FLAG_SYN)/* && !(mask & TCPMessage::TCP_FLAG_FIN)*/)
    mask |= TCPMessage::TCP_FLAG_ACK;
  
  // create the TCP header.
  TCPMessage* tcp_msg = new TCPMessage
    (src_port, dst_port, seqno, ackno, mask, calc_advertised_wnd(), 
     (S3FNET_TCPHDR_LENGTH+added_len)/sizeof(uint32), options);

  // attach the payload to the tcp message header.
  tcp_msg->carryPayload(data);

  TCP_DUMP(printf("[host=\"%s\"] %s: send_data(), flags=[%c%c%c%c%c%c] "
		  "seqno=%u, ackno=%u, wsize=%u, len=%u [state=%s]\n",
		  tcp_master->inHost()->nhi.toString(),
		  tcp_master->getNowWithThousandSeparator(),
		  (tcp_msg->flags & TCPMessage::TCP_FLAG_SYN) ? 'S' : '.',
		  (tcp_msg->flags & TCPMessage::TCP_FLAG_FIN) ? 'F' : '.',
		  (tcp_msg->flags & TCPMessage::TCP_FLAG_RST) ? 'R' : '.',
		  (tcp_msg->flags & TCPMessage::TCP_FLAG_ACK) ? 'A' : '.',
		  (tcp_msg->flags & TCPMessage::TCP_FLAG_URG) ? 'U' : '.',
		  (tcp_msg->flags & TCPMessage::TCP_FLAG_PSH) ? 'P' : '.',		
		  tcp_msg->seqno, ackno, tcp_msg->wsize,
		  data?data->realByteCount():0, get_state_name(state)));

  if(need_calc_rtt && !rtt_count)
  {
    measured_seq = seqno;        
    rtt_count = 1;
  }
  
  // Physically Send the Data
  IPPushOption ops(src_ip, dst_ip, S3FNET_PROTOCOL_TYPE_TCP, DEFAULT_IP_TIMETOLIVE);
  tcp_master->pushdown(tcp_msg, NULL, (void*)&ops, sizeof(IPPushOption));
  
  // if timer is not scheduled, schedule it and start RTT measurment;
  // whenever a segment is sent out, we should check whether we need
  // to start the retransmission timer
  if(need_set_rextimeout && rxmit_timer_count == 0)
  {
    rxmit_timer_count = timeout_value();
  }
}

uint32 TCPSession::segment_and_send(uint32 seqno, uint32 length)
{
  if(length <= 0) return 0;

  uint32 bytes_sent = 0;
  if(sndwnd->used() < mymin(rcvwnd_size, cwnd))
  {
    uint32 nbytes = mymin(rcvwnd_size, cwnd)-sndwnd->used();
    nbytes = mymin(nbytes, sndwnd->unused());
    nbytes = mymin(nbytes, length);
    nbytes = mymin(nbytes, mss);
    nbytes = mymin(nbytes, S3FNET_IP_MAX_DATA_SIZE - S3FNET_TCPHDR_LENGTH);
    while(nbytes == mss ||
	  0 < nbytes && nbytes < mss && 
	  nbytes == sndwnd->dataInBuffer() - sndwnd->used())
    {
      if(!sndwnd->use(nbytes)) return bytes_sent;
	
      send_data(seqno, nbytes, 0, rcvwnd->expect(), true, true);
	
      seqno += nbytes;
      length -= nbytes;      
      bytes_sent += nbytes;

      nbytes = mymin(rcvwnd_size, cwnd)-sndwnd->used();
      nbytes = mymin(nbytes, sndwnd->unused());
      nbytes = mymin(nbytes, length);
      nbytes = mymin(nbytes, mss);
      nbytes = mymin(nbytes, S3FNET_IP_MAX_DATA_SIZE - S3FNET_TCPHDR_LENGTH);
    }
  }
  return bytes_sent;
}

uint32 TCPSession::resend_segments(uint32 seqno, uint32 nsegments) 
{ 
  // first cancel retransmission timeout
  rxmit_timer_count = 0;

  // send up to nsegments
  uint32 bytes_sent = 0;
  nsegments = mymin(nsegments, (cwnd - sndwnd->lengthTo(seqno))/mss);
  while(nsegments > 0)
  {
    // figure out minimum sized segment; minimum between mss and the
    // amount of data to send in window
    uint32 nbytes = mymin(mss, recover_seq - seqno);
    if(nbytes > 0)
    {
      send_data(seqno, nbytes, 0, rcvwnd->expect(), false, true);
      seqno += nbytes;
      bytes_sent += nbytes;
      nsegments--;
    } else break;
  }

  // update rxmit_seq if nesessary
  rxmit_seq = seqno;

  // return nubmer for bytes sent
  return bytes_sent;
}

void TCPSession::acknowledge(bool nodelay)
{
  if(tcp_master->isDelayedAck() && !nodelay && !delayed_ack)
  {
    // put ack on delayed ack queue
    send_delay_ack();
  }
  else
  {
    // send the ack
    send_data(sndwnd->firstUnused(), 0, TCPMessage::TCP_FLAG_ACK,
	      rcvwnd->getExpectedSeqno(), false, false);

    // if delayed ack, we should cancel it
    if(tcp_master->isDelayedAck()) cancel_delay_ack();
  }
}

void TCPSession::timeout_resend()
{
  switch(state) {
  case TCP_STATE_SYN_SENT:
    // retransmit the syn segment
    send_data(sndwnd->firstUnused(), 0, TCPMessage::TCP_FLAG_SYN, 
	      rcvwnd->expect(), false, true);
    break;
    
  case TCP_STATE_SYN_RECEIVED:
    // retransmit the syn and ack segment
    send_data(sndwnd->firstUnused(), 0, TCPMessage::TCP_FLAG_SYN | TCPMessage::TCP_FLAG_ACK,
	      rcvwnd->expect(), false, true);
    break;
    
  case TCP_STATE_ESTABLISHED:
  case TCP_STATE_CLOSE_WAIT:
    // retransmit data segment
    switch(tcp_master->getVersion()) {
    case TCPMaster::TCP_VERSION_TAHOE:
      resend_segments(sndwnd->start(), INT_MAX);
      break;      
    case TCPMaster::TCP_VERSION_RENO:
    case TCPMaster::TCP_VERSION_NEW_RENO:
      resend_segments(sndwnd->start(), 1);
      break;
    case TCPMaster::TCP_VERSION_SACK:
      sack_resend_segments(true);
      break;
    default:
      assert(0);
    }
    break;
    
  case TCP_STATE_CLOSING:
  case TCP_STATE_FIN_WAIT_1:
  case TCP_STATE_LAST_ACK:
    // resend the FIN segment
    send_data(sndwnd->firstUnused/*next*/(), 0, TCPMessage::TCP_FLAG_FIN, 
	      rcvwnd->expect(), false, true);
    break;
    
  default:  
    // TCP_STATE_LISTEN, TCP_STATE_CLOSED, TCP_STATE_TIME_WAIT,
    // or TCP_STATE_FIN_WAIT_2
    error_quit("PANIC: TCPSession::timeout_resend(), unexpected state: %d.\n", state);
  }
}

uint32 TCPSession::calc_advertised_wnd()
{
  // first check the free space in the buffer
  uint32 window = rcvwnd->freeInBuffer(); 
  if(state < TCP_STATE_ESTABLISHED) return window;

  // if silly-window-syndrome-avoidance algorithm is used, run it
  if(window * 4 < rcvwnd->bufferSize() || window < mss)
    window = 0;

  // never shrink an already advertised window
  window = mymax(window, rcvwnd->size());

  // update the current receive window
  int incremental = window - rcvwnd->size();
  if(incremental > 0) rcvwnd->expand(incremental);

  return window;
}

void TCPSession::send_delay_ack()
{
  delayed_ack = true;
  tcp_master->enableFastTimeout(this);
}

void TCPSession::cancel_delay_ack()
{
  tcp_master->disableFastTimeout(this);
  delayed_ack = false;
}

}; // namespace s3fnet
}; // namespace s3f

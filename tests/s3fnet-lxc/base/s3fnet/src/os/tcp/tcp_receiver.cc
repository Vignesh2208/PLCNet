/**
 * \file tcp_receiver.cc
 * \brief Methods for the receiver side of the TCPSession class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "os/tcp/tcp_session.h"
#include "util/errhandle.h"
#include "net/host.h"


#ifdef TCP_DEBUG
#define TCP_DUMP(x) printf("TCPSESS: "); x
#else
#define TCP_DUMP(x)
#endif

namespace s3f {
namespace s3fnet {

uint32 TCPSession::appl_recv(uint32 length, byte* msg)
{
  assert(length > 0);
  uint32 got = rcvwnd->generate(length, msg);
  if(got == 0)
  {
    // if no data available, the socket will be suspended waiting for
    // the DATA_AVAILABLE signal, we set the receive parameters so
    // that the socket will be notified when message arrives
    rcvwnd->setRecvParameters(length, msg);

    // this function will be triggered from the socket layer. If we do
    // it once, we don't need to do it again.
    clear_app_state(SocketSignal::DATA_AVAILABLE);
  }
  return got;
}

void TCPSession::receive(TCPMessage* tcphdr)
{
  TCP_DUMP(printf("[host=\"%s\"] %s: receive(), flags=[%c%c%c%c%c%c] "
		  "seqno=%u, ackno=%u, wsize=%u, len=%u [state=%s]\n",
		  tcp_master->inHost()->nhi.toString(),
		  tcp_master->getNowWithThousandSeparator(),
		  (tcphdr->flags & TCPMessage::TCP_FLAG_SYN) ? 'S' : '.',
		  (tcphdr->flags & TCPMessage::TCP_FLAG_FIN) ? 'F' : '.',
		  (tcphdr->flags & TCPMessage::TCP_FLAG_RST) ? 'R' : '.',
		  (tcphdr->flags & TCPMessage::TCP_FLAG_ACK) ? 'A' : '.',
		  (tcphdr->flags & TCPMessage::TCP_FLAG_URG) ? 'U' : '.',
		  (tcphdr->flags & TCPMessage::TCP_FLAG_PSH) ? 'P' : '.',		
		  tcphdr->seqno, tcphdr->ackno, tcphdr->wsize,
		  tcphdr->payload() ? ((DataMessage*)tcphdr->payload())->realByteCount() : 0,
		  get_state_name(state)));

  // if true ack will be sent immediately
  bool must_ack = false;
  int signal = 0;

  // if it's RST segment, reset the connection
  if(tcphdr->flags & TCPMessage::TCP_FLAG_RST)
  {
    if (state != TCP_STATE_LISTEN) reset();
    return;
  }

  // if it's a SYN segment, handle it
  if(tcphdr->flags & TCPMessage::TCP_FLAG_SYN)
  {
    process_SYN(tcphdr, signal);
    if(state == TCP_STATE_SYN_SENT) must_ack = true;
  }

  // if it's a FIN segment, handle it
  if(tcphdr->flags & TCPMessage::TCP_FLAG_FIN)
  {
    process_FIN(signal);
    must_ack = true;
  }

  // handle new data
  DataMessage* payload = (DataMessage*)tcphdr->payload();
  if((payload && payload->realByteCount() > 0) &&  // there is new data coming
     (state == TCP_STATE_ESTABLISHED || state == TCP_STATE_FIN_WAIT_1 || 
      state == TCP_STATE_FIN_WAIT_2))
  {
    if(process_new_data(payload, tcphdr->seqno))
      must_ack = true;
    
    // collect the bytes report to the socket layer once we received
    // the expected bytes
    if(rcvwnd->available())
    {
      signal |= SocketSignal::DATA_AVAILABLE;
      rcvwnd->resetRecvParameters();
    }
  }

  // if it's an ACK segment, handle it
  if(tcphdr->flags & TCPMessage::TCP_FLAG_ACK)
  {
    if(process_ACK(tcphdr, signal))  must_ack = true;
  }

  if(state == TCP_STATE_CLOSED) return;

  // if we got a non-zero sized packet or must ack immediately then
  // send an ACK
  if(must_ack || (payload && payload->realByteCount() > 0))
    acknowledge(must_ack); 
  
  // check whether to send data
  if(state == TCP_STATE_ESTABLISHED || state == TCP_STATE_CLOSE_WAIT)
  {
    // figure how much data we could send out
    uint32 len_to_send = sndwnd->canSend();
    
    // check whether we can send out packets.
    if(len_to_send > 0)
    {
      uint32 seqno = sndwnd->firstUnused();
      segment_and_send(seqno, len_to_send);      
      if(sndwnd->needToRequest())
      {
    	signal |= SocketSignal::OK_TO_SEND;
      }
    }
  }
  
  if(close_issued && state == TCP_STATE_ESTABLISHED)
  {
    // if the upper layer has issued close, check whether
    if(sndwnd->empty() && sndwnd->dataInBuffer() == 0)
    {
      signal |= init_state_fin_wait_1();
    }
  }

  //printf("state=%d, signal=%d, close_issued=%d\n", state, signal, close_issued);

  // if there is a signal to send to application then send it ghyan
  // for further consideration!
  if(signal != SocketSignal::NO_SIGNAL) wake_app(signal);
}

void TCPSession::process_SYN(TCPMessage* tcphdr, int& signal) 
{
  switch(state) {
  case TCP_STATE_SYN_SENT:
    rcvwnd->setStart(tcphdr->seqno);
    rcvwnd->setSYN(true);
    nrxmits = 0;
    rcvwnd_size = tcphdr->wsize; 
    return;
    
  case TCP_STATE_SYN_RECEIVED:
    // we are not supposed to receive it
    return;
    
  case TCP_STATE_LISTEN:
    // Initialize the receive window
    rcvwnd->setStart(tcphdr->seqno);
    rcvwnd->setSYN(true);
    
    // Get initial sequence no and initialize Send Window
    sndwnd->setStart(tcp_master->getISS());
    rcvwnd_size = tcphdr->wsize;
    
    // We may want to signal the above application about that we have
    // received the first SYN segment. For further consideration.
    signal |= SocketSignal::FIRST_CONNECTION;

    signal |= init_state_syn_received();
    break;
    
  default:
    // for all other states, just ignore SYN packet because it may be
    // an retransmitted SYN packet sent by the other side.
    break;
  }
}

void TCPSession::process_FIN(int& signal)
{
  switch(state) {
  case TCP_STATE_ESTABLISHED:
    rcvwnd->setFIN(true);    
    // the other side wants to end the connection, we want to finish
    // it too; so it's simultaneous closing.
    if(close_issued) simultaneous_closing = true;
    signal |= init_state_close_wait();
    break;

  case TCP_STATE_CLOSE_WAIT:
    // at this point we have received a FIN, so if nothing in receive
    // window, there won't be more, so signal EOF to application
    if(rcvwnd->empty())
    {
      clear_app_state(SocketSignal::DATA_AVAILABLE);
      signal |= SocketSignal::SOCK_EOF;
    }
    else signal |= SocketSignal::DATA_AVAILABLE | SocketSignal::SOCK_EOF;
    break;
    
  case TCP_STATE_FIN_WAIT_1:
    rcvwnd->setFIN(true); // FIN byte
    // this state may be changed once ACK is handled
    signal |= init_state_closing();
    break;
    
  case TCP_STATE_FIN_WAIT_2:
    rcvwnd->setFIN(true); // FIN byte
    signal |= init_state_time_wait();
    break;
    
  default:
    // other states, do nothing right now
    break;
  }
}

bool TCPSession::process_ACK(TCPMessage* tcphdr, int& signal)
{
  switch(state) {
  case TCP_STATE_CLOSED:
    error_quit("PANIC: TCPSession::process_ACK(), state=%s.\n", get_state_name(state));
    return false;
    
  case TCP_STATE_LISTEN:
    // shouldn't be receiving ACK's yet
    send_data(tcphdr->ackno, 0, TCPMessage::TCP_FLAG_RST, rcvwnd->expect(), false, false);
    return false;
    
  case TCP_STATE_SYN_SENT:
    // illegal acknowledgment
    if(tcphdr->ackno != sndwnd->next())
    {
      send_data(tcphdr->ackno, 0, TCPMessage::TCP_FLAG_RST, rcvwnd->expect(), false, false);
      reset();
      return false;
    }
    process_acks(tcphdr);
    if(tcphdr->flags & TCPMessage::TCP_FLAG_SYN)
    {
      sndwnd->setSYN(false);
      signal |= init_state_established();
    }
    else signal |= init_state_syn_received();
    return false;
    
  case TCP_STATE_SYN_RECEIVED:
    if(tcphdr->ackno != sndwnd->next())
    {
      send_data(tcphdr->ackno, 0, TCPMessage::TCP_FLAG_RST, rcvwnd->expect(), false, false);
      reset();
      return false;
    }    
    if(process_acks(tcphdr))  return true;
    sndwnd->setSYN(false); // SYN byte
    signal |= init_state_established();
    return false;
    
  case TCP_STATE_ESTABLISHED:
  case TCP_STATE_CLOSE_WAIT:
    return process_acks(tcphdr);
    
  case TCP_STATE_LAST_ACK:
    if(tcphdr->ackno == sndwnd->next())
    {
      rxmit_timer_count = 0;
      sndwnd->setFIN(false);  // ACK our FIN
      signal |= init_state_closed();
      // when connection is closed, we release the session
      signal |= SocketSignal::CONN_CLOSED | SocketSignal::SESSION_RELEASED;
      return false;
    }
    else return true;   // illegal packet
    
  case TCP_STATE_FIN_WAIT_1:
    if(tcphdr->ackno == sndwnd->next())
    {
      sndwnd->setFIN(false); // our FIN was acked
      nrxmits = 0;      
      if(tcphdr->flags & TCPMessage::TCP_FLAG_FIN)
      {
    	rcvwnd->setFIN(true); // FIN byte
    	signal |= init_state_time_wait();
      }
      else signal |= init_state_fin_wait_2();
    }
    return false;
    
  case TCP_STATE_FIN_WAIT_2:
    return false;
    
  case TCP_STATE_CLOSING:
    if(tcphdr->ackno == sndwnd->next())
    {
      rxmit_timer_count = 0;
      sndwnd->setFIN(false);  // ACK our FIN
      signal |= init_state_time_wait();
    } 
    return false;
    
  case TCP_STATE_TIME_WAIT: 
    if(tcphdr->ackno == sndwnd->next())
    {
      msl_timer_count = 0;
    }
    return false;
  }

  return false;
}

bool TCPSession::process_new_data(DataMessage* payload, uint32 seqno) 
{
  // check whether the seqno falls in the window
  if(rcvwnd->within(seqno))
  {
    // URG and PSH are unsupported (RFC 793).
    rcvwnd->addToBuffer(payload, seqno); 
    return false;
  }
  else return true;  // illegal data
}

bool TCPSession::process_acks(TCPMessage* tcphdr)
{
  uint32 snd_una = sndwnd->start();

  // check if ACK is not within our window. if so, send an immediate
  // ACK and drop the segment
  if(tcphdr->ackno < snd_una || tcphdr->ackno > sndwnd->next())
    return true;

  // check to see if we have a duplicate ACK
  if(tcphdr->ackno == snd_una &&
     tcphdr->wsize == rcvwnd_size &&
     sndwnd->used() > 0)
  {
    process_dup_acks();
    return false;
  } 

  // update the remote window size
  update_remote_window_size(tcphdr->wsize);
  
  // we really got new data acked
  uint32 acked;
  if((acked = tcphdr->ackno - snd_una) > 0)
    process_new_acks(tcphdr, acked);
  
  // reset duplicate ACK count
  ndupacks = 0;
  return false;
}

void TCPSession::process_new_acks(TCPMessage* tcphdr, uint32 acked)
{  
  if(state == TCP_STATE_ESTABLISHED || 
     state == TCP_STATE_CLOSE_WAIT)
  {
    // shift the window
    sndwnd->release(acked);
  }
  
  // As implemented in Java SSFNet, when new data got acked, first
  // update retransmission timeout, and then update rtt. Is it right?

  // we don't need the retransmission timer any more
  rxmit_timer_count = 0;
  
  // if there's still packet on the fly, we should reset the timeout
  if(sndwnd->used() > 0)
  {
    // reschedule timer
    rxmit_timer_count = timeout_value();
  }

  // update timers. if data acked was RXMIT data then do not update
  // (Karn's algorithm) Smoothed RTT and Dev calculations
  if(rtt_count && (tcphdr->ackno > measured_seq))
  {
    // Jakobson's Algorithm for timers (1988)
    rtt_measured = rtt_count - 1;
    update_timeout();
  }
  
  if(state != TCP_STATE_ESTABLISHED && 
     state != TCP_STATE_CLOSE_WAIT)
    return;

  // indicate whether it's the first partial ack
  bool is_FirstNonDupACK;
  if(ndupacks >= TCP_DEFAULT_MAX_DUPACKS)
  {
    // when the first non-duplicate ACK has arrived, enter fast
    // recovery
    is_FirstNonDupACK = true;
    
    switch(tcp_master->getVersion()) {
    case TCPMaster::TCP_VERSION_TAHOE:
      // do nothing, just go to slow start!
      break;
      
    case TCPMaster::TCP_VERSION_RENO:
      // reset congestion window and go to congestion avoidance
      cwnd = mymin(ssthresh, tcp_master->getMaxConWnd());      
      // we check whether there are new data to send out we don't need
      // to resend any segments here
      break;
      
    case TCPMaster::TCP_VERSION_NEW_RENO:
      // still stay in fast recovery, until acks of new data received
      // we use fast_recovery to indicate it's still in fast
      // recovery
      fast_recovery = true;
      break;
      
    case TCPMaster::TCP_VERSION_SACK:
      fast_recovery = true;
      break;
      
    default:
      assert(0);
    }
  }
  else is_FirstNonDupACK = false;
  
  // if no new data has been confirmed, we have to resend segments
  if(tcphdr->ackno < recover_seq)
  {
    switch(tcp_master->getVersion()) {
    case TCPMaster::TCP_VERSION_TAHOE:
      // do nothing, just go to slow start!

      // The C++ TCP implementation is modified here to make it
      // compatible with the Java version (and the NS-2 version as
      // well). The congestion window is bumped by one each time a
      // non-duplicate packet is received (we're in the slow start
      // mood now). This happens no matter whether the ack is the
      // first non-duplicate ack or not! -jason.
      if(true || !is_FirstNonDupACK)
      {
    	check_cwnd();
      }
	  
      // we resend the segments - GO-BACK-N
      if(tcphdr->ackno > rxmit_seq)
      {
    	resend_segments(tcphdr->ackno, UINT_MAX);
      }
      else
      {
    	resend_segments(rxmit_seq, UINT_MAX);
      }
      break;
      
    case TCPMaster::TCP_VERSION_RENO:
      // go to congestion avoidance phase
      check_cwnd();

      if(timeout_loss)
      {
    	if(tcphdr->ackno > rxmit_seq)
    	{
    	  resend_segments(tcphdr->ackno, UINT_MAX);
    	}
    	else
    	{
    	  resend_segments(rxmit_seq, UINT_MAX);
    	}
      }
      break;
      
    case TCPMaster::TCP_VERSION_NEW_RENO:
      // it's still a partial ack, we resend one segment indicated by
      // the ack number
      resend_segments(tcphdr->ackno, 1); 
      // update congestion window: deflate the congestion window by
      // the amount of data acknowledged and then add one back and
      // send one new segment if permitted by the new value of cwnd
      cwnd -= (acked - mss);
      break;
      
    case TCPMaster::TCP_VERSION_SACK:	  
      sack_pipe -= mss;
      sack_send_in_fast_recovery();
      break;
      
    default:
      assert(0);
    }
  }
  
  // we have to update congestion window
  else
  {
    // no timeout loss 
    timeout_loss = false;	      
    if(fast_recovery && 
       (tcp_master->getVersion() == TCPMaster::TCP_VERSION_NEW_RENO || 
        tcp_master->getVersion() == TCPMaster::TCP_VERSION_SACK))
    {
      cwnd = mymin(ssthresh, mss + sndwnd->used());
      fast_recovery = false;

      // clear all blocks
      if(tcp_master->getVersion() == TCPMaster::TCP_VERSION_SACK)
    	  snd_scoreboard->clear_all_blocks();
    }
    else check_cwnd();
  }
}

void TCPSession::process_dup_acks()
{
  // another duplicate ack
  ndupacks++;
  if(ndupacks == TCP_DEFAULT_MAX_DUPACKS)
  {
    calc_threshold();

    // we remember the largest seqno already sent out
    recover_seq = sndwnd->firstUnused();
    // we think this packet is lost, so we disable measuring rtt
    rtt_count = 0;
   
    switch(tcp_master->getVersion()){
    case TCPMaster::TCP_VERSION_TAHOE:
      // in tahoe, fast recovery is off; tahoe fast rxmit resets
      // congestion window to mss and resend 1 segment
      cwnd = mss;
      resend_segments(sndwnd->start(), 1); 
      break;
      
    case TCPMaster::TCP_VERSION_RENO:
    case TCPMaster::TCP_VERSION_NEW_RENO:
      // in Reno, fast recovery is on
      cwnd = mymin(ssthresh + 3*mss, tcp_master->getMaxConWnd());
      resend_segments(sndwnd->start(), 1); 
      
      // more data to send out
      if(sndwnd->unused() >= mss)
      {
    	segment_and_send(sndwnd->firstUnused(),
    			mymin(sndwnd->unused(), sndwnd->dataInBuffer() - sndwnd->used()) );
      }
      break;
      
    case TCPMaster::TCP_VERSION_SACK:
      sack_pipe = cwnd - TCP_DEFAULT_MAX_DUPACKS;
      cwnd = mymin(ssthresh /* + 3*mss */, tcp_master->getMaxConWnd());
      // we resend a packet
      assert(sack_resend_segments(true));
      break;
      
    default:
      assert(0);
    }
  }

  else if(ndupacks > TCP_DEFAULT_MAX_DUPACKS)
  {
    // now we have more than TCP_DEFAULT_MAX_DUPACKS duplicate acks so if Reno
    // then increase the Congestion Window by mss and resend another
    // segment Do fast recovery
    switch(tcp_master->getVersion()){
    case TCPMaster::TCP_VERSION_TAHOE: 
      // do nothing
      break;
      
    case TCPMaster::TCP_VERSION_RENO:
    case TCPMaster::TCP_VERSION_NEW_RENO:
      cwnd = mymin(cwnd + mss, tcp_master->getMaxConWnd());
      if(sndwnd->unused() >= mss)
      {
    	segment_and_send(sndwnd->firstUnused(),
    			mymin(sndwnd->unused(), sndwnd->dataInBuffer() - sndwnd->used()) );
      }
      break;
      
    case TCPMaster::TCP_VERSION_SACK:
      sack_pipe -= mss;
      sack_send_in_fast_recovery();
      break;
      
    default:
      assert(0);
    }
  }
}

void TCPSession::check_cwnd()
{
#ifdef TCP_DEBUG  
  uint32 old_cwnd = cwnd;
#endif
  if(cwnd < ssthresh)
  { // slow start
    cwnd = mymin(cwnd + mss, tcp_master->getMaxConWnd());
    TCP_DUMP(printf("check_cwnd(), slow start: CW %u => %u.\n", old_cwnd, cwnd));
  }
  else
  { // congestion avoidance
    cwnd = mymin(cwnd + (mss*mss)/cwnd, tcp_master->getMaxConWnd());
    TCP_DUMP(printf("check_cwnd(), congestion avoidance: CW %u => %u.\n", old_cwnd, cwnd));
  }
}

void TCPSession::calc_threshold()
{
  ssthresh = mymax(mymin(cwnd, rcvwnd_size)/TCP_DEFAULT_THRESH_FACTOR, 2 * mss);
}

void TCPSession::update_remote_window_size(uint32 window_size)
{
  // update recv window size
  rcvwnd_size = tcp_master->getRcvWndSize();
}

}; // namespace s3fnet
}; // namespace s3f

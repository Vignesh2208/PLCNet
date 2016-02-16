/**
 * \file tcp_state.cc
 * \brief Implementation of the state change of TCPSession class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "os/tcp/tcp_session.h"
#include "net/host.h"

#ifdef TCP_DEBUG
#define TCP_DUMP(x) printf("TCPSESS: "); x
#else
#define TCP_DUMP(x)
#endif

namespace s3f {
namespace s3fnet {

int TCPSession::init_state_closed()
{
  state = TCP_STATE_CLOSED;

  // reset all the variables
  rxmit_timer_count  = 0;
  msl_timer_count  = 0;
  rcvwnd_size = tcp_master->getRcvWndSize();
  cwnd = tcp_master->getMSS();
  ssthresh = tcp_master->getInitThresh();
  mss = tcp_master->getMSS();
  rtt_smoothed = 0;
  rtt_count = 0;
  rtt_var = (int)(rint(3 / TCP_DEFAULT_SLOW_TIMEOUT))<<TCP_DEFAULT_RTTVAR_SHIFT;
  rxmit_timeout_double = initial_timeout();
  rxmit_timeout = tcp_master->inHost()->d2t(rxmit_timeout_double, 0);
  nrxmits = 0;
  ndupacks = 0;

  // cancel delay ack
  cancel_delay_ack();
 
  // release any buffers that might be allocated
  deallocate_buffers();

  tcp_master->setIdle(this);

  return 0;
}

int TCPSession::init_state_syn_sent()
{
  state = TCP_STATE_SYN_SENT;

  nrxmits = 0;
  tcp_master->setConnected(this);

  // send out SYN packet
  send_data(sndwnd->firstUnused(), 0, TCPMessage::TCP_FLAG_SYN, rcvwnd->expect(), true, true);
  sndwnd->setSYN(true);

  return 0;
}

int TCPSession::init_state_syn_received()
{
  assert(state != TCP_STATE_CLOSED);
  bool myListen = (state == TCP_STATE_LISTEN);

  state = TCP_STATE_SYN_RECEIVED;
  
  nrxmits = 0;
  tcp_master->setConnected(this);

  if(myListen)
  {
    sndwnd->setSYN(true);
    send_data(sndwnd->firstUnused(), 0, TCPMessage::TCP_FLAG_SYN | TCPMessage::TCP_FLAG_ACK,
	      rcvwnd->expect(), true, true);
  }
  else
  {
    send_data(sndwnd->next(), 0, TCPMessage::TCP_FLAG_ACK, rcvwnd->expect(), false, false);
  }

  return 0;
}

int TCPSession::init_state_listen()
{
  // set the session as listening.
  tcp_master->setListening(this);
  
  // if the previous state is not CLOSED,
  // we need to initialize the session state.
  if(state != TCP_STATE_CLOSED)
  {
    sndwnd->reset();
    rcvwnd ->reset();
    rcvwnd_size = tcp_master->getRcvWndSize();
    mss = tcp_master->getMSS();
    cwnd = mss;
    ssthresh = tcp_master->getInitThresh();  
    cancel_delay_ack();
    ndupacks = 0;
  }

  // set the state id.
  state = TCP_STATE_LISTEN;

  // cancel any timers
  rxmit_timer_count = 0;
  msl_timer_count = 0;

  return 0;
}

int TCPSession::init_state_established()
{
  state = TCP_STATE_ESTABLISHED;
  close_issued = false;
  simultaneous_closing = false;
  ssthresh = tcp_master->getInitThresh();  
  recover_seq = sndwnd->start();
  assert(!sndwnd->synAtFront());
  assert(!sndwnd->finAtFront());

  return SocketSignal::OK_TO_SEND;
}

int TCPSession::init_state_close_wait()
{
  int signal = 0;

  state = TCP_STATE_CLOSE_WAIT;
  rxmit_timer_count = 0;
 
  // at this point we have received a FIN, so if nothing in receive
  // window, there won't be, so signal EOF to application
  if(rcvwnd->empty())
  {
    clear_app_state(SocketSignal::DATA_AVAILABLE);
    signal |= SocketSignal::SOCK_EOF;
  }
  else
  {
    signal |= (SocketSignal::DATA_AVAILABLE | SocketSignal::SOCK_EOF);
  }

  return signal;
}

int TCPSession::init_state_last_ack()
{
  state = TCP_STATE_LAST_ACK;

  if(tcp_master->isDelayedAck() && delayed_ack)
  {
    send_data(sndwnd->next(), 0, TCPMessage::TCP_FLAG_ACK, rcvwnd->expect(), false, false);
    cancel_delay_ack();
  }
  
  clear_app_state(SocketSignal::OK_TO_SEND | SocketSignal::OK_TO_CLOSE);
  return 0;
}

int TCPSession::init_state_fin_wait_1()
{
  state = TCP_STATE_FIN_WAIT_1;

  // make sure nothing to send out 
  assert(sndwnd->empty() && sndwnd->dataInBuffer() == 0);
 
  // Reset this since there is no data in window.
  // In fact if its not 0 there's probably a bug.
  nrxmits = 0;
 
  // Send a fin and possibly ack any left over delayed acks
  if(tcp_master->isDelayedAck() && delayed_ack)
  {
    send_data(sndwnd->firstUnused(), 0, 
	      TCPMessage::TCP_FLAG_FIN | TCPMessage::TCP_FLAG_ACK,
	      rcvwnd->expect(), true, true);
    cancel_delay_ack();
    tcp_master->disableFastTimeout(this);
  }
  else
  {
    send_data(sndwnd->firstUnused(), 0, TCPMessage::TCP_FLAG_FIN, 
	      rcvwnd->expect(), true, true);
  }
  
  sndwnd->setFIN(true);
 
  // Now if established state received a fin while was waiting to dump
  // its buffer, then next state will be TCP_STATE_CLOSING.
  if(simultaneous_closing)  return init_state_closing();
  else return 0;
}

int TCPSession::init_state_fin_wait_2()
{  
  state = TCP_STATE_FIN_WAIT_2;

  // schedules timeout so we don't wait forever
  rxmit_timer_count = 0;    

  return 0;
}

int TCPSession::init_state_closing()
{
  int signal = 0;

  state = TCP_STATE_CLOSING;

  if(rcvwnd->empty())
  {
    clear_app_state(SocketSignal::DATA_AVAILABLE);
    signal |= SocketSignal::SOCK_EOF;
  }

  return signal;
}

int TCPSession::init_state_time_wait()
{
  state = TCP_STATE_TIME_WAIT;
 
  // set up the 2*msl timeout
  msl_timer_count = TCP_DEFAULT_MSL_TIMEOUT_FACTOR * tcp_master->getMSL();

  return 0;
}

}; // namespace s3fnet
}; // namespace s3f

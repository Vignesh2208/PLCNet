/**
 * \file tcp_timer.cc
 * \brief Methods for managing timers in the TCPSession class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "os/tcp/tcp_session.h"
#include "net/host.h"

namespace s3f {
namespace s3fnet {

#ifdef TCP_DEBUG
#define TCP_DUMP(x) printf("TCPSESS: "); x
#else
#define TCP_DUMP(x)
#endif

// various timer ids and timeout values
#define RETX_TIMER                    0
#define PERSIST_TIMER                 1
#define KEEP_ALIVE_TIMER              2
#define TWO_MSL_TIMER                 3

static int tcp_backoff[] =
  { 1, 2, 4, 8, 16, 32, 64, 64, 64, 64, 64, 64, 64 };

void TCPSession::slowTimeoutHandling()
{
  uint32 signal = 0;

  // update measured RTT
  if(rtt_count) rtt_count++;
  TCP_DUMP(printf("[host=\"%s\"] %s: slowTimeoutHandling(), rtt=%d, rxmit=%ld, msl=%ld.\n",
		  tcp_master->inHost()->nhi.toString(),
		  tcp_master->getNowWithThousandSeparator(), rtt_count,
		  rxmit_timer_count, msl_timer_count));

  // update retransmission timer. if the count is set as 0, then it's
  // not a valid timer, meaning that the timer is not set.
  if(rxmit_timer_count > 0)
  {
    rxmit_timer_count -= tcp_master->getSlowTimeout();
    
    // if it's time out, handle it.
    if(rxmit_timer_count <= 0)
      signal |= slow_timeout_handling_helper(RETX_TIMER);
  }

  // update 2msl timer. 
  if(msl_timer_count > 0)
  {
    msl_timer_count -= tcp_master->getSlowTimeout();
    
    // if it's time out, handle it.
    if(msl_timer_count <= 0)
      signal |= slow_timeout_handling_helper(TWO_MSL_TIMER);
  }
  else if(state == TCP_STATE_TIME_WAIT)
  {
    signal |= slow_timeout_handling_helper(TWO_MSL_TIMER);
  }

  if(signal) wake_app(signal);
}

uint32 TCPSession::slow_timeout_handling_helper(uint32 timerid)
{
  uint32 signal = 0;
  if(timerid == RETX_TIMER)
  {
    // we should set clear off all "sacked" flags
    if(TCPMaster::TCP_VERSION_SACK == tcp_master->getVersion())
      snd_scoreboard->clear_all_blocks();
    
    // no longer measure RTT 
    rtt_count = 0;
    nrxmits++;  // this also takes care of Karn's algorithm
    recover_seq  = sndwnd->firstUnused();
    timeout_loss = true;
    backoff_timeout();
 
    if(nrxmits <= tcp_master->getMaxRxmit())
    {
      // Jacobson's Slow Start
      // 
      // Cut the threshold to 1/2 of mymin(CW, RWndSize) and reset
      // Congestion window
      calc_threshold();
      cwnd = mss;
      ndupacks = 0;

      // update rtt var (according to Java SSFNet)
      if(nrxmits > tcp_master->getMaxRxmit() / 4)
      {
    	rtt_var += (rtt_smoothed >> TCP_DEFAULT_RTT_SHIFT);
    	rtt_smoothed = 0;
      }

      // cancel retransmission timer
      rxmit_timer_count = 0;
      // we should resend the packet assumed to get lost
      timeout_resend();
    }
    else
    {
      // we have tried one too many times...connection must be dead
      send_data(sndwnd->firstUnused(), 0, TCPMessage::TCP_FLAG_RST, rcvwnd->expect(), false, false);
      reset();
    }
  }

  else if(timerid == TWO_MSL_TIMER)
  {
    assert(state == TCP_STATE_TIME_WAIT);
    signal |= (SocketSignal::CONN_CLOSED | 
	       SocketSignal::SESSION_RELEASED);
  }

  else assert(false);
  return signal;
}

void TCPSession::fastTimeoutHandling()
{
  if(delayed_ack)
  {
    send_data(sndwnd->firstUnused(), 0, TCPMessage::TCP_FLAG_ACK, rcvwnd->expect(), false, false);
    cancel_delay_ack();
  }
}

void TCPSession::update_timeout()
{  
  if(rtt_smoothed)
  {
    int delta = rtt_measured - (rtt_smoothed >> TCP_DEFAULT_RTT_SHIFT);
    if((rtt_smoothed += delta) <= 0)  rtt_smoothed = 1;
    if(delta < 0)  delta = -delta;
    delta -= (rtt_var >> TCP_DEFAULT_RTTVAR_SHIFT);
    if((rtt_var += delta) <= 0)  rtt_var = 1;
  }
  else
  {
    rtt_smoothed = (rtt_measured + 1) << TCP_DEFAULT_RTT_SHIFT;
    rtt_var = (rtt_measured + 1) << (TCP_DEFAULT_RTTVAR_SHIFT - 1);
  }
  
  rxmit_timeout_double = mymax(mymin(tcp_master->getSlowTimeoutDouble() * ((rtt_smoothed >> TCP_DEFAULT_RTT_SHIFT) + rtt_var),
			      TCP_DEFAULT_RXMIT_MAX_TIMEOUT), TCP_DEFAULT_RXMIT_MIN_TIMEOUT);
  rxmit_timeout = tcp_master->inHost()->d2t(rxmit_timeout_double, 0);

  rtt_count = 0;
  nrxmits = 0;

  TCP_DUMP(printf("[host=\"%s\"] %s: update_timeout(), rtt_measured=%d, rtt_smoothed=%d, RTO=%ld.\n",
		  tcp_master->inHost()->nhi.toString(),
		  tcp_master->getNowWithThousandSeparator(), rtt_measured, rtt_smoothed,
		  timeout_value()));
}

void TCPSession::backoff_timeout()
{
  rxmit_timeout_double = mymax( mymin((tcp_master->getSlowTimeoutDouble() *
			       tcp_backoff[mymin(nrxmits, (uint32)TCP_DEFAULT_BACKOFF_LIMIT)] *
			       ((rtt_smoothed >> TCP_DEFAULT_RTT_SHIFT) + rtt_var)), TCP_DEFAULT_RXMIT_MAX_TIMEOUT),
			       TCP_DEFAULT_RXMIT_MIN_TIMEOUT);
  rxmit_timeout = tcp_master->inHost()->d2t(rxmit_timeout_double, 0);
}

bool TCPSession::checkIdle()
{
  int idle = sndwnd->used() == 0;
  if(idle && (now() - idle_time >= timeout_value()))
  {
    cwnd = mss;
    return true;
  }
  return false;
}

}; // namespace s3fnet
}; // namespace s3f

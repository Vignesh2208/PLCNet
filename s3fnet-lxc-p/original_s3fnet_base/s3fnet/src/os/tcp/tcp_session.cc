/**
 * \file tcp_session.cc
 * \brief Source file for the TCPSession class.
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

// all possible actions from application
#define APP_ACTION_RESET_PORT         0
#define APP_ACTION_SEND               1
#define APP_ACTION_RECEIVE            2
#define APP_ACTION_DISCONNECT         3
#define APP_ACTION_CONNECT            4
#define APP_ACTION_LISTEN             5

TCPSession::TCPSession(TCPMaster* master, int sock) :
  SocketSession(S3FNET_PROTOCOL_TYPE_TCP),
  tcp_master(master), state(0), socket(sock),
  rcvwnd(0), sndwnd(0), 
  rcvwnd_size(0), cwnd(0), ssthresh(0), mss(0), 
  ndupacks(0), nrxmits(0), persist_shift(0),
  rtt_smoothed(0), rtt_measured(0), rtt_count(0), rtt_var(0),
  rxmit_seq(0), measured_seq(0), recover_seq(0), sack_pipe(0),
  delayed_ack(false), fast_recovery(false), 
  timeout_loss(false), sack_permitted(false), 
  close_issued(false), simultaneous_closing(false),
  snd_scoreboard(0), rcv_scoreboard(0),
  rxmit_timeout(0), idle_time(0),    
  rxmit_timer_count(0), msl_timer_count(0)
{
  TCP_DUMP(printf("[host=\"%s\"] %s: new tcp session.\n",
		  tcp_master->inHost()->nhi.toString(), tcp_master->getNowWithThousandSeparator()));

  init_state_closed();
}

TCPSession::~TCPSession()
{
  if(rcvwnd) delete rcvwnd;
  if(sndwnd) delete sndwnd;
}

bool TCPSession::connect(IPADDR destip, uint16 destport)
{
  if(is_ok(APP_ACTION_CONNECT))
  {
    // Not allowing connection from LISTEN state. Can be added easily here,
    // by just switching the state back to CLOSED and connecting
    return active_open(destip, destport);
  }
  return false;
}

void TCPSession::disconnect()
{
  if(is_ok(APP_ACTION_DISCONNECT))
  {
    appl_close();
  }
}

int TCPSession::send(int length, byte* msg)
{
  if(is_ok(APP_ACTION_SEND))
  {
    return appl_send(length, msg);
  }
  return 0;
}

int TCPSession::recv(int length, byte* msg)
{
  if(is_ok(APP_ACTION_RECEIVE))
  {
    return appl_recv(length, msg);
  }
  return 0;
}

bool TCPSession::listen()
{
  if(is_ok(APP_ACTION_LISTEN))
  {
    passive_open();
    return true;
  }
  return false;
}

bool TCPSession::connected()
{
  return state >= TCP_STATE_ESTABLISHED;
}

bool TCPSession::setSource(IPADDR ip, uint16 port)
{
  src_ip   = ip; 
  src_port = port;
  return true;
}

  
void TCPSession::abort()
{
  if(sndwnd)
  {
    assert(rcvwnd);
    send_data(sndwnd->next(), 0, TCPMessage::TCP_FLAG_RST, rcvwnd->expect(), false, false);
  }
  reset();
}

void TCPSession::release()
{
  assert(tcp_master);
  tcp_master->deleteSession(this);
}

char* TCPSession::get_state_name(int state)
{
  switch(state)
  {
  	  case TCP_STATE_CLOSED: return "CLOSED";
  	  case TCP_STATE_LISTEN: return "LISTEN";
  	  case TCP_STATE_SYN_SENT: return "SYN_SENT";
  	  case TCP_STATE_SYN_RECEIVED: return "SYN_RECEIVED";
  	  case TCP_STATE_ESTABLISHED: return "ESTABLISHED";
  	  case TCP_STATE_CLOSE_WAIT: return "CLOSE_WAIT";
  	  case TCP_STATE_CLOSING: return "CLOSING";
  	  case TCP_STATE_FIN_WAIT_1: return "FIN_WAIT_1";
  	  case TCP_STATE_FIN_WAIT_2: return "FIN_WAIT_2";
  	  case TCP_STATE_LAST_ACK: return "LAST_ACK";
  	  case TCP_STATE_TIME_WAIT: return "TIME_WAIT";
  	  default: return "UNKNOWN";
  }
}

bool TCPSession::is_ok(int action)
{
  assert(tcp_master);
  switch(action) {	  
  case APP_ACTION_RESET_PORT:
    return (state == TCP_STATE_CLOSED);
  case APP_ACTION_SEND:
    return (state == TCP_STATE_ESTABLISHED || 
	    state == TCP_STATE_CLOSE_WAIT);
  case APP_ACTION_RECEIVE:
    return (state == TCP_STATE_ESTABLISHED ||
	    state == TCP_STATE_CLOSE_WAIT ||
	    state == TCP_STATE_FIN_WAIT_1 ||
	    state == TCP_STATE_FIN_WAIT_2);
  case APP_ACTION_DISCONNECT:
    return (state == TCP_STATE_ESTABLISHED || 
	    state == TCP_STATE_CLOSE_WAIT ||
	    state == TCP_STATE_TIME_WAIT);
  case APP_ACTION_CONNECT:
    return (state == TCP_STATE_CLOSED);
  case APP_ACTION_LISTEN:
    return (state == TCP_STATE_CLOSED);
  default:
    return false;
  }
}

bool TCPSession::active_open(const IPADDR destip, const uint16 destport)
{
  // set the destination
  dst_ip   = destip; 
  dst_port = destport;

  // allocate buffers
  allocate_buffers();

  // get/set iss
  assert(sndwnd && tcp_master);
  sndwnd->setStart(tcp_master->getISS());
  
  // go to TCP_STATE_SYN_SENT state
  init_state_syn_sent();

  return true;
}

bool TCPSession::passive_open()
{
  // allocate buffers
  allocate_buffers();
  
  // go to TCP_STATE_LISTEN state
  init_state_listen();

  return true;
}

void TCPSession::appl_close()
{
  assert(sndwnd);
  if(sndwnd->empty() && sndwnd->dataInBuffer() == 0)
  {
    // if there is nothing to send
    switch(state) {
    case TCP_STATE_ESTABLISHED:
      // go to state TCP_STATE_FIN_WAIT_1
      init_state_fin_wait_1();
      return;
      
    case TCP_STATE_CLOSE_WAIT:
      // send out FIN
      send_data(sndwnd->firstUnused(), 0, TCPMessage::TCP_FLAG_FIN, 
		rcvwnd->expect(), true, true);
      sndwnd->setFIN(true);
      // go to state TCP_STATE_LAST_ACK
      init_state_last_ack();
      return;
      
    case TCP_STATE_TIME_WAIT:
      wake_app(SocketSignal::CONN_CLOSED); 
      return;
    }
  }
  else
  {
    clear_app_state(SocketSignal::OK_TO_CLOSE);
    close_issued = true;
  }
}

void TCPSession::allocate_buffers()
{
  // allocate send buffer
  sndwnd = new TCPSendWindow(0, tcp_master->getSndBufSize(), tcp_master->getSndWndSize());
  assert(sndwnd);
  
  // allocate receive buffer
  rcvwnd = new TCPRecvWindow(0, tcp_master->getRcvWndSize());
  assert(rcvwnd);
}

void TCPSession::deallocate_buffers()
{
  if(rcvwnd) { delete rcvwnd; rcvwnd = 0; }
  if(sndwnd) { delete sndwnd; sndwnd = 0; }
}
  
void TCPSession::reset()
{
  // go to closed state
  init_state_closed();

  // signaling CONN_RESET might prompt the upper layer to delete this
  // session. therefore, after this, we should not do other things any
  // more, because all dats structures might be invalid.
  wake_app(SocketSignal::CONN_RESET);
}

void TCPSession::wake_app(int signal)
{
  SocketSignal socksig(socket, signal);
  if(signal & SocketSignal::OK_TO_SEND) 
    socksig.nbytes = sndwnd->dataBuffered();
  if(signal & SocketSignal::DATA_AVAILABLE)
  {
    socksig.nbytes = rcvwnd->dataReceived();
  }
  tcp_master->getSocketMaster()->control(SOCK_CTRL_SET_SIGNAL, &socksig, NULL);
}

void TCPSession::clear_app_state(int signal)
{
  SocketSignal socksig(socket, signal);
  tcp_master->getSocketMaster()->control(SOCK_CTRL_CLEAR_SIGNAL, &socksig, NULL);
}

}; // namespace s3fnet
}; // namespace s3f

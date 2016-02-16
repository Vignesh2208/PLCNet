/**
 * \file tcp_master.h
 * \brief Header file for the TCPMaster class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __TCP_MASTER_H__
#define __TCP_MASTER_H__

#include "os/socket/socket_session.h"
#include "os/tcp/tcp_init.h"
#include "util/shstl.h"

namespace s3f {
namespace s3fnet {

class TCPSession;

/**
 * \brief The TCP master.
 *
 * The TCP master is a protocol session that deals with all TCP
 * connections. It takes care of: (i) receiving TCP packets and
 * sending them to the corresponding TCP sessions; (2) sending TCP
 * packets the correspond to TCP sessions and pushing the packets down
 * to the IP layer.
 */
class TCPMaster : public SessionMaster {
  friend class TCPSession;
  
 public:
  /** tcp versions implemented in s3fnet */
  enum { 
    TCP_VERSION_TAHOE    = 0,
    TCP_VERSION_RENO     = 1, 
    TCP_VERSION_NEW_RENO = 2, 
    TCP_VERSION_SACK     = 3 
  };

  /** The constructor. */
  TCPMaster(ProtocolGraph* graph);
  
  /** The destructor. */
  virtual ~TCPMaster();

  /**
   * After a protocol is created, it needs to be configured using the
   * DML attributes specified in the DML file for this protocol. The
   * protocol should read, verify and apply the attributes.
   */
  virtual void config(s3f::dml::Configuration *cfg);
  
  /**
   * Return the protocol number. This specifies the relationship
   * between this protocol and the other protocols defined in the same
   * protocol stack.
   */
  virtual int getProtocolNumber() { return S3FNET_PROTOCOL_TYPE_TCP; }

  /**
   * This function will be called after the entire network is read in
   * and configured and after all the links have been connected, but
   * before the simulation starts running. The protocols can do any
   * kind of initialization they need which requires the network to be
   * connected. For TCP, during initialization phase, IP layer
   * immediately beneath this protocol will be figured out.
   */
  virtual void init();

  /** Create a new TCP session of the given socket id. */
  virtual SocketSession* createSession(int sock);

  /** Reclaim a TCP session. */
  void deleteSession(TCPSession* session);
  
  /** Change the state of the session to connected. */
  void setConnected(TCPSession* session);
  
  /** Change the state of the session to listening. */
  void setListening(TCPSession* session);
  
  /** Change the state of the session to idle. */
  void setIdle(TCPSession* session);
  
  /** Enable a TCP session for fast timeout handling. */
  void enableFastTimeout(TCPSession* session);
  
  /** Disable a TCP session from fast timeout handling. */
  void disableFastTimeout(TCPSession* session);

  /** Provide call_back functionality for implementing the slow timer in the TCPMaster
   *  this S3F process is used by waitFor() function */
  Process* slow_timer_callback_proc;
  /** Provide call_back functionality for implementing the fast timer in the TCPMaster
   *  this S3F process is used by waitFor() function */
  Process* fast_timer_callback_proc;
  /** The callback function registered with the slow_timer_callback_proc */
  void slow_timer_callback(Activation);
  /** The callback function registered with the fast_timer_callback_proc */
  void fast_timer_callback(Activation);
  /** Storing the data for the slow timer callback function. In this case, the TCPMaster object is stored. */
  ProtocolCallbackActivation* slow_timer_ac;
  /** Storing the data for the fast timer callback function. In this case, the TCPMaster object is stored. */
  ProtocolCallbackActivation* fast_timer_ac;

 protected:
  /** Send a message down the protocol stack to a lower layer. */
  virtual int push(Activation msg, ProtocolSession* hi_sess, void* extinfo = 0, size_t extinfo_size = 0);

  /** Receive a message up from the protocol stack. */
  virtual int pop(Activation msg, ProtocolSession* lo_sess,  void* extinfo = 0, size_t extinfo_size = 0);

  /** Disassociate a protocol session from managed lists. */
  void separate_session(TCPSession* sess);

  /** Finally remove sessions reclaimed using the deleteSession method. */
  void delete_defunct_sessions();

 public:
  // return the configuration parameters
  int         getVersion()        		{ return tcp_version; }
  uint32      getISS()            		{ return iss; }
  uint32      getMSS()            		{ return mss; }
  uint32      getRcvWndSize()     		{ return rcv_wnd_size; }
  uint32      getSndBufSize()     		{ return snd_buf_size; }
  uint32      getSndWndSize()     		{ return snd_wnd_size; }
  uint32      getMaxRxmit()       		{ return max_rxmit; }
  ltime_t	  getSlowTimeout()    		{ return slow_timeout; }
  double	  getSlowTimeoutDouble()    { return slow_timeout_double; }
  ltime_t 	  getFastTimeout()    		{ return fast_timeout; }
  double 	  getFastTimeoutDouble()    { return fast_timeout_double; }
  ltime_t 	  getIdleTimeout()    		{ return idle_timeout; }
  ltime_t 	  getMSL()            		{ return msl; }
  bool        isDelayedAck()      		{ return delayed_ack; }
  uint32      getMaxConWnd()      		{ return max_cong_wnd>0?max_cong_wnd:UINT_MAX; }
  uint32      getInitThresh()     		{ return TCP_DEFAULT_INIT_THRESH; }

 private:
  typedef S3FNET_SET(TCPSession*) TCP_SESSIONS_SET;

  /** TCP sessions that are listening. */
  TCP_SESSIONS_SET listening_sessions;

  /** TCP sessions that are connected with peers. */
  TCP_SESSIONS_SET connected_sessions;

  /** TCP sessions that are idle. */
  TCP_SESSIONS_SET idle_sessions;

  /** TCP Sessions to be called when fast timer expires. */
  TCP_SESSIONS_SET fast_timeout_sessions;
  
  /** TCP sessions to be deleted. */
  TCP_SESSIONS_SET defunct_sessions;
 
  /** To identify the TCP variant. */
  int tcp_version;

  /** Initial sequence number. */
  uint32 iss;

  /** Maximum segment size. */
  uint32 mss;

  /** Receive window size. */
  uint32 rcv_wnd_size;

  /** Send window size. */
  uint32 snd_wnd_size;

  /** Send buffer size. */
  uint32 snd_buf_size;

  /** Maximum number of retransmissions of a segment. */
  uint32 max_rxmit;

  /** Slow timeout interval. */
  ltime_t slow_timeout;
  double slow_timeout_double;

  /** Fast timeout interval. */
  ltime_t fast_timeout;
  double fast_timeout_double;

  /** Timeout interval for FIN_WAIT_2. */
  ltime_t idle_timeout;
  double idle_timeout_double;

  /** Maximum segment lifetime. */
  ltime_t msl;
  double msl_double;

  /** True if delayed acknowledgment is turned on. */
  bool delayed_ack;

  /** Maximum congestion window size. */
  uint32 max_cong_wnd;

  /** The time this TCP master protocol session starts. */
  ltime_t boot_time;
  double boot_time_double;

  /** The time window this TCP master protocol session starts; used to
      avoid the artificial synchrony in simulation. */
  ltime_t boot_time_window;
  double boot_time_window_double;

 private:
  /** The IP layer protocol session, located below this protocol
      session on the protocol stack. */
  ProtocolSession* ip_sess;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__TCP_MASTER_H__*/

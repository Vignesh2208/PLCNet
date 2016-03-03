/**
 * \file tcp_session.h
 * \brief Header file for the TCPSession class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __TCP_SESSION_H__
#define __TCP_SESSION_H__

#include "os/socket/socket_session.h"
#include "os/tcp/tcp_master.h"
#include "os/tcp/tcp_message.h"
#include "os/tcp/tcp_blocks.h"
#include "os/tcp/tcp_sndwnd.h"
#include "os/tcp/tcp_rcvwnd.h"

namespace s3f {
namespace s3fnet {

/**
 * \brief The TCP session.
 */
class TCPSession : public SocketSession {
 public:
  friend class TCPMaster;

  // all possible states of a tcp session; the order matters!
  enum {
    TCP_STATE_CLOSED       = 0,
    TCP_STATE_LISTEN       = 1,
    TCP_STATE_SYN_SENT     = 2,
    TCP_STATE_SYN_RECEIVED = 3,
    TCP_STATE_ESTABLISHED  = 4,
    TCP_STATE_CLOSE_WAIT   = 5,
    TCP_STATE_CLOSING      = 6,
    TCP_STATE_FIN_WAIT_1   = 7,
    TCP_STATE_FIN_WAIT_2   = 8,
    TCP_STATE_LAST_ACK     = 9,
    TCP_STATE_TIME_WAIT    = 10
  };

  /** The constructor. */
  TCPSession(TCPMaster* master, int sock);

  /** The destructor. */
  virtual ~TCPSession();

  /* Functions to be called by the socket interface. */

  /** Connect to a destination ip address and port number. Returns
      true if the connection has been established successfuly. */
  virtual bool connect(IPADDR destip, uint16 destport);
  
  /** Disconnect this session. */
  virtual void disconnect();

  /**
   * Send the given number of bytes. The msg parameter, if not NULL,
   * points to the message storage.  The method returns the number of
   * bytes that have truly been sent out. -1 means error.
   */
  virtual int send(int length, byte* msg = 0);

  /**
   * Receive a message of the given size. The msg parameter, if not
   * NULL, points to the buffer to store the message. The method
   * returns the number of bytes that have been truly received. -1
   * means error.
   */
  virtual int recv(int length, byte* msg = 0);

  /** Passive open. Returns true if there is an incoming connection
      that has been established successfully. */
  virtual bool listen();                                

  /** Whether a connection is made. Returns true if connected. */
  virtual bool connected();     

  /** Set the ip and the port number of the connection. Returns true
      if set up correctly. */
  virtual bool setSource(IPADDR ip, uint16 port);  

  /** Abort the session. */
  virtual void abort();

  /** Set socket. */
  virtual void setSocketID(int sock) { socket = sock; }

  /** Return the master protocol session. */
  virtual SessionMaster* getSessionMaster() { return tcp_master; }

  /** Release this session if unnecessary. */
  virtual void release();
  
  /* Functions to be called by the tcp master. */

  /** Receive the message popped up from the ip session. */
  void receive(TCPMessage* tcphdr);

  /** Slow timeout handler (in tcp_tmr.cc). */ 
  void slowTimeoutHandling();  

  /** Fast timeout handler (in tcp_tmr.cc). */
  void fastTimeoutHandling();

 private:
  /** Point to the tcp master. */
  TCPMaster* tcp_master;

  /** The current state of the tcp session. */
  int state;

  /** The socket id. */
  int socket;

  /** The receive window of the tcp session. */
  TCPRecvWindow* rcvwnd;

  /** The send window of the tcp session. */
  TCPSendWindow* sndwnd;

  /** Remote receive window size. */
  uint32 rcvwnd_size;
  
  /** Congestion window. */
  uint32 cwnd;

  /** Threshold size for updating the congestion window. */
  uint32 ssthresh;

  /** Maximum segment size. */
  uint32 mss;

  /** Number of consecutive duplicate ACKs received. */
  uint32 ndupacks;

  /** Number of consecutive packet retransmissions. */
  uint32 nrxmits;

  /** Left shift to get persist (exponential) timeout. */
  uint32 persist_shift;

  /** Smoothed round trip time. */
  int rtt_smoothed;

  /** Measured round trip time. */
  int rtt_measured;

  /** The current RTT count. */
  int rtt_count;

  /** Current estimated RTT variance. */
  int rtt_var;

  /** The last sequence number of retransmitted segment plus one. */
  uint32 rxmit_seq;

  /** The sequence number of the segment currently being measured. */
  uint32 measured_seq;

  /** The largest seqno sent out when loss happens. */
  uint32 recover_seq; 

  /** Indicate how much can be sent during fast recovery in SACK. */
  uint32 sack_pipe;

  /** True if the delayed ack is turned on. */
  bool delayed_ack;

  /** True if the session is still in the fast recovery mode. */
  bool fast_recovery;

  /** Whether it is a timeout loss. */
  bool timeout_loss;

  /** Whether SACK is permitted as indicated by the peer. */
  bool sack_permitted;

  /** Whether the upper layer has issued close. */
  bool close_issued;

  /** Whether it is a simultaneous closing. */
  bool simultaneous_closing;

  /** The score board for blocks sacked maintained by the sender side. */
  TCPBlockList* snd_scoreboard;

  /** The score board for the receiver side. */
  TCPBlockList* rcv_scoreboard;

  /** The current retransmission timeout. */
  ltime_t rxmit_timeout;
  double rxmit_timeout_double;

  /** Idle time for the other party. */
  ltime_t idle_time;
  double idle_time_double;

  /** Retransmission timer count. */
  ltime_t rxmit_timer_count;
  double rxmit_timer_count_double;


  /** The timer count that lasts 2*msl. */
  ltime_t msl_timer_count;
  double msl_timer_count_double;

 private:
  //
  // session helper [tcp_session.cc]
  //

  /** Translate the tcp state into a string. */
  static char* get_state_name(int state);

  /** Return true if the given action is ok at the time. */
  bool is_ok(int action);

  /** Active open. */
  bool active_open(IPADDR dst_ip, uint16 dst_port);

  /** Passive open. */
  bool passive_open();

  /** Close the connection. */
  void appl_close();

  /** Allocate both send and receive windows. */
  void allocate_buffers();

  /** Deallocate both send and receive windows. */
  void deallocate_buffers();

  /** Reset the session to CLOSED state and wake up application. */
  void reset();

  /** Wake application upon a signal. */
  void wake_app(int signal);

  /** Clear application signal bit(s). */
  void clear_app_state (int signal);

  //
  // TCP state change [tcp_state.cc]
  //

  /** Change to CLOSED state. */
  int init_state_closed();

  /** Change to SYN_SENT state. */
  int init_state_syn_sent();

  /** Change to SYN_RECEIVED state. */
  int init_state_syn_received();

  /** Change to LISTEN state. */
  int init_state_listen();

  /** Change to ESTABLISHED state. */
  int init_state_established();

  /** Change to CLOSE_WAIT state. */
  int init_state_close_wait();

  /** Change to LAST_ACK state. */
  int init_state_last_ack();

  /** Change to FIN_WAIT_1 state. */
  int init_state_fin_wait_1();

  /** Change to FIN_WAIT_2 state. */
  int init_state_fin_wait_2();

  /** Change to CLOSING state. */
  int init_state_closing();

  /** Change to TIME_WAIT state. */
  int init_state_time_wait();

  //
  // Data sending functions  [tcp_sender.cc]
  //

  /**
   * When the upper layer attempts to send out data, this function
   * will be called.  The message will be kept in the send window
   * until it will be sent out and got acknowledged by the
   * receiver. In addition, the message should be allocated at the
   * upper layer. In other words, the upper layer cannot allocate the
   * message from the stack.
   */
  uint32 appl_send(uint32 length, byte* msg);
  
  /**
   * Physically send the data to the IP layer. The method will call
   * the push function of the TCP master.
   */
  void send_data(uint32 seqno, uint32 datalen = 0, 
		 byte mask = 0, uint32 ackno = 0,
		 bool need_calc_rtt = false,
		 bool need_set_rextimeout = false);

  /** Segment and send data. */
  uint32 segment_and_send(uint32 seqno, uint32 length);

  /** Resend the segments. */
  uint32 resend_segments(uint32 seqno, uint32 nsegments);

  /** Generate acknowledgements; no delay is imposed if the nodelay
      parameter is true. */
  void acknowledge(bool nodelay);

  /** This function is called when retransmission timer expires. */
  void timeout_resend();

  /** Calculate the advertised window size. */
  uint32 calc_advertised_wnd();

  /** Set the session to send delayed ACK when next time the fast
      timer fires. */
  void send_delay_ack();

  /** Set the session to cancel sending delayed ACK when next timer
      the fast timer fires. */
  void cancel_delay_ack();

  //
  // Data receiving functions [tcp_receiver.cc]
  //

  /**
   * When the upper layer wants to receive data (length bytes) from
   * TCP, this function will be called. The msg parameter points the
   * message buffer, which can be NULL if only fake data is needed.
   */
  uint32 appl_recv(uint32 length, byte* msg);
  
  /** Process SYN packets. */
  void process_SYN(TCPMessage* tcphdr, int& signal);
  
  /** Process FIN packets. */
  void process_FIN(int& signal);

  /** Process ACK packets. */
  bool process_ACK(TCPMessage* tcphdr, int& signal);

  /** Process the ACKs that correspond to received TCP message, and
      update session states like RTT and retransmission timer. */
  bool process_acks(TCPMessage* tcphdr);

  /** Process data arrival. */
  bool process_new_data(DataMessage* payload, uint32 seqno);

  /** Process duplicate ACKs received. */
  void process_dup_acks();

  /** Process ACKs that respond to new data. */
  void process_new_acks(TCPMessage* tcphdr, uint32 acked);

  /** Check congestion window size. */
  void check_cwnd();

  /** Calculate the threshold. */
  void calc_threshold();

  /** update the remote receive window size. */
  void update_remote_window_size(uint32 window_size);

  //
  // Timer Update Functions [tcp_timer.cc]
  //

  /** Slow timeout handler with a given timer id. */
  uint32 slow_timeout_handling_helper(const uint32 timerid);

  /** Update the current smoothed RTT timeout with the new rtt
      measurment. */
  void update_timeout();

  /** Backoff the current timeout. */
  void backoff_timeout();

  /** An alias to get the current simulation time. */
  ltime_t now() { return tcp_master->getNow(); }
  
  /** Current smoothed RTT timeout value. */
  ltime_t timeout_value() { return rxmit_timeout; }
  double timeout_double_value() { return rxmit_timeout_double; }

  /** Return initial timeout value for the first timeout. */
  double initial_timeout()
  {
    return (((rtt_smoothed>>2) + rtt_var)>>1) * TCP_DEFAULT_SLOW_TIMEOUT;
  }
  
  /** Check whether the idle time is long enough to slow start. */
  bool checkIdle();

  //
  // selective retransmission [tcp_sack.cc]
  //

  /**
   * This method handles the situation when a sender tries to send a
   * packet. It sends the sack permitted option. At the same time, it
   * keeps which segments are sent out but have not yet been
   * acknowledged (maybe sacked) in the list of entries.
   */
  int sender_send_sack(int sack_flag, byte mask, byte*& options);

  /**
   * This method handles the situation when a receiver tries to send a
   * packet. It should consider sending the sack options. The method
   * returns the length to be added to the standard TCP header length.
   */
  int receiver_send_sack(int sack_flag, byte mask, uint32 ackno, byte*& options);

  /**
   * This method handles the situation when a sender receives sack
   * options. The information is kept accordingly.
   */
  void sender_recv_sack(TCPMessage* tcphdr);

  /**
   * This method handles the situation when a receiver receives a
   * packet. The method retrieves the sack permitted information. At
   * the same time, it updates the highest sequence number received so
   * far.
   */
  void receiver_recv_sack(TCPMessage* tcphdr);

  /** The method handles sending sack when fast recovery is
      enabled. */
  void sack_send_in_fast_recovery();

  /** When the sack option is set, this method is used for
      retransmissions. */
  bool sack_resend_segments(bool first_flag);
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__TCP_SESSION_H__*/

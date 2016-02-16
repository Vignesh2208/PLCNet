/**
 * \file tcp_client.h
 * \brief Header file for the TCPClientSession class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __TCP_CLIENT_H__
#define __TCP_CLIENT_H__

#include "os/base/protocol_session.h"
#include "net/ip_prefix.h"
#include "util/shstl.h"

namespace s3f {
namespace s3fnet {

class TCPClientSessionContinuation;
class TCPClientSessionCallbackActivation;
class SocketMaster;

/**
 * \brief The TCP client test protocol session.
 *
 * This is a test protocol that does not exist in reality. In a normal
 * situation, the protocol waits for a period of time before it
 * randomly selects a TCP server and tries to retrieve a number of
 * bytes from the server. The process repeats until simulation ends.
 */
class TCPClientSession: public ProtocolSession {
  friend class TCPClientSessionContinuation;

 public:
  /** The constructor. */
  TCPClientSession(ProtocolGraph* graph);

  /** The destructor. */
  virtual ~TCPClientSession();

  /** Configure the TCP client test protocol session. */
  virtual void config(s3f::dml::Configuration *cfg);

  /**
   * Return the protocol number.
   * The number returned specifies the relationship
   * between this protocol and the other protocols defined in the same
   * protocol stack.
   * @return Protocol number for blocking tcp client
   */
  virtual int getProtocolNumber() { return S3FNET_PROTOCOL_TYPE_TCPTEST_CLIENT; }

  /** Initialize this protocol session. */
  virtual void init();

  // for start timer callback

  /**
   * Process object for start timer callback.
   * Provide call_back functionality for implementing the start timer in the TCPClientSession
   *  this S3F process is used by waitFor() function
   */
  Process* start_timer_callback_proc;

  /**
  * The callback function registered with the start_timer_callback_proc
  */
  void start_timer_callback(Activation ac);

  /**
  * Activation object for start timer callback.
  * Stores the data for the callback function. In this case, the
  * TCPClientSession object is stored.
  */
  ProtocolCallbackActivation* start_timer_ac;

  /** check if start_once() is called */
  bool start_timer_called_once;

 protected:
  // functions representing different stages

  /** the first function to call for starting a communication at client side */
  void main_proc(int sample_off_time, ltime_t lead_time);

  /** first time to choose the fixed TCP server if the DML specifies to use fixed server */
  void start_once();

  /** start the socket communication, such as bind and connect, and choose random server if the DML specifies to use random server  */
  void start_on();

  /** function representing the server_connected stage */
  void server_connected(TCPClientSessionContinuation* const cnt);

  /** function representing the request_sent stage */
  void request_sent(TCPClientSessionContinuation* const cnt);

  /** function representing the data_received stage */
  void data_received(TCPClientSessionContinuation* const cnt);

  /** function representing the session_closed stage */
  void session_closed(TCPClientSessionContinuation* const cnt);

  /**
  * Get a server ip and port randomly from traffic description
  * @param server_ip the server IP address
  * @param server_port the server port
  * @return False if failed
  */
  bool get_random_server(IPADDR& server_ip, uint16& server_port);

  /**
  * Called if the file transfer takes too long.
  * @param sock the socket id
  */
  void timeout(int sock);

  /**
  * Push message to lower protocol layer.
  * This method should not be called; it is provided here to prompt
  * an error message if it's called accidentally.
  */
  virtual int push(Activation msg, ProtocolSession* hi_sess, void* extinfo = 0, size_t extinfo_size = 0);

  /**
  * Pop message to higher protocol layer.
  * This method should not be called; it is provided here to prompt
  * an error message if it's called accidentally.
  */
  virtual int pop(Activation msg, ProtocolSession* lo_sess, void* extinfo = 0, size_t extinfo_size = 0);

  /** Multiple instances of this protocol are allowed on the same protocol stack. */
  virtual int instantiation_type() { return PROT_MULTIPLE_INSTANCES; }

 private:
  // configurable parameters

  /**
  * Time before a session starts.
  */
  ltime_t start_time;

  /**
  * Time before a session starts.
  * Double representation of start_time
  */
  double start_time_double;

  /**
  * Size of random window before session starts.
  */
  ltime_t start_window;

  /**
  * Size of random window before session starts.
  * Double representation of start_window
  */
  double start_window_double;

  /**
  * Timeout before aborting a session
  */
  ltime_t user_timeout;

  /**
  * Off time between sessions.
  */
  ltime_t off_time;

  /**
  * Off time between sessions.
  * Double representation of off_time
  */
  double off_time_double;

  /**
  * Whether 1st session starts with an off time.
  */
  bool off_time_run_first;

  /**
  * Constant or exponential off time.
  */
  bool off_time_exponential;

  /**
  * Whether to find a random target.
  */
  bool fixed_server;

  /**
  * Size of the request sent to the server.
  */
  uint32 request_size;

  /**
  * Size of the file to be sent from the server.
  */
  uint32 file_size;

  /**
  * The starting port number for client sessions.
  */
  uint16 start_port;

  /**
  * Traffic server list name (default: forTCP)
  */
  S3FNET_STRING server_list;

  /**
  * Whether we print out the result or not.
  */
  bool show_report;

  // state variables

  /**
  * Points to the socket master.
  */
  SocketMaster* sm;

  /**
  * The ip address of the client (interface 0).
  */
  IPADDR client_ip;

  /**
  * Number of sessions initiated by this client (statistics).
  */
  uint32 nsess;

  /**
  * Server ip address of the current connection.
  */
  IPADDR server_ip;

  /**
  * Server port number of the current connection.
  */
  uint16 server_port;
};

/**
 *  Stores the data for the callback function.
 *  In this case, the TCPClientSession object and the
 *  TCPClientSessionContinuation object are stored.
 */
class TCPClientSessionCallbackActivation : public ProtocolCallbackActivation
{
  public:
    /**
    * Constructor
    */
	TCPClientSessionCallbackActivation(ProtocolSession* sess, TCPClientSessionContinuation* tcp_cnt) :
		ProtocolCallbackActivation(sess), cnt(tcp_cnt){};

    /*
    * Destructor
    */
	virtual ~TCPClientSessionCallbackActivation(){};

    /*
    * The Continuation that will maintain the app state
    */
	TCPClientSessionContinuation* cnt;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__TCP_CLIENT_H__*/

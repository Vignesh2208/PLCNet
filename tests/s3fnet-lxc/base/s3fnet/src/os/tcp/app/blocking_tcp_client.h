/**
 * \file blocking_tcp_client.h
 * \brief Header file for the BTCPClientSession class.
 *
 * authors : Lenny Winterrowd, Dong (Kevin) Jin
 */

#ifndef __BTCP_CLIENT_H__
#define __BTCP_CLIENT_H__

#include "os/base/protocol_session.h"
#include "net/ip_prefix.h"
#include "util/shstl.h"

namespace s3f {
namespace s3fnet {

class BTCPClientSessionContinuation;
class BTCPClientSessionCallbackActivation;
class SocketMaster;

/**
 * \brief The super-lightweight TCP client test protocol session.
 *
 * Differs from TCPClientSession in that this is designed to work with
 * nb_tcp_server. There is no request phase which means that the payload
 * size must be pre-programmed (in DML) with both the client and the server
 * instead of just the client (TCPClientSession). Was developed for testing
 * of blocking/non-blocking interaction.
 *
 * For most purposes, use TCPClientSession.
 *
 * This is a test protocol that does not exist in reality. In a normal
 * situation, the protocol waits for a period of time before it
 * randomly selects a TCP server and tries to retrieve a number of
 * bytes from the server. The process repeats until simulation ends.
 */
class BTCPClientSession: public ProtocolSession {
  friend class BTCPClientSessionContinuation;

 public:
  /** The constructor. */
  BTCPClientSession(ProtocolGraph* graph);

  /** The destructor. */
  virtual ~BTCPClientSession();

  /** Configure the TCP client test protocol session. */
  virtual void config(s3f::dml::Configuration *cfg);

  /**
   * Return the protocol number.
   * The number returned specifies the relationship
   * between this protocol and the other protocols defined in the same
   * protocol stack.
   * @return Protocol number for blocking tcp client
   */
  virtual int getProtocolNumber() { return S3FNET_PROTOCOL_TYPE_TCPTEST_BLOCKING_CLIENT; }

  /** Initialize this protocol session. */
  virtual void init();

  /** Process object for start timer callback */
  Process* start_timer_callback_proc;

  /** Callback function for start timer callback */
  void start_timer_callback(Activation ac);

  /** Activation object for start timer callback */
  ProtocolCallbackActivation* start_timer_ac;

  /**
  * Bool for start timer callback
  * Indicates that the start timer has already been established.
  */
  bool start_timer_called_once;

 protected:
  // functions representing different stages

  /**
  * The main process of the tcp_client
  * @param sample_off_time the time between requests
  * @param lead_time the initial delay
  */
  void main_proc(int sample_off_time, ltime_t lead_time);

  /**
  * Function run on the first start
  * Establishes callbacks/timers, etc.
  */
  void start_once();

  /**
  * Sets up the connection
  */
  void start_on();

  /**
  * Client is connected to the server
  * @param cnt the Continuation that stores the client state
  */
  void server_connected(BTCPClientSessionContinuation* const cnt);

  /**
  * Have received data from the server
  * @param cnt the Continuation that stores the client state
  */
  void data_received(BTCPClientSessionContinuation* const cnt);

  /**
  * Session has been closed
  * @param cnt the Continuation that stores the client state
  */
  void session_closed(BTCPClientSessionContinuation* const cnt);

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
* Class to start client actions at the correct time(s)
*/
class BTCPClientSessionCallbackActivation : public ProtocolCallbackActivation
{
  public:
    /**
    * Constructor
    */
	BTCPClientSessionCallbackActivation(ProtocolSession* sess, BTCPClientSessionContinuation* tcp_cnt) :
		ProtocolCallbackActivation(sess), cnt(tcp_cnt){};

    /*
    * Destructor
    */
	virtual ~BTCPClientSessionCallbackActivation(){};

    /*
    * The Continuation that will maintain the app state
    */
	BTCPClientSessionContinuation* cnt;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__BTCP_CLIENT_H__*/

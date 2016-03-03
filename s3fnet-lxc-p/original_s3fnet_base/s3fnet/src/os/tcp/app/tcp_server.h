/**
 * \file tcp_server.h
 * \brief Header file for the TCPServerSession class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __TCP_SERVER_H__
#define __TCP_SERVER_H__

#include "os/base/protocol_session.h"
#include "net/ip_prefix.h"

namespace s3f {
namespace s3fnet {

class TCPServerSessionCallbackActivation;
class TCPServerSessionContinuation;
class SocketMaster;

/**
 * \brief The TCP server test protocol session.
 *
 * This is a test protocol that does not exist in reality. In a normal
 * situation, the protocol waits for an incoming request from a TCP
 * client, spawns a process to handle the client connection, and then
 * sends the requested number of bytes to the client.
 */
class TCPServerSession: public ProtocolSession {
  friend class TCPServerSessionContinuation;

 public:
  /** The constructor. */
  TCPServerSession(ProtocolGraph* graph);

  /** The destructor. */
  virtual ~TCPServerSession();

  /** Configure the TCP server test protocol session. */
  virtual void config(s3f::dml::Configuration *cfg);

  /**
   * Return the protocol number.
   * The number returned specifies the relationship
   * between this protocol and the other protocols defined in the same
   * protocol stack.
   * @return Protocol number for blocking tcp client
   */
  virtual int getProtocolNumber() { return S3FNET_PROTOCOL_TYPE_TCPTEST_SERVER; }

  /** Initialize this protocol session. */
  virtual void init();

  // for start timer callback

  /**
   * Process object for start timer callback
   * Provide call_back functionality for implementing the start timer in the
   * TCPServerSession. This S3F process is used by waitFor() function.
   */
  Process* start_timer_callback_proc;

  /**
  * The callback function registered with the start_timer_callback_proc
  */
  void start_timer_callback(Activation ac);

  /**
  * Activation object for start timer callback.
  * Stores the data for the callback function. In this case, the
  * TCPServerSession object is stored.
  */
  ProtocolCallbackActivation* start_timer_ac;

 protected:
  // functions representing different stages

  /** start the TCP server */
  void start_on();

  /** function representing the handle_client stage */
  void handle_client(int socket);

  /** function representing the connection_accepted stage */
  void connection_accepted(TCPServerSessionContinuation* const cnt);

  /** function representing the request_received stage */
  void request_received(TCPServerSessionContinuation* const cnt);

  /** function representing the sending_data stage */
  void sending_data(TCPServerSessionContinuation* const cnt);

  /** function representing the data_sent stage */
  void data_sent(TCPServerSessionContinuation* const cnt);

  /** function representing the session_closed stage */
  void session_closed(TCPServerSessionContinuation* const cnt);

 private:
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

 private:
  // configurable parameters

  /**
  * Port number to receive incoming request.
  */
  uint16 server_port;

  /**
  * Size of the request from client (must be consistent).
  */
  uint32 request_size;

  /**
  * Number of client sessions that can be handled simultaneously.
  */
  uint32 client_limit;

  /**
  * Accept has been completed
  * We have at least one client connected.
  */
  bool accept_completed;

  /**
  * Whether we print out the result or not.
  */
  bool show_report;

  // state variables

  /**
  * Pointer to the socket master.
  */
  SocketMaster* sm;

  
  /**
  * The ip address of this server (interface 0).
  */
  IPADDR server_ip;

  /**
  * Number of clients currently connected.
  */
  uint32 nclients;
};

/**
 * Stores the data for the callback function.
 * In this case, the TCPServerSession object and the
 * TCPServerSessionContinuation object are stored.
 */
class TCPServerSessionCallbackActivation : public ProtocolCallbackActivation
{
  public:
    /**
    * Constructor
    */
	TCPServerSessionCallbackActivation(ProtocolSession* sess, TCPServerSessionContinuation* tcp_cnt) :
		ProtocolCallbackActivation(sess), cnt(tcp_cnt){};

    /*
    * Destructor
    */
	virtual ~TCPServerSessionCallbackActivation(){};

    /*
    * The Continuation that will maintain the app state
    */
	TCPServerSessionContinuation* cnt;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__TCP_SERVER_H__*/

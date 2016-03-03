/**
 * \file udp_server.h
 * \brief Header file for the UDPServerSession class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __UDP_SERVER_H__
#define __UDP_SERVER_H__

#include "os/base/protocol_session.h"
#include "net/ip_prefix.h"

namespace s3f {
namespace s3fnet {

class UDPServerSessionCallbackActivation;
class UDPServerSessionContinuation;
class SocketMaster;

/**
 * \brief The UDP server test protocol session.
 *
 * This is a test protocol that does not exist in reality. In a normal
 * situation, the protocol waits for an incoming request from a UDP
 * client, spawns a process to handle the client connection, and then
 * sends the requested number of bytes to the client. The difference
 * between this class and the BlockingUDPServerSession class is that
 * the latter uses blocking socket calls. In SSF, blocking is
 * implemented using non-simple processes that come with a detectable
 * higher execution cost than simple processes.
 */
class UDPServerSession: public ProtocolSession {
  friend class UDPServerSessionContinuation;

 public:
  /** The constructor. */
  UDPServerSession(ProtocolGraph* graph);

  /** The destructor. */
  virtual ~UDPServerSession();

  /** Configure the UDP server test protocol session. */
  virtual void config(s3f::dml::Configuration *cfg);

  /**
   * Return the protocol number. This specifies the relationship
   * between this protocol and the other protocols defined in the same
   * protocol stack.
   */
  virtual int getProtocolNumber() { return S3FNET_PROTOCOL_TYPE_UDPTEST_SERVER; }

  /** Initialize this protocol session. */
  virtual void init();

  /** Provide call_back functionality for implementing the start timer in the UDPServerSession
   *  this S3F process is used by waitFor() function */
  Process* start_timer_callback_proc;
  /** The callback function registered with the start_timer_callback_proc */
  void start_timer_callback(Activation ac);
  /** Storing the data for the callback function. In this case, the UDPServerSession object is stored. */
  ProtocolCallbackActivation* start_timer_ac;

 protected:
  // functions representing different stages
  /** start the UDP server */
  void start_on();
  /** function representing the handle_client stage */
  void handle_client(int socket);
  /** function representing the request_received stage */
  void request_received(UDPServerSessionContinuation* const cnt);
  /** function representing the client_connected stage */
  void client_connected(UDPServerSessionContinuation* const cnt);
  /** function representing the interval_elapsed stage */
  void interval_elapsed(UDPServerSessionContinuation* const cnt);
  /** function representing the data_sent stage */
  void data_sent(UDPServerSessionContinuation* const cnt);
  /** function representing the session_closed stage */
  void session_closed(UDPServerSessionContinuation* const cnt);

 private:
  /** This method should not be called; it is provided here to prompt
      an error message if it's called accidentally. */
  virtual int push(Activation msg, ProtocolSession* hi_sess, void* extinfo = 0, size_t extinfo_size = 0);

  /** This method should not be called; it is provided here to prompt
      an error message if it's called accidentally. */
  virtual int pop(Activation msg, ProtocolSession* lo_sess, void* extinfo = 0, size_t extinfo_size = 0);

 private:
  // configurable parameters
  uint16 server_port; ///< Port number to receive incoming request.
  uint32 request_size; ///< Size of the request from client (must be consistent).
  uint32 client_limit; ///< Number of client sessions that can be handled simultaneously.
  uint32 datagram_size; ///< Size of each udp datagram sent to client.
  ltime_t send_interval; ///< Time between successive sends.
  bool show_report; ///< Whether we print out the result or not.

  // state variables
  SocketMaster* sm; ///< Point to the socket master.
  IPADDR server_ip; ///< The ip address of this server (interface 0).
  uint32 nclients; ///< Number of clients currently connected.
};

/** storing the data for the callback function
 *  this case, the UDPServerSession object and the UDPServerSessionContinuation object are stored.
 */
class UDPServerSessionCallbackActivation : public ProtocolCallbackActivation
{
  public:
	UDPServerSessionCallbackActivation(ProtocolSession* sess, UDPServerSessionContinuation* udp_cnt) :
		ProtocolCallbackActivation(sess), cnt(udp_cnt){};
	virtual ~UDPServerSessionCallbackActivation(){};
	UDPServerSessionContinuation* cnt;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__UDP_SERVER_H__*/

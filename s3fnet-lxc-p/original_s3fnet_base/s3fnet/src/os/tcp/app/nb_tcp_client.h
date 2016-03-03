/**
 * \file nb_tcp_client.h
 * \brief Header file for the NBTCPClientSession class.
 *
 * authors : Lenny Winterrowd
 */

#ifndef __NB_TCP_CLIENT_H__
#define __NB_TCP_CLIENT_H__

#include "os/base/protocol_session.h"
#include "net/ip_prefix.h"
#include "util/shstl.h"

namespace s3f {
namespace s3fnet {

class NBTCPClientSessionCallbackActivation;
class NBTCPClientSessionContinuation;
class NBSocketMaster;

/**
 * \brief A nonblocking super-lightweight TCP client test protocol session.
 *
 * This class was designed for testing purposes only. It should be
 * expanded upon before used in any actual experimentation. It does not
 * contain a request phase like TCPClientSession so the payload size must be
 * preconfigured (via DML) with both the client and server instead of just the
 * client.
 *
 * This is a test protocol that does not exist in reality. In a normal
 * situation, the protocol waits for an incoming request from a TCP
 * client, spawns a process to handle the client connection, and then
 * sends a number of bytes to the client.
 */
#define MAXCLIENTS 10
class NBTCPClientSession: public ProtocolSession {
  friend class NBTCPClientSessionContinuation;

 public:
  /** The constructor. */
  NBTCPClientSession(ProtocolGraph* graph);

  /** The destructor. */
  virtual ~NBTCPClientSession();

  /** Configure the TCP client test protocol session. */
  virtual void config(s3f::dml::Configuration *cfg);

  /**
   * Return the protocol number.
   * The number returned specifies the relationship
   * between this protocol and the other protocols defined in the same
   * protocol stack.
   * @return Protocol number for blocking tcp client
   */
  virtual int getProtocolNumber() { return S3FNET_PROTOCOL_TYPE_TCPTEST_NON_BLOCKING_CLIENT; }

  /** Initialize this protocol session. */
  virtual void init();

  /** Process object for start timer callback */
  Process* start_timer_callback_proc;

  /** Callback function for start timer callback */
  void start_timer_callback(Activation ac);

  /** Activation object for start timer callback */
  ProtocolCallbackActivation* start_timer_ac;

 protected:

  /**
  * Get a server ip and port randomly from traffic description
  * @param server_ip the server IP address
  * @param server_port the server port
  * @return False if failed
  */
  bool get_random_server(IPADDR& server_ip, uint16& server_port);

  // functions representing different stages

  /**
  * Sets up the connection
  * @param caller the Continuation the allows non-blocking execution to occur (via callback)
  */
  void start_on(NBTCPClientSessionContinuation* const caller);

  /**
  * The main (non-blocking) loop of the client app
  * @param caller the Continuation the allows non-blocking execution to occur (via callback)
  */
  void non_blocking_loop(NBTCPClientSessionContinuation* const caller);

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
  * Port number to send outgoing request.
  */
  uint16 server_port;

  /**
  * Port number on which to receive incoming data.
  */
  uint16 client_port;

  /**
  * Size of the request from client.
  * Request size must be consistent with the non-blocking server.
  */
  uint32 file_size;

  /**
  * Whether we print out the result or not.
  */
  bool show_report;

  /**
  * Traffic server list name (default: forTCP)
  */
  S3FNET_STRING server_list;

  /**
  * Indicates whether the client is connected to the server
  */
  bool connected;

  /**
  * Number of bytes received from the server
  */
  uint32 bytes_received;

  // state variables

  /**
  * Points to the socket master.
  * Reference to the (non-blocking) Socket Master instance
  */
  NBSocketMaster* sm;

  /**
  * The ip address of the server we're connecting to
  */
  IPADDR server_ip;
};

/**
* Class to start client actions at the correct time(s)
*/
class NBTCPClientSessionCallbackActivation : public ProtocolCallbackActivation
{
  public:
    /**
    * Constructor
    */
	NBTCPClientSessionCallbackActivation(ProtocolSession* sess, NBTCPClientSessionContinuation* tcp_cnt) :
		ProtocolCallbackActivation(sess), cnt(tcp_cnt){};

    /*
    * Destructor
    */
	virtual ~NBTCPClientSessionCallbackActivation(){};

    /*
    * The Continuation that will maintain the app state
    */
	NBTCPClientSessionContinuation* cnt;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__NB_TCP_CLIENT_H__*/

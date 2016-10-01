/**
 * \file socket_master.h
 * \brief Header file for the SocketMaster class.
 *
 * authors : Dong (Kevin) Jin, Lenny Winterrowd
 */

#ifndef __SOCKET_MASTER_H__
#define __SOCKET_MASTER_H__

#include "os/socket/socket_session.h"
#include "os/base/protocol_session.h"
#include "util/shstl.h"

namespace s3f {
namespace s3fnet {

// get the socket protocol session; must be called within the protocol
// session class
#define SOCKET_API ((SocketMaster*)(inHost()->sessionForName(SOCKET_PROTOCOL_NAME)))

class SocketMaster;

/**
 * \brief Continuation used by the non-blocking socket interface.
 */
class BSocketContinuation : public SocketContinuation {
 public:
  /** The constructor. */
  BSocketContinuation(ProtocolSession* sess) : SocketContinuation(sess) {}

  /**
  * Indicates successful state completion.
  * Called if the function using the continuation completes
  * successfully. The function needs to be overridden in the derived
  * class.
  */
  virtual void success() = 0;

  /**
  * Indicates failed state completion.
  * Called if an error occurred in the function using the
  * continuation. The function needs to be overridden in the derived
  * class.
  */
  virtual void failure() = 0;

  /**
  * Proceeds to next continuation stage.
  * Used as a function pointer to proceed to the next stage of the
  * continuation. The continuation function must be a method of the
  * SocketMaster class.
  */
  void (SocketMaster::*next_stage)(int, BSocketContinuation*);
};

/**
 * \brief The socket master protocol session.
 *
 * The socket master protocol session is used to manage sockets; it
 * provides an interface between socket protocol sessions (such as tcp
 * and udp socket sessions) and the underlying protocols. This
 * implementation consists of non-blocking calls in the socket
 * interface.
 */
class SocketMaster : public ProtocolSession
{
 public:
  /** The constructor. */
  SocketMaster(ProtocolGraph* graph);

  /** The destructor. */
  virtual ~SocketMaster();
  
  /**
   * Return the protocol number.
   * Returns the protocol number specifying the relationship
   * between this protocol and the other protocols defined in the same
   * protocol stack.
   * @return protocol number for Socket protocol
   */
  virtual int getProtocolNumber() { return S3FNET_PROTOCOL_TYPE_SOCKET; }

  /** Initialize the socket master. */
  virtual void init();

  /** Control messages are passed through this function. */
  virtual int control(int ctrltyp, void* ctrlmsg, ProtocolSession* sess);

  virtual const char* psesstype() { return "BLOCKING_SOCKET"; } // DEBUG

  //
  // Socket interface: non-blocking calls.
  //
  
  /** Returns a new socket id
  * @return socket id, -1 means error
  */
  int socket();

  /**
   * Bind the socket
   * Binds the socket with the source ip address, port, and the
   * protocol that maintains the socket.
   * @param sock the socket id
   * @param source_ip the source ip address
   * @param source_port the source port
   * @param protocol the protocol to be used over the socket (ie to choose TCP or UDP)
   * @param options socket option flags
   * @return true if socket is bound successfully, false otherwise
   */
  bool bind(int sock, IPADDR source_ip, uint16 source_port, char* protocol, uint32 options = 0);
  
  /**
   * Listens for incoming connections
   * This function set up the socket so that it can queue up to
   * 'queuesize' number of foreign connection attempts. The function
   * returns true on success. This function is actually not
   * implemented; it's a placeholder.
   * @param sock the socket id
   * @param queuesize the number of connections to queue
   * @return true on success, false otherwise
   */
  bool listen(int sock, uint32 queuesize);

  /**
   * Indicates if the socket is connected.
   * A side effect of this function is that the destination ip address and port
   * number are also returned if the parameters provided are not NULL.
   * @param sock the socket id
   * @param peer_ip address of ipv4 structure to write destination ip into on success
   * @param peer_port address of port structure to write destination port into on success
   * @return true if the socket is connected, false otherwise
   */
  bool connected(int sock, IPADDR* peer_ip = NULL, uint16* peer_port=NULL);

  /**
   * Indicates end of connection.
   * Returns true if the socket has no more data and the peer has
   * closed the connection.
   * @param sock the socket id
   * @return true if socket has no more data and connection is closed, false otherwise
   */
  bool eof(int sock);

  /**
  * Reset the connections and invalidate the socket.
  * @param sock the socket id
  */
  void abort(int sock);

  //
  // Socket interface: blocking calls; these functions must be invoked
  // as a procedure call (that is, continuation must be passed as a
  // parameter).
  //

  /**
   * Connect to remote host
   * This function establishes a connection to the given destination
   * ip and port. The success method of the continuation will be
   * called on success.
   * @param sock the socket id
   * @param ip the destination ip address
   * @param port the destination port
   * @param caller the blocking socket continuation
   */
  void connect(int sock, IPADDR ip, uint16 port, BSocketContinuation* caller);

 /**
   * Checks for arrival of a connection request.
   * If successful, the success function of the
   * continuation will be called and the return value will be set as
   * the new socket number corresponding to the new connection just
   * established. The failure function of the continuation will be
   * called if an error occurred. If make_new_socket is false, the new
   * connection will use the same socket id as provided in the
   * parameter. If first_conn_notify is not NULL, the success method
   * of this continuation will be called to signal the first
   * connection. The first connection signal is an indication the
   * application process should fork a new process to handle an
   * incoming connection.
   * @param sock the socket id
   * @param make_new_socket whether to fork a new process for incoming connections
   * @param caller the blocking socket continuation
   * @param first_conn_notify contination to notify socket of first connection
   */
  void accept(int sock, bool make_new_socket, 
	      BSocketContinuation *caller, 
	      BSocketContinuation *first_conn_notify = 0);

  /**
   * Send data to remote host
   * Send a message of the given length and return the number of bytes
   * that have been sent successfully. The failure function of the
   * continuation will be called if there is an error. Otherwise, the
   * success method is called and the return value is the number of
   * bytes got transferred successfully. If the msg parameter is not
   * NULL, a real message will be sent.
   * @param sock the socket id
   * @param length the size of the message in bytes
   * @param msg the message byte buffer
   * @param caller the blocking socket continuation
   */
  void send(int sock, uint32 length, byte* msg, BSocketContinuation *caller);

  /**
   * Receive data from remote host
   * Receive a message of the given length. On success, the success
   * method of the continuation will be called and the return value
   * will be set to the number of bytes that have been received
   * successfully. The failure method of the continuation will be
   * called if there is an error. If the buffer parameter is not NULL
   * and if the arrived packet contains a real message, the message
   * will be copied to the message buffer.
   * @param sock the socket id
   * @param bufsiz the size of the buffer to read into
   * @param buffer the buffer to read into 
   * @param caller the blocking socket continuation
   */
  void recv(int sock, uint32 bufsiz, byte* buffer, BSocketContinuation *caller);

  /**
   * Terminate the connection
   * Close the connection and invalidate the socket. The success
   * method of the continuation will be called on success.
   * @param sock the socket id
   * @param caller the blocking socket continuation
   */
  void close(int sock, BSocketContinuation *caller);

 private:
  // these function are continuations of the method in the standard socket interface
  /** continuations of the method connect1() in the standard socket interface */
  void connect1(int sock, BSocketContinuation *caller);
  /** continuations of the method accept1a() in the standard socket interface */
  void accept1a(int sock, BSocketContinuation *caller);
  /** continuations of the method accept1b() in the standard socket interface */
  void accept1b(int sock, BSocketContinuation *caller);
  /** continuations of the method accept2a() in the standard socket interface */
  void accept2a(int sock, BSocketContinuation *caller);
  /** continuations of the method accept2b() in the standard socket interface */
  void accept2b(int sock, BSocketContinuation *caller);
  /** continuations of the method send1() in the standard socket interface */
  void send1(int sock, BSocketContinuation *caller);
  /** continuations of the method recv1() in the standard socket interface */
  void recv1(int sock, BSocketContinuation *caller);
  /** continuations of the method close1() in the standard socket interface */
  void close1(int sock, BSocketContinuation *caller);

  /**
   * Checks if we need to block and prepares to do so.
   * This is a "replacement" of the block_till in the original ssfnet blocking
   * socket master, which actually simulated blocking the simulated process
   * itself instead of using continuations. Here, we basically just set masks.
   * @param sockid the socket id
   * @param mask the mask to wait for
   * @param anysignal whether to wake on any signal
   * @param caller the blocking socket continuation
   */
  void block_till(int sockid, uint32 mask, bool anysignal, BSocketContinuation* caller);

 protected: 
  /** The next available socket id. */
  int new_sockid;

  typedef S3FNET_MAP(int, Socket*) SOCK_MAP;
  typedef S3FNET_SET(int) SOCK_SET;

  /** A map from a socket id to a bound socket. */
  SOCK_MAP bound_socks;

  /** A set of unbound socket ids. */
  SOCK_SET unbound_socks;

 private:
  /**
   * Push to lower protocol layer.
   * The push method is not supposed to be called for socket master
   * protocol session; it's defined here to prevent misuse.
   */
  virtual int push(Activation msg, ProtocolSession* hi_sess, void* extinfo = 0, size_t extinfo_size = 0);

  /**
   * Pop to higher protocol layer.
   * The pop method is not supposed to be called for socket master
   * protocol session; it's defined here to prevent misuse.
   */
  virtual int pop(Activation msg, ProtocolSession* lo_sess, void* extinfo = 0, size_t extinfo_size = 0);
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__SOCKET_MASTER_H__*/

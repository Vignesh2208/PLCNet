/**
 * \file nb_socket_master.h
 * \brief Header file for the NBSocketMaster class.
 *
 * authors : Lenny Winterrowd, Dong (Kevin) Jin
 */

#ifndef __NB_SOCKET_MASTER_H__
#define __NB_SOCKET_MASTER_H__

#include "os/socket/socket_session.h"
#include "os/base/protocol_session.h"
#include "util/shstl.h"

namespace s3f {
namespace s3fnet {

// get the socket protocol session; must be called within the protocol
// session class
#define SOCKET_API ((NBSocketMaster*)(inHost()->sessionForName(SOCKET_PROTOCOL_NAME)))

class NBSocketMaster;

/**
 * \brief Continuation used by the non-blocking socket interface.
 */
class NBSocketContinuation : public SocketContinuation {
 public:
    /** The constructor. */
  NBSocketContinuation(ProtocolSession* sess) : SocketContinuation(sess) {}

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
  * Triggers a callback into the calling program.
  * Called when socket state changes and needs to wake the calling
  * program. The function must be overridden in the derived class
  * and should be based on the current program's accessible state.
  */
  virtual void resume() = 0;

  /**
  * Proceeds to next continuation stage.
  * Used as a function pointer to proceed to the next stage of the
  * continuation. The continuation function must be a method of the
  * NBSocketMaster class. GENERALLY NOT USED FOR NON-BLOCKING
  * SOCKET CONTINUATIONS (required for Continuation base class).
  */
  int (NBSocketMaster::*next_stage)(int, NBSocketContinuation*);

  /**
  * Callback into application using socket
  * Used to resume execution of the non-blocking socket's callflow
  * (ie into a server application's flow, for instance).
  */
  void (ProtocolSession::*nb_callback)(void*, NBSocketContinuation*);
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
class NBSocketMaster : public ProtocolSession
{
 public:

  /**
  * Enum for non-blocking socket states
  * Usually used as return values for socket functions.
  */
  enum {
    ESUCCESS        = 0, /**< Indicates success */
    EGENERIC        = -1, /**< Indicates generic error */
    EWOULDBLOCK     = -2, /**< Function returned because it would otherwise block. */
    EINPROGRESS     = -3, /**< Connection is currently in progress. */
    ECONNECTED      = -4 /**< Socket is already connected. */
  };

  /** The constructor. */
  NBSocketMaster(ProtocolGraph* graph);

  /** The destructor. */
  virtual ~NBSocketMaster();
  
  /**
   * Return the protocol number.
   * Returns the protocol number specifying the relationship
   * between this protocol and the other protocols defined in the same
   * protocol stack.
   * @return protocol number for Socket protocol
   */
  virtual int getProtocolNumber() { return S3FNET_PROTOCOL_TYPE_SOCKET; }

  /** Initialize the non-blocking socket master. */
  virtual void init();

  /** Control messages are passed through this function. */
  virtual int control(int ctrltyp, void* ctrlmsg, ProtocolSession* sess);


  /**
  * Indicates the protocol session type as a string
  * Introduced for debugging purposes only. Differentiates between
  * blocking and nonblocking sockets easily.
  * @return session type as string
  */
  virtual const char* psesstype() { return "NONBLOCKING_SOCKET"; } // DEBUG

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
  // Socket interface: (non)blocking calls; these functions must be invoked
  // as a procedure call (that is, continuation must be passed as a
  // parameter). They do not actually block from a simulation perspective
  // (ie they return and the application continues execution).
  //

  /**
   * Connect to remote host
   * This function establishes a connection to the given destination
   * ip and port. The success method of the continuation will be
   * called on success.
   * @param sock the socket id
   * @param ip the destination ip address
   * @param port the destination port
   * @param caller the non-blocking socket continuation to allow callback/wakeup
   * @return status of connect call, returns EWOULDBLOCK instead of blocking
   */
  int connect(int sock, IPADDR ip, uint16 port, NBSocketContinuation* caller);

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
   * @param caller the non-blocking socket continuation to allow callback/wakeup
   * @param first_conn_notify contination to notify socket of first connection
   * @return status of accept call, returns EWOULDBLOCK instead of blocking
   */
  int accept(int sock, bool make_new_socket, 
          NBSocketContinuation *caller, 
          NBSocketContinuation *first_conn_notify = 0);

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
   * @param caller the non-blocking socket continuation to allow callback/wakeup
   * @return status of send call, returns EWOULDBLOCK instead of blocking
   */
  int send(int sock, uint32 length, byte* msg, NBSocketContinuation *caller);

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
   * @param caller the non-blocking socket continuation to allow callback/wakeup
   * @return status of recv call, returns EWOULDBLOCK instead of blocking
   */
  int recv(int sock, uint32 bufsiz, byte* buffer, NBSocketContinuation *caller);

  /**
   * Terminate the connection
   * Close the connection and invalidate the socket. The success
   * method of the continuation will be called on success.
   * @param sock the socket id
   * @param caller the non-blocking socket continuation to allow callback/wakeup
   * @return status of close call, returns EWOULDBLOCK instead of blocking
   */
  int close(int sock, NBSocketContinuation *caller);

 private:
  // these function are continuations of the method in the standard
  // socket interface
  /**
  * Second stage of connect
  * Called by connect(). Is reentrant for the non-blocking socket architecture.
  * Return values are identical to connect().
  * @param sock the socket id
  * @param caller the non-blocking socket continuation to allow callback/wakeup
  * @return status of connect call, returns EWOULDBLOCK instead of blocking
  * @see connect()
  */
  int connect1(int sock, NBSocketContinuation *caller);

  /**
  * Second stage of send
  * Called by send(). Is reentrant for the non-blocking socket architecture.
  * Return values are identical to send().
  * @param sock the socket id
  * @param caller the non-blocking socket continuation to allow callback/wakeup
  * @return status of send call, returns EWOULDBLOCK instead of blocking
  * @see send()
  */
  int send1(int sock, NBSocketContinuation *caller);

  /*
  * Second stage of recv
  * Called by recv(). Is reentrant for the non-blocking socket architecture.
  * Return values are identical to recv().
  * @param sock the socket id
  * @param caller the non-blocking socket continuation to allow callback/wakeup
  * @return status of recv call, returns EWOULDBLOCK instead of blocking
  * @see recv()
  */
  int recv1(int sock, NBSocketContinuation *caller);

  /*
  * Second stage of close
  * Called by close(). Is reentrant for the non-blocking socket architecture.
  * Return values are identical to close().
  * @param sock the socket id
  * @param caller the non-blocking socket continuation to allow callback/wakeup
  * @return status of close call, returns EWOULDBLOCK instead of blocking
  * @see close()
  */
  int close1(int sock, NBSocketContinuation *caller);

  /**
   * Checks if blocking would occur.
   * This is a "replacement" of the block_till in the blocking socket
   * master. Basically, we just set the masks.
   * @param sockid the socket id
   * @param mask the mask to wait for
   * @param anysignal whether to wake on any signal
   * @param caller the non-blocking socket continuation to allow callback/wakeup
   * @return the status, generally EWOULDBLOCK is propagated from this call
   */
  int block_till(int sockid, uint32 mask, bool anysignal, NBSocketContinuation* caller);

 protected: 
  /** The next available socket id. */
  int new_sockid;

  typedef S3FNET_MAP(int, Socket*) NBSOCK_MAP;
  typedef S3FNET_SET(int) NBSOCK_SET;

  /** A map from a socket id to a bound socket. */
  NBSOCK_MAP bound_socks;

  /** A set of unbound socket ids. */
  NBSOCK_SET unbound_socks;

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

#endif /*__NB_SOCKET_MASTER_H__*/

/**
 * \file nb_tcp_server.h
 * \brief Header file for the NBTCPServerSession class.
 *
 * authors : Lenny Winterrowd
 */

#ifndef __NB_TCP_SERVER_H__
#define __NB_TCP_SERVER_H__

#include "os/base/protocol_session.h"
#include "net/ip_prefix.h"

namespace s3f {
namespace s3fnet {

class NBTCPServerSessionCallbackActivation;
class NBTCPServerSessionContinuation;
class NBSocketMaster;

#define BUFSIZE 1024

/**
* Class to send a file
* Sends a file to multiple potential receivers in a non-blocking fashion,
* iterating over each open connection and sending some bytes on each pass
* (usually triggered by callback from the continuation). 
*/
class FileSender {
  friend class NBTCPServerSessionContinuation;

  public:

    /**
    * Contstructor
    */
    FileSender() { mState = IDLE; }     /* not doing anything initially */

    /**
     * Start sending a file on the given socket.
     * Set the socket to be nonblocking.
     * @param fsize the name of the file to send.
     * @param socket an open network connection.
     */
    void sendFile(int fsize,int socket);

    /**
     * Continue sending the file started by sendFile().
     * Call this periodically.
     * @param sm the (non-blocking) socket master reference
     * @param cnt the Continuation the allows non-blocking execution to occur (via callback)
     * @return zero if not idle and no error
     */
    int handleIO(NBSocketMaster* sm, NBTCPServerSessionContinuation* cnt);

  protected:
    /**
    * Socket of the current file being sent
    */
    int mSocket;          
  
    /**
    * Number of bytes being sent
    */
    int mFileSize;

    /**
    * Number of bytes already sent
    */
    int mBytesSent;

    /**
    * Current chunk of file 
    */
    //char mBuf[BUFSIZE];     

    /**
    * Bytes in buffer
    */
    int mBufLen;

    /**
    * Bytes used so far; <= m_buf_len
    */
    int mBufUsed;

    /*
    * Enum reprsenting what we're doing
    */
    enum {
           IDLE, /**< FileSender is idle */
           SENDING /**< FileSender is sending */
          } mState;
};

/**
 * \brief A nonblocking TCP server test protocol session.
 *
 * This is a test protocol that does not exist in reality. In a normal
 * situation, the protocol waits for an incoming request from a TCP
 * client, spawns a process to handle the client connection, and then
 * sends a number of bytes to the client.
 *
 * In this protocol, the 'request' is just a connection. On connect, the server
 * sends the client a pre-configured (via DML) payload. This is minimalistic
 * but sufficient for testing purposes.
 */
#define MAXCLIENTS 10
class NBTCPServerSession: public ProtocolSession {
  friend class NBTCPServerSessionContinuation;

 public:
  /** The constructor. */
  NBTCPServerSession(ProtocolGraph* graph);

  /** The destructor. */
  virtual ~NBTCPServerSession();

  /** Configure the TCP server test protocol session. */
  virtual void config(s3f::dml::Configuration *cfg);

  /**
   * Return the protocol number.
   * The number returned specifies the relationship
   * between this protocol and the other protocols defined in the same
   * protocol stack.
   * @return Protocol number for blocking tcp client
   */
  virtual int getProtocolNumber() { return S3FNET_PROTOCOL_TYPE_TCPTEST_NON_BLOCKING_SERVER; }

  /** Initialize this protocol session. */
  virtual void init();

  /** Process object for start timer callback */
  Process* start_timer_callback_proc;

  /** Callback function for start timer callback */
  void start_timer_callback(Activation ac);

  /** Activation object for start timer callback */
  ProtocolCallbackActivation* start_timer_ac;

 protected:
  // functions representing different stages

  /**
  * Sets up the listener for connections
  * @param caller the Continuation the allows non-blocking execution to occur (via callback)
  */
  void start_on(NBTCPServerSessionContinuation* const caller);

  /**
  * The main (non-blocking) loop of the client app
  * @param caller the Continuation the allows non-blocking execution to occur (via callback)
  */
  void non_blocking_loop(NBTCPServerSessionContinuation* const caller);

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
  NBSocketMaster* sm;

  /**
  * The ip address of this server (interface 0).
  */
  IPADDR server_ip;

  /**
  * Number of clients currently connected.
  */
  uint32 nclients;

  /**
  * Array of FileSenders to handle clients
  */
  FileSender mFilesenders[MAXCLIENTS];
};


/**
* Class to start server actions at the correct time(s)
*/
class NBTCPServerSessionCallbackActivation : public ProtocolCallbackActivation
{
  public:

    /**
    * Constructor
    */
	NBTCPServerSessionCallbackActivation(ProtocolSession* sess, NBTCPServerSessionContinuation* tcp_cnt) :
		ProtocolCallbackActivation(sess), cnt(tcp_cnt){};

    /*
    * Destructor
    */
	virtual ~NBTCPServerSessionCallbackActivation(){};

    /*
    * The Continuation that will maintain the app state
    */
	NBTCPServerSessionContinuation* cnt;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__NB_TCP_SERVER_H__*/

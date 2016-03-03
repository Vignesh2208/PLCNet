/**
 * \file socket_session.h
 * \brief Header file for SocketSession and related classes.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __SOCKET_SESSION_H__
#define __SOCKET_SESSION_H__

#include "net/ip_prefix.h"
#include "os/base/protocol_session.h"
#include "s3fnet.h"

namespace s3f {
namespace s3fnet {

class SessionMaster;

/** Continuation used by the socket interface. */
class SocketContinuation {
 public:
  /** The constructor. */
  SocketContinuation(ProtocolSession* sess) : 
    retval(0), owner(sess), chain_continuation(0) {}

  /** The destructor. */
  virtual ~SocketContinuation() {
    if(chain_continuation) delete chain_continuation;
	myscval = -1;
  }

  /** Return the owner protocol session. */
  ProtocolSession* getOwnerSession() { return owner; }

  /*
  * Returns type of SocketContinuation as string
  * Used primarily for debugging. Can easily identify Blocking vs
  * Non-blocking socket continuations.
  * @return type of SocketContinuation as string
  */
  virtual const char* getType() { return "N/A"; }

  /*
  * Special value for debugging
  * Value that may be set by any deriving class or instance as a unique
  * identifier of continuations. Extremely useful for debugging continuation
  * behavior.
  */
  int myscval;

  /**
  * Function to set the SocketContinuation (SC) value
  * @param v SC value to set
  */
  virtual void setSCVal(int v) { myscval = v; }

  /**
  * Function to get the SocketContinuation (SC) value
  * @return the SC value
  */
  virtual int getSCVal() { return myscval; }

 public:
  /** The return value of the function using the continuation.
  * The return value makes sense only when the function has completed
  * successfully. Note that a function using continuation could be
  * of a bool type, in which case we still use an integer return
  * value.
  */
  int retval;

  /** The owner protocol session. */
  ProtocolSession* owner;

  /** Other continuation structures can be chained here. */
  SocketContinuation* chain_continuation;
};

/**
 * \brief An application session.
 *
 * Each application session can be identified by a tuple: (protocol
 * id, src ip, src port, dest ip, dest port).
 */
class SessionID {
 public:
  /** The constructor. */
  SessionID(uint32 proto=0, IPADDR srcip=0,  uint16 srcport=0, IPADDR destip=0, uint16 destport=0) :
    prot_id(proto), src_ip(srcip), dst_ip(destip), src_port(srcport), dst_port(destport) {}

  /** Override the assignment operator. */
  SessionID& operator = (const SessionID& sid) {
    prot_id  = sid.prot_id;
    src_ip   = sid.src_ip;
    dst_ip   = sid.dst_ip;
    src_port = sid.src_port;
    dst_port = sid.dst_port;
    return *this;
  }

  /** Override == operator. */
  friend bool operator == (const SessionID& s1, const SessionID& s2) {
    return(s1.prot_id  == s2.prot_id  &&
	   s1.src_ip   == s2.src_ip   &&
	   s1.dst_ip   == s2.dst_ip   &&
	   s1.src_port == s2.src_port &&
	   s1.dst_port == s2.dst_port);
  }

  /** Override != operator. */
  friend bool operator != (const SessionID& s1, const SessionID& s2) {
    return !(s1 == s2);
  }

  /**
  * Translate the ID into a textual string.
  * @param buf buffer to write into (MUST NOT BE NULL)
  * @return pointer to the same buffer passe din
  */
  char* toString(char* buf) {
    assert(buf);
    char buf0[50], buf1[50];
    sprintf(buf, "prot %u src_ip %s src_port %u dst_ip %s dst_port %u",
	    prot_id, IPPrefix::ip2txt(src_ip, buf0), src_port, 
	    IPPrefix::ip2txt(dst_ip, buf1), dst_port);
    return buf;
  }

 public:
  /** The protocol number. */
  uint32 prot_id;

  /** Source ip address. */
  IPADDR src_ip;

  /** Destination ip address. */
  IPADDR dst_ip;

  /** Source port number. */
  uint16 src_port;

  /** Destination port number. */
  uint16 dst_port;
};

/**
 * \internal
 * \brief A structure used to compare two session ids.
 * Returns true is sess_1 is less than sess_2.
 */
struct SessionCmp {
  /** Comparator */
  bool operator()(const SessionID& sess_1, const SessionID& sess_2) {
    return ((sess_1.prot_id < sess_2.prot_id) ||	    
	    (sess_1.prot_id == sess_2.prot_id && sess_1.src_ip < sess_2.src_ip) ||	    
	    (sess_1.prot_id == sess_2.prot_id  && sess_1.src_ip == sess_2.src_ip && 
	     sess_1.src_port < sess_2.src_port) ||	    
	    (sess_1.prot_id == sess_2.prot_id && sess_1.src_ip == sess_2.src_ip && 
	     sess_1.src_port == sess_2.src_port && sess_1.dst_ip < sess_2.dst_ip) ||	    
	    (sess_1.prot_id == sess_2.prot_id && sess_1.src_ip == sess_2.src_ip && 
	     sess_1.src_port == sess_2.src_port && sess_1.dst_ip == sess_2.dst_ip && 
	     sess_1.dst_port < sess_2.dst_port));
  }
};

/**
 * \brief The socket session.
 *
 * A socket session is an application session used for communication
 * between an application and the underlying protocols (especially,
 * the socket master protocol session) through a socket.
 */
class SocketSession : public SessionID {
 public:
  /** The constructor. */
  SocketSession(uint32 proto=0, IPADDR srcip=0,  uint16 srcport=0, IPADDR destip=0, uint16 destport=0) :
    SessionID(proto, srcip, srcport, destip, destport) {}

  /** The destructor. */
  virtual ~SocketSession() {}
  
  /**
  * Connect to remote host
  * Return true if connected successfuly to the destination with a
  * given ip address and port number. Must be implemented by deriving
  * class (pure virtual).
  * @param destip the destination ip address
  * @param destport the desination port
  * @return bool true on success, else false
  */
  virtual bool connect(IPADDR destip, uint16 destport) = 0;
  
  /**
  * Disconnect the session.
  * Must be implemented by deriving class (pure virtual).
  */
  virtual void disconnect() = 0;

  /**
   * Send data to remote host
   * Send the data down the protocol stack. If it is fake data, msg is
   * NULL. Return how many bytes have been sent out. -1 means error.
   * Must be implemented by deriving class (pure virtual).
   * @param length size in bytes of message to send
   * @param msg actual byte buffer to send
   * @return number of bytes sent, -1 means error
   */
  virtual int send(int length, byte* msg) = 0;

  /**
   * Receive data from remote host
   * Receive the arrived data. If the msg is not NULL, the data is
   * copied into the message buffer up to the number of bytes
   * requested.  Return how many bytes that have really been
   * received. -1 means error. Must be implemented by deriving class
   * (pure virtual).
   * @param length size in bytes of receive buffer provided
   * @param msg receive buffer to write received data into
   * @return number of bytes read, -1 means error
   */
  virtual int recv(int length, byte* msg) = 0;

  /**
  * Passive open.
  * Must be implemented by deriving class (pure virtual).
  * @return true if connected successfully, false otherwise
  */
  virtual bool listen() = 0;

  /**
  * Returns true if connected.
  * Must be implemented by deriving class (pure virtual).
  * @return true if connected, false otherwise
  */
  virtual bool connected() = 0;

  /**
  * Set the source ip and port for a connection.
  * Must be implemented by deriving class (pure virtual).
  * @param ip source ip address
  * @param port source port
  * @return true if successful, false otherwise
  */
  virtual bool setSource(IPADDR ip, uint16 port) = 0;

  /**
  * Abort the session.
  * Must be implemented by deriving class (pure virtual).
  */
  virtual void abort() = 0;

  /**
  * Set the socket id.
  * Must be implemented by deriving class (pure virtual).
  * @param sock the socket id
  */
  virtual void setSocketID(int sock) = 0;

  /**
  * Return the master protocol session.
  * Must be implemented by deriving class (pure virtual).
  * @return the session's master (ie BlockingSocketMaster)
  */
  virtual SessionMaster* getSessionMaster() = 0;

  /** Called by socket to reinitialize the destination ip and port. */
  virtual void reinit() {
    dst_ip = IPADDR_ANYDEST;
    dst_port = 0;
  }

  /**
  * Necessary steps are taken to reclaim this socket session.
  * Must be implemented by deriving class (pure virtual).
  */
  virtual void release() = 0;
};

/**
 * \brief Signals to and from a socket.
 *
 * The socket signal is used to communicate between the socket master
 * and the supporting (tcp/udp) protocol session.  
 */
class SocketSignal {
 public:
  /**
  * Signals to socket
  * Used to unblock socket (added to state)
  */
  enum {
    NO_SIGNAL        = 0x00000000, /**< No signal */
    CONN_RESET       = 0x00000001, /**< connection reset */
    DATA_AVAILABLE   = 0x00000002, /**< data is available */
    OK_TO_SEND       = 0x00000004, /**< clear to send data */
    CONN_CLOSED      = 0x00000008, /**< connection is closed */
    SOCK_EOF         = 0x00000010, /**< no more data */
    OK_TO_CLOSE      = 0x00000020, /**< connection may be closed */
    FIRST_CONNECTION = 0x00000040, /**< is the first connection */
    SESSION_RELEASED = 0x00000080, /**< session has already been released */
  };

  /** The id of the socket. */
  int sockid;

  /** The signal itself. */
  uint32 signal;

  /** Number of bytes transfered. */
  uint32 nbytes;

 public:

  /** The constructor. */
  SocketSignal(int sock, uint32 sig) : 
    sockid(sock), signal(sig), nbytes(0) {}
};

/**
 * \brief The socket information.
 *
 * A socket is an application representation of a connection to a
 * remote peer. It contains necessary information on the state of the
 * connection.
 */
class Socket {
 public:
  /**
  * State of socket
  * Uses the same mask as signals
  */
  enum {
    NO_SIGNAL        = 0x00000000, /**< No signal */
    CONN_RESET       = 0x00000001, /**< connection reset */
    DATA_AVAILABLE   = 0x00000002, /**< data is available */
    OK_TO_SEND       = 0x00000004, /**< clear to send data */
    CONN_CLOSED      = 0x00000008, /**< connection is closed */
    SOCK_EOF         = 0x00000010, /**< no more data */
    OK_TO_CLOSE      = 0x00000020, /**< connection may be closed */
    FIRST_CONNECTION = 0x00000040, /**< is the first connection */
    SESSION_RELEASED = 0x00000080, /**< session has already been released */
    CONNECT_ACTIVE   = 0x00000100, /**< connect is in progress */
    ALL_SIGNALS      = 0x00ffffff, /**< logical AND of all signals */
    SOCK_DELETED     = 0x10000000 /**< socket was already deleted */
  };

  /**
  * Socket's resume state for non-blocking execution
  * Used for proper reentrant behavior.
  */
  enum {
    RESUME_NONE    = 0x00, /**< No resume state */
    RESUME_ACCEPT1 = 0x01, /**< Socket has passed first stage of accept */
    RESUME_ACCEPT2 = 0x02, /**< Socket has passed second stage of accept */
    RESUME_MAKENEW = 0x04, /**< Socket should fork a new thread for incoming connections */
    RESUME_SEND1   = 0x08, /**< Socket has passed first stage of send */
    RESUME_CONNECT1 = 0x10, /**< Socket has passed first stage of connect */
    RESUME_RECV1    = 0x20, /**< Socket has passed first stage of recv */
    RESUME_CLOSE1   = 0x40 /**< Socket has passed first stage of close */
  };

  /** The constructor. */
  Socket() :
    session(0), state(NO_SIGNAL), mask(0), continuation(0),
    bytes_completed(0), active_counter(0), resume(RESUME_NONE) {}

  /** Release/free the socket session */
  void release() { if(session) session->release(); }

  /** Reinitialize the socket after disconnection. */
  void reinit() { state &= ~ALL_SIGNALS; }

  /**
  * Checks if socket is active
  * @return true if socket is active somewhere, false otherwise
  */
  bool is_active() { return active_counter > 0; }

  /**
  * Activates socket
  * Will cause is_active() to return true.
  * @see is_active()
  * @see deactivate()
  */
  void activate() { active_counter++; }

  /**
  * Deactivates socket
  * Reverses one activate() call
  * @see activate()
  */
  void deactivate() { active_counter--; }

  /**
  * Sets socket state to deleted
  */
  void die_slowly() { state |= SOCK_DELETED; }

  /**
  * Checks if socket has been deleted
  * @return true if already deleted, false otherwise
  * @see die_slowly()
  */
  bool has_died() { return state & SOCK_DELETED; }

 public:
  /** Pointer to the owner socket session. */
  SocketSession* session;

  /** The state of the socket. */
  uint32 state;

  /** The mask of the socket. */
  uint32 mask;

  /** The semaphore is used only by the blocking socket master to
      synchronize processes. */
  //s3f::ssf::ssf_semaphore semaphore;

  /** Used by the nonblocking socket master to unblock the socket
      interface functions. */
  SocketContinuation* continuation;

  /** The number of bytes that the socket has completed sending or
      receiving. */
  uint32 bytes_completed;

  /**
  * Gets the socket's resume state
  * Used by non-blocking sockets for proper reentrant behavior.
  * @return socket's resume state
  */
  inline int getResume() { return resume; }

  /**
  * Sets the socket's resume state
  * Used by non-blocking sockets for proper reentrant behavior.
  * @param r new resume state
  */
  inline void setResume(int r) { resume = r; }

 protected:
  /** State of the socket for proper resume in multi-stage functions
     like accept(). */
  int resume;

 private:
  /** The destructor as a private method to prevent accidental
      deletion. The user should call the release method instead. */
  ~Socket() {}

  /** Indicates how many processes are actively waiting on this
      socket, in which case the socket must not be removed. If the
      active_counter is negative, it means the socket is put on the
      death role. */
  int active_counter;
};

/** 
 * \brief Base class for protocol session masters.
 *
 * Specific protocols, such as TCP and UDP, are derived from this
 * class. In essence, a session master is a protocol session that is
 * in close contact with the socket master and can create multiple
 * socket sessions.
 */
class SessionMaster : public ProtocolSession {
 public:
  /** Return the socket master, the protocol session directly above
      the protocol session master. */
  ProtocolSession* getSocketMaster() { return sock_master; }

  /** Create a socket session of a given socket id. */
  virtual SocketSession* createSession(int sock) = 0;

 protected:
  /** The constructor. */
  SessionMaster(ProtocolGraph* graph) : ProtocolSession(graph), sock_master(0) {}

 protected:
  /** The socket master above the protocol session master. */
  ProtocolSession* sock_master;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__SOCKET_SESSION_H__*/

/**
 * \file udp_session.h
 * \brief Header file for the UDPSession class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __UDP_SESSION_H__
#define __UDP_SESSION_H__

#include "os/udp/udp_master.h"
#include "os/socket/socket_session.h"
#include "s3fnet.h"

namespace s3f {
namespace s3fnet {

class DataMessage;
class UDPMessage;

/**
 * \brief The UDP session.
 */
class UDPSession : public SocketSession {
 public:
  /** The constructor. */
  UDPSession(UDPMaster* master, int sock);

  /** The destructor. */
  virtual ~UDPSession();

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
  virtual SessionMaster* getSessionMaster() { return udp_master; }

  /** Release this session if unnecessary. */
  virtual void release();
  
  /* Functions to be called by the udp master. */

  /** Receive the message popped up from the ip session. */
  void receive(UDPMessage* udphdr);

 private:
  /** Wake up the application on a signal. */
  void wake_app(int signal);

  /** Clear a specific signal bit from application. */
  void clear_app_state(int signal);

  /** 
   * Copy the received messages into the user receive buffer of the
   * given size. Return the number of bytes successfully transferred.
   */
  int generate(int length, byte* msg);

 private:
  /** Point to the udp master. */
  UDPMaster* udp_master;

  /** The socket id. */
  int socket;

  /** Whether the UDP session has been connected. */
  bool is_connected;
  
  /** The receive buffer to store the received messages. */
  S3FNET_DEQUE(DataMessage*) rcvbuf;
  
  /** The total number of bytes stored in the receive buffer. */
  int rcvbuf_len;

  /** Offset to the first message in the receive buffer, which points
      to the byte to be transferred next to the application. */
  int rcvbuf_offset;

  /** The buffer (owned by application) to receive data. */
  byte* appl_rcvbuf;

  /** Total number of bytes expected to receive. */
  uint32 appl_rcvbuf_size;

  /** The size of the received data (that has been put into the
      receive buffer). */
  uint32 appl_data_rcvd;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__UDP_SESSION_H__*/

/**
 * \file udp_master.h
 * \brief Header file for the UDPMaster class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __UDP_MASTER_H__
#define __UDP_MASTER_H__

#include "os/socket/socket_session.h"
#include "util/shstl.h"

namespace s3f {
namespace s3fnet {

class UDPSession;
 
/**
 * \brief The UDP master.
 *
 * The UDP master is a protocol session that manages all UDP sessions.
 */
class UDPMaster : public SessionMaster {
  friend class UDPSession;

 public:
  /** The constructor. */
  UDPMaster(ProtocolGraph* graph);

  /** The destructor. */
  virtual ~UDPMaster();

  /** Configure the UDP master protocol session. */
  virtual void config(s3f::dml::Configuration *cfg);

  /**
   * Return the protocol number. This specifies the relationship
   * between this protocol and the other protocols defined in the same
   * protocol stack.
   */
  virtual int getProtocolNumber() { return S3FNET_PROTOCOL_TYPE_UDP; }

  /** Initialize this protocol session. */
  virtual void init();

  /** Control messages are passed through this function. */
  virtual int control(int ctrltyp, void* ctrlmsg, ProtocolSession* sess);

 private:
  /**
   * The push method is called by the UDP sessions to send data down
   * the protocol stack.
   */
  virtual int push(Activation msg, ProtocolSession* hi_sess, void* extinfo = 0, size_t extinfo_size = 0);

  /**
   * Receive data up the protocol stack to the upper layer. The data
   * will be demultiplexed to the corresponding UDP session.
   */
  virtual int pop(Activation msg, ProtocolSession* lo_sess, void* extinfo = 0, size_t extinfo_size = 0);

 public:
  /** Create a new UDP session of the given socket id. */
  virtual SocketSession* createSession(int sock);
  
  /** Reclaim a UDP session. */
  void deleteSession(UDPSession* session);

 private:
  /** The IP protocol session is right below this protocol session. */
  ProtocolSession* ip_sess;

  /** The maximum UDP datagram size. */
  int max_datagram_size;

  /** A set of UDP sessions maintained by the master. */
  S3FNET_SET(UDPSession*) udp_sessions;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__UDP_MASTER_H__*/

/**
 * \file dummy_session.h
 * \brief Header file for the DummySession class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __DUMMY_SESSION_H__
#define __DUMMY_SESSION_H__

#include "os/base/protocol_session.h"
#include "util/shstl.h"
#include "net/ip_prefix.h"

namespace s3f {
namespace s3fnet {

class IPSession;

#define DUMMY_PROTOCOL_CLASSNAME "S3F.OS.DUMMY"

/**
 * \brief A dummy protocol session, sending hello messages to specified peers.
 */
class DummySession : public ProtocolSession {

 public:
  /** the constructor
   *  must be take an argument of the protocol graph
   */
  DummySession(ProtocolGraph* graph);

  /** the destructor */
  virtual ~DummySession();

  /** each protocol must have a unique protocol number */
  virtual int getProtocolNumber() { return S3FNET_PROTOCOL_TYPE_DUMMY; }

  /** called to configure this protocol session */
  virtual void config(s3f::dml::Configuration* cfg);

  /** called after config() to initialize this protocol session */
  virtual void init();

  /** called by other protocol sessions to send special control messages */
  virtual int control(int ctrltyp, void* ctrlmsg, ProtocolSession* sess);

  /** called by the protocol session above to push a protocol message down the protocol stack */
  virtual int push(Activation msg, ProtocolSession* hi_sess, void* extinfo = 0, size_t extinfo_size = 0);

  /** called by the protocol session below to pop a protocol message up the protocol stack */
  virtual int pop(Activation msg, ProtocolSession* lo_sess, void* extinfo = 0, size_t extinfo_size = 0);

  /** the S3F process used by waitFor() function in the dummy protocol, provide call_back functionality */
  Process* callback_proc;

  /** the callback function registered with the callback_proc */
  void callback(Activation ac);

  /** the actual body of the callback function */
  void callback_body(Activation ac);

  /** storing the data for the callback function
   *  this case, the dummy session object is stored.
   */
  ProtocolCallbackActivation* dcac;
  
  // the following stores the configuration from read from DML
  /** hello message for the dummy protocol */
  S3FNET_STRING hello_message;
  /** NHI of the peer node which hello messages will be sent */
  S3FNET_STRING hello_peer_nhi;
  /** IP of the peer node which hello messages will be sent */
  S3FNET_STRING hello_peer_ip;
  /** duration between which hello messages will be sent */
  ltime_t hello_interval;
  /** jitter added into the hello_interval */
  ltime_t jitter;

  // state information of this protocol session
  /** the IP of the peer we send periodic hello messages */
  IPADDR hello_peer;
  /** the IP layer below this protocol */
  IPSession* ip_session;

  //for stats collection
  /** packet sequence number (increasing) */
  int pkt_seq_num;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__DUMMY_SESSION_H__*/

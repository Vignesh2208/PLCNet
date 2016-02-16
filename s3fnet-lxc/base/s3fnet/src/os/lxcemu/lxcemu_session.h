/**
 * \file lxcemu_session.h
 * \brief Header file for the LxcemuSession class. Adapted from dummy_session.h
 *
 * authors : Vladimir Adam
 */

#ifndef __LXCEMU_SESSION_H__
#define __LXCEMU_SESSION_H__

#include "os/base/protocol_session.h"
#include "util/shstl.h"
#include "net/ip_prefix.h"

namespace s3f {
namespace s3fnet {

class IPSession;

#define LXCEMU_PROTOCOL_CLASSNAME "S3F.OS.LXCEMU"

/**
 * \brief A lxcemu protocol session, sending hello messages to specified peers.
 */
class LxcemuSession : public ProtocolSession {

 public:
  /** the constructor
   *  must be take an argument of the protocol graph
   */
  LxcemuSession(ProtocolGraph* graph);

  /** the destructor */
  virtual ~LxcemuSession();

  /** each protocol must have a unique protocol number */
  virtual int getProtocolNumber() { return S3FNET_PROTOCOL_TYPE_LXCEMU; }

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

  /** the S3F process used by waitFor() function in the lxcemu protocol, provide call_back functionality */
  Process* callback_proc;

  /** the callback function registered with the callback_proc */
  void callback(Activation ac);

  /** the actual body of the callback function */
  void callback_body(EmuPacket* packet, IPADDR srcIP, IPADDR destIP);

  void injectEvent(EmuPacket* packet, IPADDR srcIP, IPADDR destIP);

  /** storing the data for the callback function
   *  this case, the lxcemu session object is stored.
   */
  ProtocolCallbackActivation* dcac;
  
  /** the IP layer below this protocol */
  IPSession* ip_session;

  //for stats collection
  /** packet sequence number (increasing) */
  int pkt_seq_num;
};


/**
 *  Storing the data for the callback function.
 *  In this case, the LXCEventSession object, destination IP address and the emulation packet are stored.
 */
class LXCEventSessionCallbackActivation : public ProtocolCallbackActivation
{
  public:
	LXCEventSessionCallbackActivation(ProtocolSession* sess, IPADDR _dst_ip, IPADDR _src_ip, EmuPacket* ppkt);
	virtual ~LXCEventSessionCallbackActivation(){}

	IPADDR dst_ip; ///< the destination IP address
	IPADDR src_ip; ///< the source IP address
	EmuPacket* packet; ///< pointer to the emulation packet
};




}; // namespace s3fnet
}; // namespace s3f

#endif /*__LXCEMU_SESSION_H__*/

/**
 * \file protocols.h
 * \brief Header file for the Protocols class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __PROTOCOLS_H__
#define __PROTOCOLS_H__

#include "util/shstl.h"

namespace s3f {
namespace s3fnet {

class ProtocolSession;
class ProtocolGraph;

/**
 * \brief Managing protocols.
 *
 * The Protocols class is used to manage instantiation of protocol
 * sessions of given class names. C++ lacks support for
 * self-reflection and this class provides a mechanism for creating
 * protocol sessions from their corresponding class string names read
 * from the DML description. In particular, the class defines a macro
 * called S3FNET_REGISTER_PROTOCOL. It has two parameters: the name of
 * the protocol session class and the matching string to identify the
 * protocol session class in DML. For example, if you create a MAC
 * layer protocol "MyMACSession" and if you want to identify your
 * protocol in DML with "mac.my-mac-session", you should put
 * S3FNET_REGISTER_PROTOCOL(MyMACSession, "mac.my-mac-session") inside
 * the source file where you define the protocol.
 */
class Protocols {
 public:
  /**
   * The method is used by ProtocolGraph's config method to create
   * protocol session instances from the DML description.
   */
  static ProtocolSession* newInstance(char* classname, ProtocolGraph* graph);

 public:
  /**@name Protocol instantiation
   *
   * The following field and method should be protected. We made
   * them public in order for them to work with S3FNET_REGISTER_PROTOCOL
   * macro.
   */
  //@{

  /** A list of protocols already registered. */
  static S3FNET_POINTER_VECTOR* registered_protocols;

  /** Called by the S3FNET_REGISTER_PROTOCOL macro to register a protocol
      session. */
  static int registerProtocol(ProtocolSession* (*fct)(char*, ProtocolGraph*));

  //@}
};

/** Each protocol defined in the system must declare itself with
   this macro. */
#define S3FNET_REGISTER_PROTOCOL(classname,protoname) \
  static ProtocolSession* s3fnet_new_protocol_##classname	  \
    (char* protocolClassName, ProtocolGraph* pgraph) {		  \
      if(!strcasecmp(protoname, protocolClassName))		  \
        return new classname(pgraph); else return 0; }		  \
  int s3fnet_register_protocol_##classname =			  \
    Protocols::registerProtocol(s3fnet_new_protocol_##classname)

#define S3FNET_REGISTER_PROTOCOL_WITH_ALIAS(classname,protoname,alias) \
  static ProtocolSession* s3fnet_new_protocol_##classname	  \
    (char* protocolClassName, ProtocolGraph* pgraph) {		  \
      if(!strcasecmp(protoname, protocolClassName) ||		  \
	 !strcasecmp(alias, protocolClassName))			  \
        return new classname(pgraph); else return 0; }		  \
  int s3fnet_register_protocol_##classname =			  \
    Protocols::registerProtocol(s3fnet_new_protocol_##classname)

}; // namespace s3fnet
}; // namespace s3f

#endif /*__PROTOCOLS_H__*/

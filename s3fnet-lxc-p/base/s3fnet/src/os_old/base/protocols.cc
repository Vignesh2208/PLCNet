/**
 * \file protocols.cc
 * \brief Source file for the Protocols class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "os/base/protocols.h"
#include "os/base/protocol_session.h"

namespace s3f {
namespace s3fnet {

S3FNET_POINTER_VECTOR* Protocols::registered_protocols = 0;

int Protocols::registerProtocol(ProtocolSession* (*fct)(char*, ProtocolGraph*))
{
  // allocate the global data structure if it's not there
  if(registered_protocols == 0) {
    registered_protocols = new S3FNET_VECTOR(void*);
    assert(registered_protocols);
  }

  // add the protocol constructor (WE DON'T HAVE DUPLICATION CHECK!)
  registered_protocols->push_back((void*)fct);

  // we need to return something so that S3FNET_REGISTER_PROTOCOL macro works
  return 0;
}

ProtocolSession* Protocols::newInstance(char* classname, ProtocolGraph* graph)
{
  // if the protocol class name matches, return a new instance
  if(!registered_protocols) return 0;
  for(S3FNET_VECTOR(void*)::iterator iter = registered_protocols->begin();
      iter != registered_protocols->end(); iter++) {
    void* fct = *iter;
    ProtocolSession* sess = 
      (*(ProtocolSession*(*)(char*, ProtocolGraph*))fct)
      (classname, graph);
    if(sess) return sess;
  }
  return 0;
}

}; // namespace s3fnet
}; // namespace s3f

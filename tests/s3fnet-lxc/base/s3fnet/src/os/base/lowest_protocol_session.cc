/**
 * \file lowest_protocol_session.cc
 * \brief Source file for the LowestProtocolSession class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "os/base/lowest_protocol_session.h"

namespace s3f {
namespace s3fnet {

int LowestProtocolSession::control(int ctype, void* cmsg, ProtocolSession* sess)
{
   if(ctype == PSESS_CTRL_SESSION_IS_LOWEST)
   {
     assert(cmsg);
     *((bool*)cmsg) = true;
     return 0;
   }
   else return NicProtocolSession::control(ctype, cmsg, sess);
}

}; // namespace s3fnet
}; // namespace s3f

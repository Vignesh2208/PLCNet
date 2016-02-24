#ifndef __SERIAL_MESSAGE_H
#define __SERIAL_MESSAGE_H

#include "os/base/protocol_message.h"
#include "s3fnet.h"

namespace s3f {
namespace s3fnet {

#define RTS 1	// Request to Send
#define CTS 2	// Clear to Send
#define DATA 3	// Normal data
#define DNS 4	// Do not send anymore


/**
 * \brief IP header.
 */ 
class SerialMessage: public ProtocolMessage {
 public:
  /** The default constructor (without any argument). */
  SerialMessage();
  
  /** The copy constructor. */
  SerialMessage(const SerialMessage& iphdr);

  /** Clone the protocol message. */
  virtual ProtocolMessage* clone();
  
  /** Return the protocol number that the message is representing. */
  virtual int type() { return S3FNET_PROTOCOL_TYPE_SERIAL; }
  
  /**
   * This method returns the number of bytes that are needed to pack
   * this IP header. This is for serialization.
   */
  virtual int packingSize()
  {
    return KERN_BUF_SIZE + ProtocolMessage::packingSize();
  }
  
  /**
   * Return the number of bytes that a SERIAL header really occupies in
   * the real world, even though we might not actually allocating the
   * buffer with this size in simulation.
   */

  virtual int realByteCount() { return 0; }

  char src_lxcName[KERN_BUF_SIZE];

  int command;

  int length;

  char * data;

 protected:
  /** The destructor is protected from accidental invocation. */
  virtual ~SerialMessage();
};

};
};

#endif

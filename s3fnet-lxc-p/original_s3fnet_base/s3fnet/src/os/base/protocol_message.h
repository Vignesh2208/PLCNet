/**
 * \file protocol_message.h
 * \brief Header file for the ProtocolMessage class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __PROTOCOL_MESSAGE_H__
#define __PROTOCOL_MESSAGE_H__

#include "util/shstl.h"
#include "s3fnet.h"

namespace s3f {
namespace s3fnet {

/**
 * \brief Protocol message through the protocol stack.
 *
 * ProtocolMessage is the base class used for representing a packet
 * header and payload specific to a protocol. A protocol message is
 * organized as a single linked list. The reason why double linked
 * list is not used is that single linked list can better support
 * optimizations made by individual types of protocol messages (e.g.,
 * memory reference counters). The payload of a protocol message may
 * very well be the packet header for the upper protocol on the
 * protocol stack.
 *
 * Each derived protocol message class must register with the Messages
 * class using the S3FNET_REGISTER_MESSAGE macro. @see Messages class
 * for further details.
 *
 */
class ProtocolMessage : public Message
{
 public:
  /** Called by S3FNET_REGISTER_MESSAGE macro to register a protocol message. */
  static int registerMessage(ProtocolMessage* (*fct)(), int type);

  /** 
   * The default constructor.
   */
  ProtocolMessage();

  /**
   * Note that C++ does not guarantee that the copy constructor of the
   * base class is called when dealing with the copy constructor of
   * the derived class. The user must explicitly specify it!
   *
   * In a normal implementation, the copy constructor will copy not
   * only this object but also its payload. The copy constructor of
   * the base class is used to deal with cloning payload. It is
   * therefore very important that the base class's copy constructor
   * is called by the copy constructor of the derived class. Failing
   * this, the payload will be dropped implicitly and hideous bug
   * results.
   */
  ProtocolMessage(const ProtocolMessage& msg);

  /**
   * The destructor will reclaim the packet header as well as its
   * payload.  This method is intentionally set to be <b>protected</b>
   * so that the user may not delete the object explicitly. The erase
   * method should be used to reclaim the memory.
   */
  virtual ~ProtocolMessage();

  /**
   * This method is required for each derived class. This message
   * should be defined in the derived class as something like:
   <pre>
   ProtocolMessage* MyMessage::clone() 
     { return new MyMessage(*this); }
   </pre>,
   * where MyMessage is a derived class of ProtocolMessage.
   */ 
  virtual ProtocolMessage* clone() = 0;

  /**
   * Each protocol message has a unique type identifier returned by
   * this method. Subclass must override this method. The type of the
   * protocol message should be the same as the protocol number it is
   * representing so that a protocol session will be able to
   * demultiplex protocol messages and send them to the corresponding
   * protocol session above it.
   */
  virtual int type() = 0;

  /**
   * This method appends another protocol message to this protocol
   * message and treats it as the payload.
   */
  void carryPayload(ProtocolMessage* payload);


  /**
   * If there is a payload, drop it from the linked list, then return
   * it.  Return NULL if there is no payload.
   */
  virtual ProtocolMessage* dropPayload();

  /** Returns the next header. */
  ProtocolMessage* payload()  { return next; }

  /**
   * This method is called to release the memory of this packet and
   * its subsequent payload. One must avoid deleting a protocol
   * message object directly.  This requirement is necessary for the
   * support to optimizations such as the reference counter.
   *
   * For better protection, the destructor of the protocol message
   * (and its extended classes) should be defined as protected.
   */
  virtual void erase();

  virtual void erase_all();

  /** Returns the total byte count (in simulation) including the
      payload. */
  int totalPackingSize();

  /**
   * Returns the byte count (in simulation) only for this protocol
   * header.  In simulation, when the packet is sent cross timelines,
   * it will be packed into a byte array using serialize function. The
   * return value of packingSize() should return the size in bytes
   * this header will be packed into.  The derived class should
   * override this method. <b>It is very important to ensure that the
   * method in the base class be called</b>: the byte count here
   * should be included in the calculation of a protocol message.
   */
  virtual int packingSize();

  /** Returns the total number of bytes of the protocol message
      including the payload in the real-world. */
  int totalRealBytes();

  /**
   * Returns the <i>real</i> size only for this protocol header. The
   * default behavior is doing nothing except returning zero.
   */
  virtual int realByteCount() = 0;

  //@}

  /**
   * Given a protocol message type code, this method goes through the
   * message chain starting from this message, and returns the first
   * ProtocolMessage that has this type code.  If none is found, NULL
   * is returned.
   */
  ProtocolMessage* getMessageByType(int type_code);

  //int msg_id()  { return __msg_id; }

 //protected:
  /** A mapping between message type and message constructor. */
  static S3FNET_INT2PTR_MAP* registered_messages;

 private:
  /** Points to the protocol header after this node. */
  ProtocolMessage* next;
};

/** Each protocol message defined in the system must declare itself with the following macro. */
#define S3FNET_REGISTER_MESSAGE(name, type)	\
  static ProtocolMessage* s3fnet_new_message_##name() { return new name; } \
  int s3fnet_register_message_##name = ProtocolMessage::registerMessage(s3fnet_new_message_##name, type)

}; // namespace s3fnet
}; // namespace s3f

#endif /*__PROTOCOL_MESSAGE_H__*/

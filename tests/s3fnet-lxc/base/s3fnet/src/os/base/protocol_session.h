/**
 * \file protocol_session.h
 * \brief Header file for the ProtocolSession class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __PROTOCOL_SESSION_H__
#define __PROTOCOL_SESSION_H__

#include "dml.h"
#include "s3f.h"
#include "s3fnet.h"

namespace s3f {
namespace s3fnet {

class Host;
class ProtocolGraph;

/**
 * \brief A protocol layer on the protocol stack.
 *
 * The ProtocolSession class represents a protocol layer on the
 * ISO/OSI protocol stack. It's the base class for protocol
 * implementations. The class specifies default mechanisms for how a
 * protocol session should behave. The data path is specified by two
 * methods -- push() and pop() -- for receiving data from the protocol
 * session above and below. The data exchanged between protocol layers
 * are encapsulated in a ProtocolMessage object. The exchange of
 * control messages between protocol layers (not necessarily adjacent)
 * is implemented through invocations to the control()
 * method. Subclasses may override these methods with specific
 * behavior. Each protocol session class must register with the
 * Protocols class. @see Protocols class for further details.
 */
class ProtocolSession : public s3f::dml::Configurable {
  friend class ProtocolGraph;
  friend class NetworkInterface;
  friend class Host;

 public:
  /** Multiple protocol session instantiation type. */
  enum InstantiationType {
    PROT_UNIQUE_INSTANCE		= 0x00,
    PROT_MULTIPLE_INSTANCES		= 0x01,
    PROT_MULTIPLE_IMPLEMENTATIONS	= 0x02
  };

  /// A normal protocol session must go though the following stages.
  enum ExecutionStage {
    PSESS_CREATED	= 0, ///< this protocol session has just been created
    PSESS_CONFIGURED	= 1, ///< the config method of this protocol session has been called
    PSESS_INITIALIZED	= 2, ///< the init method of this protocol session has been called
    PSESS_WRAPPED_UP	= 3 /// the wrapup method of this protocol session has been called
  };

  /**
   * Symbolic name of the protocol this session implements. The name
   * of the protocol is derived from the DML 'name' attribute. It must
   * not be NULL once the protocol session is configured.
   */
  char* name;

  /**
   * Version number of this protocol implemenntation, represented as a
   * floating point number (e.g., 1.23).
   */
  float version;

  /**
   * Name of the protocol class from which this protocol session is
   * instantiated. The protocol class must be registered with the
   * Protocols class so that we will be able to instantiate an object
   * from the class. (C++ does not support self-reflection). The
   * class name must not be NULL once the protocol session is
   * configured.
   */
  char* use;

 public:
  /**
   * The config method is used to configure the protocol session from
   * DML description. It is expected to be called first, right after 
   * the ProtocolSession object is created and before the init method
   * is called. A DML configuration should be passed as an argument to
   * the method and it should be the value (a list of attributes) of a
   * 'session' attribute (inside 'graph'). The default behavior of
   * this method is to find out 'name' and 'use' attributes. The
   * derived class should override this method to obtain protocol
   * specific parameters from the DML description. However, the
   * derived class must not forget to call this method in the base
   * class so that the default behavior is carried out.
   * The config method recognizes the following attributes:
   *   <ul>
   *     <li><b>name</b>: name of the protocol session (required)
   *     <li><b>use</b>: protocol class name (required)
   *   </ul>
   * It is important to note that the timer still cannot be used
   * at this stage since the owner community hasn't been resolved. It
   * has to wait until after the init method is called. 
   */
  virtual void config(s3f::dml::Configuration* cfg);

  /**
   * The method is used to initialize the protocol session once the
   * protocol session has been created and configured. The default
   * behavior is doing nothing. The derived class may override this
   * method for protocol specific initialization. Note that the order
   * of initialization of the protocol sessions in a protocol graph
   * is unspecified. Don't rely on the ordering to help sort out the
   * relationships between protocols.
   */
  virtual void init();

  /**
   * The method is used to wrap up the protocol session. It is called
   * when the simulation finishes. It is designed to give user a
   * chance to report simulation statistics. The default behavior is
   * doing nothing. Subclass may override this method for protocol
   * specific dealings. Note that there is no specific order in which
   * wrapup is called for protocol sessions in a protocol graph.
   */
  virtual void wrapup();

  /**
   * The original push method is divided into pushdown() and push().
   * The pushdown method is the generic part of the code, which is
   * reserved for modeling the handling of messages transferred
   * between protocol layers. @see ProtocolSession::push method for
   * details.
   */
  int pushdown(Activation msg, ProtocolSession* hi_sess, void* extinfo = 0, size_t extinfo_size = 0);

  /**
   * The original pop method is divided into popup() and pop().  The
   * popup method is the generic part of the code, which is reserved
   * for modeling the handling of messages transferred between
   * protocol layers. @see ProtocolSession::pop method for details.
   */
  int popup(Activation msg, ProtocolSession* lo_sess, void* extinfo = 0, size_t extinfo_size = 0);

  /**
   * The control method is used by the protocol session to receive
   * control messages (or information queries) from other protocol
   * sessions both above and below this protocol session on the
   * stack. The derived class should override this method to provide
   * support for handling different control messages. In this case,
   * the derived class should invoke this method when encountering
   * a control type that it does not know to to process.
   *
   * The control messages are distinguished via types. The ctrl type
   * is an integer; its meaning is specified by the particular
   * protocol. The users are recommended to use ctrl type values
   * beyond 1000 (excluding 1000). The values within 1000 are
   * reserved.  The control message is of type void* and should be
   * cast to the appropriate types accordingly. Note that, unless
   * specified otherwise, the caller is responsible to reclaim the
   * control message once the method returns. The protocol session
   * that invoking this method will have a reference to itself in the
   * third argument.
   *
   * The method returns an integer, indicating whether the control
   * message is successfully processed.  Unless specified otherwise, a
   * zero means success. Non-zero means trouble and the returned value
   * can be used as the error number, the meaning of which is
   * specified by each particular protocol.
   *
   * For information on a specific control type,
   * see s3f::s3fnet::ProtocolSessionCtrlTypes
   */
  virtual int control(int ctrltyp, void* ctrlmsg, ProtocolSession* sess);

  /**
   * The method returns the protocol graph in which this protocol
   * session resides.
   */
  ProtocolGraph* inGraph() { return ingraph; }

  /** Return a pointer to the host owning this protocol session. */
  virtual Host* inHost();

  /**
   * Each protocol session should have its unique protocol number. The
   * number will be used to retrieve the protocol session object using
   * the protocol graph's sessionForNumber() method. The derived class
   * should override this method and return the correct protocol
   * number. The default is zero. The protocol number should be the
   * same as the type of the corresponding protocol message, which the
   * protocol uses to interact with the protocol session below it. A
   * protocol session may use the protocol message's type to
   * demultiplex the protocol messages at the destination and send the
   * message to the correct protocol session.
   */
  virtual int getProtocolNumber() = 0;

  /**
   * The method returns a pointer to the random number generator.
   */
  Random::RNG* getRandom(){ return sess_rng;}

  /**
   * The method returns the current simulation time. To be safe,
   * please do NOT call this function after initialization and before
   * wrapup phase. That is, it is unsafe to call this method in the
   * constructor, destructor, config, init, and wrapup methods.  The
   * time service is carried out by the host's owner community. This
   * method just directs the call to the community. It's given here
   * for convenience.
   */
  ltime_t getNow();

  /** For printing the current simulation time in a better readable format */
  char* getNowWithThousandSeparator();

  /**
   * This method returns true if the protocol session has been
   * configured. This method is used to make sure we configure the
   * protocol session before initializing it.
   */
  int configured() { return stage >= PSESS_CONFIGURED; }

  /**
   * This method returns true if the protocol session has been
   * initialized. This can be used to sequence calls to the init
   * method of protocol sessions. For example, if protocol session A
   * requires protocol session B to be initialized before A can be
   * initialized, A's init method can call B's init method as long as
   * B's initialized() returns true. The system does not enforce any
   * ordering in its calls the init methods of all protocol sessions
   * declared in the dml configuration.
   */
  int initialized() { return stage >= PSESS_INITIALIZED; }

  /**
   * This method returns true if the protocol session has been wrapped
   * up. This can be used to sequence calls to the wrapup method of
   * protocol sessions. For example, if protocol session A requires
   * protocol session B to be wrapped up before A can be wrapped up,
   * A's wrapup method can call B's wrapup method as long as B's
   * wrapped_up() returns true. The system does not enforce any
   * ordering in its calls the wrapup methods of all protocol sessions
   * declared in the dml configuration.
   */
  int wrapped_up() { return stage >= PSESS_WRAPPED_UP; }

  virtual const char* psesstype() { return "GENERIC"; } // DEBUG

 protected:
  /** The constructor is protected; only called from ProtocolGraph. */
  ProtocolSession(ProtocolGraph* graph);

  /** The destructor is protected; only ProtocolGraph reclaims it. */
  virtual ~ProtocolSession();

  /** 
   * This method controls how protocol instances can coexist with one
   * another. It returns one of the following:
   * <ul>
   *   <li>PROT_UNIQUE_INSTANCE (default): allows only one instance for
   *     each type of protocol per protocol stack;
   *   <li>PROT_MULTIPLE_INSTANCES: allows multiple instances of the
   *     same implementation for each type of protocol per protocol stack;
   *   <li>PROT_MULTIPLE_IMPLEMENTATIONS: allows multiple instances from
   *     different implementations of the same protocol to exist on the
   *     protocol stack.
   * </ul>
   */
  virtual int instantiation_type() { return PROT_UNIQUE_INSTANCE; }

 private:
  /**
   * The method is called when a message is pushed down to this
   * protocol session from the protocol session above. The protocol
   * session above calling this method will have a reference to
   * itself in the second argument. Extra information can be provided
   * as the third argument. The protocol message is given 
   * as the first argument. Once this method is called, the
   * message belongs to this session unless it's passed along, for
   * example, further down the protocol stack. That is, if the message
   * is to be dropped for any reason, this session must be responsible
   * to reclaim the message. Message exchange is implemented as method
   * calls for efficiency reasons. It consumes no simulation time
   * (even without context switch) unless users code it explicitly.
   * The extinfo parameter provides a means to pass any auxiliary information 
   * that doesn't belong in the protocol message with the call. extinfo_size 
   * must contain the size in bytes of the structure passed as extinfo. 
   * The extinfo data MUST NOT contain any pointer structures, since it will 
   * only be copied as a chunk of memory, based on the extinfo_size.
   * The return value semantics have changed since early versions. Normal 
   * return is zero. Non-zero means that the method has blocked and the caller 
   * MUST return immediately. The method will be called again with a re-entry
   * flag value after the time delay.
   */
  virtual int push(Activation msg, ProtocolSession* hi_sess, void* extinfo = 0, size_t extinfo_size = 0);

  /**
   * The method is called when a message is popped up to this
   * protocol session from the protocol session below. The protocol
   * session below calling this method will have a reference to
   * itself in the third argument. Extra information can be provided
   * as the third argument. The protocol message is given 
   * as the first argument, and the second parameter is the packet that holds
   * the chain of all the ProtocolMessages. There may be scenarios that the
   * packet does not exist, e.g. the host sends some message to itself. In that
   * situation, the second parameter should be NULL and the first parameter
   * MUST be the head of the chain of ProtocolMessages.
   * The extinfo parameter provides a means to pass any auxiliary information 
   * that doesn't belong in the protocol message with the call. extinfo_size 
   * must contain the size in bytes of the structure passed as extinfo. 
   * The extinfo data MUST NOT contain any pointer structures, since it will 
   * only be copied as a chunk of memory, based on the extinfo_size.
   * The return value semantics have changed since early versions. Normal 
   * return is zero. Non-zero means that the method has blocked and the caller 
   * MUST return immediately. The method will be called again with a reentry 
   * flag value after the time delay.
   *
   * The ownership of the the packet (including all messages) is
   * passed alone when pop is called.  It means that the callee (upper
   * layer) is responsible to call the erase() of the packet if
   * it decides to release all the memory. This also brings the
   * convenience that the upper layer can handle the whole packet
   * freely. There is NO exception for this rule.
   *
   * By default the ownership of the extinfo is kept by the caller.
   * It means that the caller(lower layer) is responsible to release
   * the memory used by extInfo if needed, However, there CAN be
   * exceptions of this rule if the caller and the callee has their
   * own agreement and have explicitly noted so.
   */
  virtual int pop(Activation msg, ProtocolSession* lo_sess, void* extinfo = 0, size_t extinfo_size = 0);

protected:
  /** Points to the protocol graph. */
  ProtocolGraph* ingraph;

  /** Pointer to a random number generator. */
  Random::RNG* sess_rng;

  /**
   * This variable indicates the current stage of this protocol
   * session. Each protocol session will go through four staged:
   * PSESS_CREATED, PSESS_CONFIGURED, PSESS_INITIALIZED, and then
   * PSESS_WRAPPED_UP.
   */
  int stage;
};

/**
 * Storing data for the waitFor()'s callback function in a protocol session.
 * By default, it stores a pointer for this protocol session.
 */
class ProtocolCallbackActivation : public Message
{
  public:
	ProtocolCallbackActivation(ProtocolSession* sess);
	virtual ~ProtocolCallbackActivation();
	ProtocolSession* session;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__PROTOCOL_SESSION_H__*/

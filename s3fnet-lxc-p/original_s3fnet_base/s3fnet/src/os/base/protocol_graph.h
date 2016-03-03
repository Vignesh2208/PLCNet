/**
 * \file protocol_graph.h
 * \brief Header file for the ProtocolGraph class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __PROTOCOL_GRAPH_H__
#define __PROTOCOL_GRAPH_H__

#include "util/shstl.h"
#include "dml.h"

namespace s3f {
namespace s3fnet {

class ProtocolSession;
class Host;

typedef S3FNET_MAP(S3FNET_STRING, ProtocolSession*) S3FNET_GRAPH_PROTONAME_MAP;
typedef S3FNET_MAP(int, ProtocolSession*) S3FNET_GRAPH_PROTONUM_MAP;
typedef S3FNET_VECTOR(ProtocolSession*) S3FNET_GRAPH_PSESS_VECTOR;

/**
 * \brief A stack of protocol sessions.
 *
 * The ProtocolGraph class represents a protocol graph, which is an
 * abstraction of the protocol stack in a  node, in a fashion 
 * similar to the ISO/OSI stack model. A protocol graph maintains a
 * list of protocol sessions, representing protocols running at
 * different layers. It serves as a container for the protocol 
 * sessions.
 */
class ProtocolGraph : public s3f::dml::Configurable {
  friend class ProtocolSession;

 public:
  /**
   * Optional string to describe the protocol graph. The string is NULL
   * if the DML 'graph' attribute does not contain a 'description'
   * attribute.
   */
  char* description;

 public:

  /**
   * The config method is used to configure the protocol graph. It is
   * expected to be called first, right after the ProtocolGraph object
   * is created and before the init method is called. A DML configuration
   * should be passed as an argument to the method and it should be
   * the value (a list of attributes) of a 'graph' attribute. An example
   * of the DML configuration is given as follows:
      <pre>
         graph [
           ProtocolSession [ name "protocol name" using "class name" ... ]
           ...
         ]
         interface [ id 0
           ProtocolSession [ name "mac" using "mac class name" ... ]
           ProtocolSession [ name "phy" using "phy class name" ... ]
         ]
         interface [ id 1 ... ]
     </pre>
   * The config method will configure each protocol session in the
   * protocol graph, represented by the 'session' attribute. Also, the
   * method will search for global attributes '.graph.session' and
   * instantiate each of the protocol sessions if it hasn't been
   * created. The network interface is configured by 'interface'
   * attribute, which optionally contain another list of protocol
   * sessions, namely the MAC session and the PHY session.
   */
  virtual void config(s3f::dml::Configuration* cfg);

  /**
   * The init method is used to initialize the protocol graph once it
   * has been configured. The init method will call init methods of
   * all protocol sessions in the protocol graph. The order in which
   * protocol sessions are initialized should NOT be expected!
   */
  virtual void init();

  /**
   * The wrapup method is called at the end of the simulation. It is
   * used to collect statistical information of the simulation run.
   * The wrapup method in turn calls the wrapup methods of all
   * protocol sessions in the protocol graph.  The order in which
   * protocol sessions wrap up should NOT be expected!
   */
  virtual void wrapup();

  /**
   * If a protocol session has been created and registered under the 
   * given name, the method returns the protocol session. Otherwise,
   * a NULL is returned.
   */
  ProtocolSession* sessionForName(char* pname);

  /**
   * If a protocol session has been created and registered under the 
   * given protocol number, the method returns the protocol session. 
   * Otherwise, a NULL is returned.
   */
  ProtocolSession* sessionForNumber(int pno);

 protected:
  /** Mapping between protocol names and instances of protocol sessions. */
  S3FNET_GRAPH_PROTONAME_MAP pname_map;

  /** Mapping between protocol numbers and instances of protocol sessions. */
  S3FNET_GRAPH_PROTONUM_MAP pno_map;

  /** List of protocol sessions created. */
  S3FNET_GRAPH_PSESS_VECTOR protocol_list;

 protected:
  /** The constructor. */
  ProtocolGraph();

  /** The destructor. */
  virtual ~ProtocolGraph();

  /** Internal use: to configure a protocol session. */
  ProtocolSession* config_session(s3f::dml::Configuration* cfg);

  /** Insert a protocol session to this protocol graph. */
  void insert_session(ProtocolSession* psess);
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__PROTOCOL_GRAPH_H__*/

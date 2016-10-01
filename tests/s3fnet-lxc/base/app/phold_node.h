/**
 * \file phold_node.h
 * \brief phold model for given topological graph
 */

#ifndef __PHOLD_NODE_H__
#define __PHOLD_NODE_H__

#include <s3f.h>

/**
 * class of phold node, implemented as an S3F entity
 */
class pHold_Node: public Entity {
public:
	int myid; ///< unique node id
	int myports; ///< for computing the next out message; equal to the number of node
	InChannel**  ic; ///< list of InChannels of this node
	OutChannel** oc; ///< list of OutChannels of this node
	Process*     listen_proc; ///< S3F process, listen on InChannels
	Process*     talk_proc; ///< S3F process, act after an activation is received on an InChannel

	Random::RNG rng; ///< %random number

	int next_out; ///< for computing the target outChannel for the next out message
	long recvd; ///<  count of total activations (listen + talk)

	/** default constructor */
	pHold_Node();

	/** constructor
	 *  @param tl the timeline that this entity is algined to.
	 *  @param name name of the phold node entity.
	 *  @param id assign to myid
	 *  @param ports assign to myports
	 */
	pHold_Node(Timeline* tl, string name, int id, int ports);

    /** Establish the mappings from this node's outchannel's  to the inchannels of the others.
	 *  Show use of name directories
	 */
	void init();

	/* process bodies. */
	/** "listen" waits on an inchannel. On receiving an activation, it samples a waiting time, and waitsFor that period */
	void listen(Activation);

	/**  after the waitsFor period specified in "listen", the "talk" process acts. */
	void talk(Activation);
};

/**
 * \brief The phold_Node simulation interface class for interacting with the underlying S3F API.
 */
class MyInterface : public Interface {
public:
	   /** the constructor
	    * @param tl the number of simulation threads to create (pthreads).
	    * @param ltps the time scale of the clock ticks in terms of log base 10 clock ticks per second.
	    */
	MyInterface(int tl, int ltps) : Interface(tl,ltps) {}

	/** make a clique of nodes
	 *  user needs to create own Interface class and instantiate BuildModel method.
	 */
	void BuildModel( int, int, vector<string> );
};

/** Messages transmitting among phold nodes */
class PholdMessage : public Message {
public:
	PholdMessage(int);
	virtual ~PholdMessage();
	virtual PholdMessage* clone();
	int msg_id()  { return __msg_id; }
	int count()   { return __count; }
	void inc()    { __count++; }
private:
	int __msg_id;
	int __count;
};


#endif /*__PHOLD_NODE_H__*/

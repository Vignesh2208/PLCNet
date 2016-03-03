/**
 * \file message.h
 *
 * \brief Header file for S3F Message
 */

#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#ifndef __S3F_H__
#error "message.h can only be included by s3f.h"
#endif

/**
 * Messages passed between connected OutChannels and InChannels
 */
class Message {
public:
	Message();
	Message(unsigned int);
	Message(const Message& msg);
	Message& operator= (Message const& msg);
	unsigned int get_type() { return __type; }
	virtual Message* clone();
	virtual void erase(){};
	virtual void erase_all(){};
	virtual ~Message();
	void inc_evts()     { __evts++; }
	int  dec_evts()     { return --__evts; }
protected:
	unsigned int __type;
	int __evts;

	/* ************************************************/
	/*    Standard Public Interface for Activation    */
	/*************************************************/

	/* Activation is the base class of all user-defined process activations. */
};

/* We use the standard library tr1 shared pointer to do reference
   counting and automated deletion of Activations.  The ActivationPtr
   class is used to pass pointers to Activations around
 */

#ifndef SAFE_MSG_PTR
typedef Message* Activation;
#else
typedef std::tr1::shared_ptr<s3f::Message> Activation;
#endif

#endif   // __MESSAGE_H__


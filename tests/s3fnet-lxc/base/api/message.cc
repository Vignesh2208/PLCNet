/**
 * \file message.cc
 *
 * \brief Source file for S3F Message
 */

#include <s3f.h>

#ifndef SAFE_MSG_PTR
Message::Message() { __type = 0; __evts = 0;  }
Message::Message(unsigned int t)  { __type = t; __evts = 0;  }
#else
Message::Message() { __type = 0; }
Message::Message(unsigned int t)  { __type = t; }
#endif

Message::~Message() { }
Message::Message(const Message& msg) {
	__type = msg.__type;
	__evts = 0;
}

Message& Message::operator= (Message const& msg) {
	__type = msg.__type;
	__evts = 0;
	return *this;
}

Message* Message::clone() { 
	Message* msg = new Message();
	msg->__type = __type;
	msg->__evts = 0;
	return msg;
}

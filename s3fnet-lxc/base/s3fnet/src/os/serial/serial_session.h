#ifndef __SERIAL_SESSION_H__
#define __SERIAL_SESSION_H__

#include "os/base/protocol_session.h"
#include "s3fnet.h"
#include "util/shstl.h"
#include "util/errhandle.h"
#include "net/host.h"
#include "os/base/protocols.h"
#include "net/network_interface.h"
#include "os/serial/serial_message.h"
#include <iostream>

namespace s3f {
namespace s3fnet {

#define SERIAL_PROTOCOL_CLASSNAME "S3F.OS.SERIAL"
#define SERIAL_DEVICE_CONTROL_FILE_NAME "/dev/s3fserial0"

typedef unsigned char uint8_t;


class SerialSession: public ProtocolSession {
	
	public:

		int dev_fd;
		uint8_t rts_mask;
		uint8_t pending_conn_mask;
		char * conn_to_lxc_map[NR_SERIAL_DEVS];
		NetworkInterface * conn_to_nic_map[NR_SERIAL_DEVS];
		Process* callback_proc;
		LXC_Proxy* sess_proxy;

		SerialSession(ProtocolGraph* graph);
		void flush_buffer(char * buf, int size);
		void injectEvent(ltime_t incoming_time, int conn_id);
		void reset_ioctl_conn(struct ioctl_conn_param * ioctl_conn);
		int get_rxbuf_space(int conn_id);
		void callback(Activation ac);
		void callback_body(SerialMessage * msg, NetworkInterface * outgoing_nic);
		int pop(Activation msg, ProtocolSession* lo_sess, void* extinfo, size_t extinfo_size);
		int push(Activation msg, ProtocolSession* hi_sess, void* extinfo, size_t extinfo_size);
		int getProtocolNumber();
		int control(int ctrltyp, void* ctrlmsg, ProtocolSession* sess);
		void init();
		void config(s3f::dml::Configuration* cfg);
		~SerialSession();


};

class SerialSessionCallbackActivation : public ProtocolCallbackActivation
{
  public:
	SerialSessionCallbackActivation(ProtocolSession* sess, int command, int conn_id,  char * data, int length);
	virtual ~SerialSessionCallbackActivation(){}

	int command;
	int conn_id;
	char * data;
	int length;
	
};

};
};

#endif

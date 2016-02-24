#ifndef __SERIAL_SESSION_H__
#define __SERIAL_SESSION_H__

#include "os/base/protocol_session.h"
#include "s3fnet.h"
#include "util/shstl.h"

namespace s3f {
namespace s3fnet {

#define SERIAL_PROTOCOL_CLASSNAME "S3F.OS.SERIAL"
#define SERIAL_DEVICE_CONTROL_FILE_NAME "/dev/s3fserial0"

struct ioctl_conn_param{

	int conn_id;						
	char owner_lxc_name[KERN_BUF_SIZE];	
	char dst_lxc_name[KERN_BUF_SIZE];
	int num_bytes_to_write;			 // number of bytes to write to rxbuf
	char bytes_to_write[RX_BUF_SIZE];// buffer from which data is copied to lxc's rx_buf
	int num_bytes_to_read;			 // number of bytes to read from txbuf
	char bytes_to_read[TX_BUF_SIZE]; // buffer to which data from txbuf is copied.
};

class SerialSession: public ProtocolSession {
	
	public:

		int dev_fd;
		uint8_t rts_mask;
		uint8_t pending_conn_mask;
		char * conn_to_lxc_map[NR_SERIAL_DEVS];
		NetworkInterface * conn_to_nic_map[NR_SERIAL_DEVS];
		Process* callback_proc;

		SerialSession(ProtocolGraph* graph);
		void flush_buffer(char * buf, int size);
		void injectEvent(ltime_t incoming_time, int conn_id);
		void reset_ioctl_conn(struct ioctl_conn_params * ioctl_conn);
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
	SerialSessionCallbackActivation(ProtocolSession* sess, int command, char * data, int length);
	virtual ~LXCEventSessionCallbackActivation(){}

	int command;
	int conn_id;
	char * data;
	int length;
	
};

};
};

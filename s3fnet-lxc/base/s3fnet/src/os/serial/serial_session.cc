#include "os/serial/serial_session.h"
#include <fcntl.h>

#ifdef SERIAL_DEBUG
#define SERIAL_DUMP(x) printf("SERIAL: "); x
#else
#define SERIAL_DUMP(x) x
#endif

namespace s3f {
namespace s3fnet {

S3FNET_REGISTER_PROTOCOL(SerialSession, SERIAL_PROTOCOL_CLASSNAME);

SerialSession::SerialSession(ProtocolGraph* graph) : 
  ProtocolSession(graph)
 {
 	SERIAL_DUMP(printf("Serial Session Constructor\n"));
 }

void SerialSession::config(s3f::dml::Configuration* cfg)
{
 // IMPORTANT, MUST BE CALLED FIRST!
 ProtocolSession::config(cfg);
 SERIAL_DUMP(printf("Serial Session Config done\n"));

}

SerialSession::~SerialSession(){
	int i = 0;
	SERIAL_DUMP(printf("Deleting Serial Session\n"));
	/*for(i = 0 ; i < NR_SERIAL_DEVS; i++){
		if(conn_to_lxc_map[i] != NULL)
			free(conn_to_lxc_map[i]);
	}*/

	
	if(callback_proc)
		delete callback_proc;

	if(dev_fd >= 0)
		close(dev_fd);
	
	
	SERIAL_DUMP(printf("Deleted Serial Session\n"));
}

void SerialSession::init()
{ 
    SERIAL_DUMP(printf("SerialSession::init [host=\"%s\"] init().\n", inHost()->nhi.toString()));
    ProtocolSession::init();

    if(strcasecmp(name, NET_PROTOCOL_NAME) && strcasecmp(name, SERIAL_PROTOCOL_NAME))
      error_quit("ERROR: SerialSession::init(), unmatched protocol name: \"%s\", expecting \"NET\" or \"serial\".\n", name);

  	
  	
  	assert(inHost()->proxy != NULL) ; 
  	sess_proxy = inHost()->proxy;	
  	dev_fd = open(SERIAL_DEVICE_CONTROL_FILE_NAME,O_RDWR);

  	assert(dev_fd >= 0);
  	rts_mask = 0;
  	pending_conn_mask = 0;
  	int i = 0;
  	for(i = 0; i < NR_SERIAL_DEVS; i++){
  		conn_to_lxc_map[i] = NULL;
  		conn_to_nic_map[i] = NULL;
  		conn_to_lxc_map[i] = (char *)malloc(KERN_BUF_SIZE*sizeof(char));

  		if(!conn_to_lxc_map[i])
  			error_quit("ERROR : SerialSession::init() - malloc error");
  		flush_buffer(conn_to_lxc_map[i],KERN_BUF_SIZE);
  		strcpy(conn_to_lxc_map[i],"NA");
  	}

  	Host* owner_host = inHost();
	callback_proc = new Process((Entity *) owner_host,(void (s3f::Entity::*)(s3f::Activation))&SerialSession::callback);

}

int SerialSession::control(int ctrltyp, void* ctrlmsg, ProtocolSession* sess)
{  

	switch(ctrltyp) {
		case IP_CTRL_VERIFY_TARGET_IP_ADDRESS : 
		case IP_CTRL_VERIFY_LOCAL_IP_ADDRESS : 
		case IP_CTRL_GET_FORWARDING_TABLE	: 
		case IP_CTRL_LOAD_FORWARDING_TABLE	: 
		case IP_CTRL_INSERT_ROUTE			:	 
		case IP_CTRL_DELETE_ROUTE			: 
		case IP_CTRL_INSERT_DMLROUTE		: 
		case IP_CTRL_ADD_INTERCEPT_SESSION	:
		case IP_CTRL_PUSH_REQUEST			: 
		case IP_CTRL_ADD_FIB_LISTENER		: 
		case IP_CTRL_FIB_ADDROUTE			: 
		case IP_CTRL_FIB_DELROUTE			:	SERIAL_DUMP(printf("Return from control\n"));

												return 0;
		
  		default : return ProtocolSession::control(ctrltyp, ctrlmsg, sess);

  }
}


int SerialSession::getProtocolNumber() { return S3FNET_PROTOCOL_TYPE_SERIAL; }


int SerialSession::push(Activation msg, ProtocolSession* hi_sess, void* extinfo, size_t extinfo_size){
	error_quit("ERROR: a message is pushed down to the Serial session from protocol layer above; it's impossible.\n");
	return 0;

}

int SerialSession::pop(Activation msg, ProtocolSession* lo_sess, void* extinfo, size_t extinfo_size){

	
	struct ioctl_conn_param ioctl_conn;
	int i = 0, conn_id = -1;
	int space_avail = 0;
	int command = 0;
	int length = 0;
	char * data;
	
	//check if it is a LxcemuMessage
	Host * owner_host = inHost();
	NetworkInterface * nic;
	ProtocolMessage* message = (ProtocolMessage*) msg;
	if (message->type() != S3FNET_PROTOCOL_TYPE_SERIAL) {
		error_quit(
				"ERROR: the message popup to Serial session is not S3FNET_PROTOCOL_TYPE_SERIAL.\n");
	}

	LXC_Proxy* proxy = inHost()->proxy;

	assert(proxy != NULL);

	
	ltime_t timelineTime = inHost()->alignment()->now();
	ltime_t proxyVT = proxy->getElapsedTime();
	SerialMessage * recv_msg = (SerialMessage*)msg;

	SERIAL_DUMP(printf("Received message at lxc = %s, data = %s\n",proxy->lxcName,recv_msg->data));

	for(i = 0; i < NR_SERIAL_DEVS; i++){
		if(strcmp(conn_to_lxc_map[i],recv_msg->src_lxcName) == 0){
			conn_id = i;
			break;
		}
	}

	if(conn_id == -1){
		reset_ioctl_conn(&ioctl_conn);	
		strncpy(ioctl_conn.owner_lxc_name,proxy->lxcName,KERN_BUF_SIZE);
		strncpy(ioctl_conn.dst_lxc_name,recv_msg->src_lxcName,KERN_BUF_SIZE);
		if(ioctl(dev_fd,S3FSERIAL_GETCONNID,&ioctl_conn) < 0){
			SERIAL_DUMP(printf("ERROR SerialSession::pop ioctl GETCONNID"));
			free(recv_msg->data);
			//delete recv_msg->data;
			//delete recv_msg;
			return 0;
		}
		assert(ioctl_conn.conn_id >=0 && ioctl_conn.conn_id < NR_SERIAL_DEVS);
		conn_id = ioctl_conn.conn_id;
		SERIAL_DUMP(printf("Resolved conn_id of received message = %d\n",conn_id));
		strncpy(conn_to_lxc_map[conn_id], recv_msg->src_lxcName,KERN_BUF_SIZE);
	}


	if(conn_to_nic_map[conn_id] == NULL){
		for(i = 0; i < NR_SERIAL_DEVS; i++){
			nic = owner_host->getNetworkInterface(i);
			if(nic != NULL){
				if(nic->get_dst_nic() != NULL){
					if(strcmp(nic->get_dst_nic()->getHost()->proxy->lxcName,recv_msg->src_lxcName) == 0){
						conn_to_nic_map[conn_id] = nic;
						break;
					}
				}
			}
		}
		assert(conn_to_nic_map[conn_id] != NULL);
		SERIAL_DUMP(printf("Resolved outgoing nic for received message\n"));
	}

	assert(recv_msg->length > 0);
	space_avail = get_rxbuf_space(conn_id);

	SERIAL_DUMP(printf("Space available in rx buf = %d\n",space_avail));

	if(recv_msg->command == RTS){
		if(space_avail >= TX_BUF_SIZE){
			// send CTS
			command = CTS;
			length = 4;
			//data =  new char[4];
			data =  (char *)malloc(4*sizeof(char));
			strcpy(data,"CTS");
		}
	}
	else{

		// may have to send DNS
		if(space_avail < TX_BUF_SIZE){
			// send DNS
			command = DNS;
			length = 4;
			//data = new char[4];
			data =  (char *)malloc(4*sizeof(char));
			strcpy(data,"DNS");
		}


		if(recv_msg->command == CTS){
			// received CTS
			rts_mask |= (1 << conn_id);
			// TODO : Possibly send the data if any here.
			goto end;
		}
		else if(recv_msg->command == DNS){
			// received DNS
			rts_mask &= ~(1 << conn_id);
			goto end;
		}

		// if received data		
		// copy whatever can be copied to the rx buf
		reset_ioctl_conn(&ioctl_conn);	
		strncpy(ioctl_conn.owner_lxc_name,proxy->lxcName,KERN_BUF_SIZE);
		strncpy(ioctl_conn.dst_lxc_name,recv_msg->src_lxcName,KERN_BUF_SIZE);
		ioctl_conn.conn_id = conn_id;
		if(recv_msg->length < space_avail)
			space_avail = recv_msg->length;

		ioctl_conn.num_bytes_to_write = space_avail;
		memcpy(ioctl_conn.bytes_to_write,recv_msg->data,space_avail);
		SERIAL_DUMP(printf("Writing msg data = %s, to RX buffer\n",ioctl_conn.bytes_to_write));

		ioctl(dev_fd,S3FSERIAL_IOWRX,&ioctl_conn); // write to rx buf. application would later read it when it wakes up.
	}

	end:
	free(recv_msg->data);
	//delete recv_msg->data;
	//delete recv_msg;

	if(command){
		// need to send out a CTS or DNS immediately
		SerialSessionCallbackActivation* oac = new SerialSessionCallbackActivation(this, command,conn_id,data, length);
		Activation ac (oac);
		HandleCode h = owner_host->waitFor( callback_proc, ac, 0, owner_host->tie_breaking_seed);
	}


	long difference = (timelineTime - proxyVT);
	if (difference < 0) //Timeline time < virtual time meaning it was sent early
		proxy->packetsSentLate++;
	else if (difference > 0)
		proxy->packetsSentEarly++;
	else
		proxy->packetsSentOnTime++;

	proxy->totalPacketError += labs(difference);
	proxy->packetsSentOut++;


	SERIAL_DUMP(printf("Handled msg recv at lxc = %s\n",proxy->lxcName));

	return 0;


}

void SerialSession::callback(Activation ac){

	SERIAL_DUMP(printf("Inside SerialSession Send Callback\n"));
	SerialSession* sess = (SerialSession*)((SerialSessionCallbackActivation*)ac)->session;
	int command = ((SerialSessionCallbackActivation*)ac)->command;
	int conn_id = ((SerialSessionCallbackActivation*)ac)->conn_id;
	char * data = ((SerialSessionCallbackActivation*)ac)->data;
	int length =  ((SerialSessionCallbackActivation*)ac)->length;
	SERIAL_DUMP(printf("Inside SerialSession Send Callback. Extracted params\n"));

	assert(conn_id >= 0 && conn_id < NR_SERIAL_DEVS);
	assert(length > 0);
	assert(sess != NULL);

	assert(sess->conn_to_nic_map[conn_id] != NULL);	
	NetworkInterface * out_nic = sess->conn_to_nic_map[conn_id];
	SerialMessage * msg = new SerialMessage();
	assert(msg != NULL);

	SERIAL_DUMP(printf("copying proxy lxcname\n"));
	strcpy(msg->src_lxcName,sess->sess_proxy->lxcName);	
	SERIAL_DUMP(printf("Calling callback body\n"));
	msg->data = data;
	msg->command = command;
	msg->length = length;

	if(msg->command == DATA){
		// connection is no longer pending
		pending_conn_mask &= ~(1 << conn_id);
	}
	
	sess->callback_body(msg,out_nic);
}

void SerialSession::callback_body(SerialMessage * msg, NetworkInterface * outgoing_nic){

	ProtocolSession* outgoing_mac = outgoing_nic->getHighestProtocolSession();
	assert(outgoing_mac != NULL);
	SERIAL_DUMP(printf("Pushing down to Dummy Mac Layer\n"));
    outgoing_mac->pushdown(msg, this, NULL, 0);
}


void SerialSession::injectEvent(ltime_t incoming_time, int conn_id){


	Host * owner_host = inHost();
	NetworkInterface * nic;
	char * data;
	int command = 0;
	int i = 0;
	int length = 0;
	struct ioctl_conn_param ioctl_conn;
	char * dst_lxc_name = NULL;
	assert(inHost()->proxy != NULL);

	SERIAL_DUMP(printf("Injecting new Serial Event\n"));
	reset_ioctl_conn(&ioctl_conn);
	SERIAL_DUMP(printf("ioctl_conn param reset\n"));
	
	strcpy(ioctl_conn.owner_lxc_name,inHost()->proxy->lxcName);
	ioctl_conn.conn_id = conn_id;

	if(ioctl(dev_fd,S3FSERIAL_GETCONNLXC, &ioctl_conn) < 0){
		SERIAL_DUMP(printf("ERROR SerialSession::injectEvent ioctl GETCONNLXC"));
		return;
	}

	SERIAL_DUMP(printf("finished ioctl call\n"));

	dst_lxc_name = ioctl_conn.dst_lxc_name;

	if(conn_id < NR_SERIAL_DEVS && conn_id >= 0 && dst_lxc_name != NULL){
		if(strcmp(conn_to_lxc_map[conn_id],"NA") == 0)
			strcpy(conn_to_lxc_map[conn_id], dst_lxc_name);

		SERIAL_DUMP(printf("updating conn_to_nic_map\n"));
		if(conn_to_nic_map[conn_id] == NULL){
			for(i = 0; i < NR_SERIAL_DEVS; i++){
				nic = owner_host->getNetworkInterface(i);
				if(nic != NULL){
					if(nic->get_dst_nic() != NULL){					
						if(strcmp(nic->get_dst_nic()->getHost()->proxy->lxcName,dst_lxc_name) == 0){
							conn_to_nic_map[conn_id] = nic;
							break;
						}
					}
				}

			}

		}

		assert(conn_to_nic_map[conn_id] != NULL);
		pending_conn_mask |= (1 << conn_id);

		SERIAL_DUMP(printf("finished updating all maps\n"));
	}
	else
		error_quit("ERROR : SerialSession::injectEvent invalid connection_id");

	reset_ioctl_conn(&ioctl_conn);
	assert(inHost()->proxy != NULL);
	strncpy(ioctl_conn.owner_lxc_name,inHost()->proxy->lxcName,KERN_BUF_SIZE);

	SERIAL_DUMP(printf("checking if rts needs to be sent\n"));
	fflush(stdout);
	if(rts_mask & (1 << conn_id)){
		// got cts
		command = DATA;
		ioctl_conn.conn_id = conn_id;
		if(ioctl(dev_fd,S3FSERIAL_IORTX,&ioctl_conn) < 0)
			error_quit("ERROR : SerialSession::injectEvent error ioctl rtx");

		assert(ioctl_conn.num_bytes_to_read > 0);
		//data = new char[ioctl_conn.num_bytes_to_read];
		data = (char *)malloc((ioctl_conn.num_bytes_to_read+1)*sizeof(char));
		memcpy(data,ioctl_conn.bytes_to_read,ioctl_conn.num_bytes_to_read);
		length = ioctl_conn.num_bytes_to_read;
		
	}
	else{
		// have to send rts
		command = RTS;
		//data = new char[4];
		data = (char*)malloc(4*sizeof(char));
		strcpy(data,"RTS");
		length = 4;
	}

	SERIAL_DUMP(printf("creating new serial session callback\n"));

	SerialSessionCallbackActivation* oac = new SerialSessionCallbackActivation(this, command,conn_id,data, length);
	Activation ac (oac);
	ltime_t timelineTime       = inHost()->alignment()->now();
	ltime_t packetIncomingTime = incoming_time;
	ltime_t wait_time = packetIncomingTime - timelineTime;
	LXC_Proxy* proxy = owner_host->proxy;

	if (timelineTime > packetIncomingTime)
	{
		proxy->packetsInjectedIntoPast++;
		proxy->totalTimeInjectedIntoPast += (timelineTime - packetIncomingTime);
	}
	else if (timelineTime < packetIncomingTime)
	{
		proxy->packetsInjectedIntoFuture++;
		proxy->totalTimeInjectedIntoFuture += (packetIncomingTime - timelineTime);
	}
	else
	{
		proxy->packetsInjectedAtCorrectTime++;
	}

	if (wait_time < 0)
	{
		wait_time = 0;
	}
	assert(wait_time >= 0);

	// We get a packet from an LXC, send it immediately through the simulation
	// by "Scheduling" it onto the list of events to process.

	SERIAL_DUMP(printf("scheduling new serial session callback\n"));

	HandleCode h = owner_host->waitFor( callback_proc, ac, wait_time, owner_host->tie_breaking_seed);


}

void SerialSession::flush_buffer(char * buf, int size){
	int i = 0;
	for(i = 0; i < size; i++)
		buf[i] = '\0';
}

void SerialSession::reset_ioctl_conn(struct ioctl_conn_param * ioctl_conn){
	flush_buffer(ioctl_conn->owner_lxc_name,KERN_BUF_SIZE);
	flush_buffer(ioctl_conn->dst_lxc_name,KERN_BUF_SIZE);
  	flush_buffer(ioctl_conn->bytes_to_write,RX_BUF_SIZE);
  	flush_buffer(ioctl_conn->bytes_to_read,TX_BUF_SIZE);
  	ioctl_conn->conn_id = 0;
  	ioctl_conn->num_bytes_to_write = 0;
  	ioctl_conn->num_bytes_to_read = 0;
}

int SerialSession::get_rxbuf_space(int conn_id){

	struct ioctl_conn_param ioctl_conn;
	reset_ioctl_conn(&ioctl_conn);
	int num_recv_bytes;
	strncpy(ioctl_conn.owner_lxc_name,inHost()->proxy->lxcName,KERN_BUF_SIZE);
	ioctl_conn.conn_id = conn_id;

	if(ioctl(dev_fd,S3FSERIAL_GETCONNSTATUS,&ioctl_conn) < 0)
		return 0;

	assert(ioctl_conn.num_bytes_to_read >= 0);
	num_recv_bytes = ioctl_conn.num_bytes_to_read;

	return RX_BUF_SIZE - num_recv_bytes;
}

SerialSessionCallbackActivation::SerialSessionCallbackActivation
(ProtocolSession* sess, int _command, int _conn_id, char *_data, int _length) :
	ProtocolCallbackActivation(sess),
	command(_command),
	conn_id(_conn_id),
	data(_data),
	length(_length)
{
}

};
};

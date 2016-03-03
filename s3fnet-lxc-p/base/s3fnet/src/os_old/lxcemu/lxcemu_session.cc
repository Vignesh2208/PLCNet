/**
 * \file lxcemu_session.cc
 * \brief Source file for the LxcemuSession class. Adapted from dummy_session.cc
 *
 * authors : Vladimir Adam
 */

#include "os/lxcemu/lxcemu_session.h"
#include "os/lxcemu/lxcemu_message.h"
#include "os/ipv4/ip_session.h"
#include "util/errhandle.h" // defines error_quit() method
#include "net/host.h" // defines Host class
#include "os/base/protocols.h" // defines S3FNET_REGISTER_PROTOCOL macro
#include "os/ipv4/ip_interface.h" // defines IPPushOption and IPOptionToAbove classes
#include "net/net.h"
#include "env/namesvc.h"

#ifdef LXCEMU_DEBUG
#define LXCEMU_DUMP(x) printf("LXCEMU: "); x
#else
#define LXCEMU_DUMP(x)
#endif

namespace s3f {
namespace s3fnet {

S3FNET_REGISTER_PROTOCOL(LxcemuSession, LXCEMU_PROTOCOL_CLASSNAME);

LxcemuSession::LxcemuSession(ProtocolGraph* graph) :
		ProtocolSession(graph) {
	// create your session-related variables here
	LXCEMU_DUMP(printf("A lxcemu protocol session is created.\n"));
}

LxcemuSession::~LxcemuSession() {
	// reclaim your session-related variables here
	LXCEMU_DUMP(printf("A lxcemu protocol session is reclaimed.\n"));
	if (callback_proc)
		delete callback_proc;
}

void LxcemuSession::config(s3f::dml::Configuration* cfg) {
	// the same method at the parent class must be called
	ProtocolSession::config(cfg);
	// parameterize your protocol session from DML configuration
}

void LxcemuSession::init() {
	// the same method at the parent class must be called
	ProtocolSession::init();

	// initialize the session-related variables here
	LXCEMU_DUMP(printf("Lxcemu session is initialized.\n"));
	pkt_seq_num = 0;

	// we couldn't resolve the NHI to IP translation in config method,
	// but now we can, since name service finally becomes functional
		// similarly we couldn't resolve the IP layer until now
	ip_session = (IPSession*) inHost()->getNetworkLayerProtocol();
	if (!ip_session)
		error_quit("ERROR: can't find the IP layer; impossible!");

	Host* owner_host = inHost();
	callback_proc = new Process((Entity *) owner_host,
			(void (s3f::Entity::*)(s3f::Activation))&LxcemuSession::callback);

}

void LxcemuSession::callback(Activation ac)
{
	//LxcemuSession* ds = (LxcemuSession*) ((ProtocolCallbackActivation*) ac)->session;
	//ds->callback_body(ac);

	LxcemuSession* sess = (LxcemuSession*)((LXCEventSessionCallbackActivation*)ac)->session;
	EmuPacket* packet = ((LXCEventSessionCallbackActivation*)ac)->packet;
	IPADDR dst_ip = 	((LXCEventSessionCallbackActivation*)ac)->dst_ip;
	IPADDR src_ip = 	((LXCEventSessionCallbackActivation*)ac)->src_ip;
	sess->callback_body(packet, src_ip, dst_ip);
}

void LxcemuSession::callback_body(EmuPacket* packet, IPADDR srcIP, IPADDR destIP)
{
	LxcemuMessage* dmsg = new LxcemuMessage();
	dmsg->ppkt = packet;

	Activation dmsg_ac(dmsg);

	IPPushOption ipopt;
	ipopt.dst_ip = destIP;
	ipopt.src_ip = IPADDR_INADDR_ANY;
	ipopt.prot_id = S3FNET_PROTOCOL_TYPE_LXCEMU;
	ipopt.ttl = DEFAULT_IP_TIMETOLIVE;
	ip_session->pushdown(dmsg_ac, this, (void*) &ipopt, sizeof(IPPushOption));

}

int LxcemuSession::control(int ctrltyp, void* ctrlmsg, ProtocolSession* sess) {
	switch (ctrltyp) {
	case LXCEMU_CTRL_COMMAND1:
	case LXCEMU_CTRL_COMMAND2:
		return 0; // lxcemu control commands, we do nothing here
	default:
		return ProtocolSession::control(ctrltyp, ctrlmsg, sess);
	}
}

int LxcemuSession::push(Activation msg, ProtocolSession* hi_sess, void* extinfo,
		size_t extinfo_size) {
	error_quit(
			"ERROR: a message is pushed down to the lxcemu session from protocol layer above; it's impossible.\n");
	return 0;
}

int LxcemuSession::pop(Activation msg, ProtocolSession* lo_sess, void* extinfo,
		size_t extinfo_size) {

	/*
	 * This means that a packet is ready to be sent over to the LXC. It has gone through the simulator
	 * The LXC is frozen at the desired virtual time so it needs to be injected
	 *
	 */

	LXCEMU_DUMP(printf("A message is popped up to the lxcemu session from the IP layer.\n"));
	char* pkt_type = "lxcemu";

	//check if it is a LxcemuMessage
	ProtocolMessage* message = (ProtocolMessage*) msg;
	if (message->type() != S3FNET_PROTOCOL_TYPE_LXCEMU) {
		error_quit(
				"ERROR: the message popup to lxcemu session is not S3FNET_PROTOCOL_TYPE_LXCEMU.\n");
	}

	LXC_Proxy* proxy = inHost()->proxy;
	assert(proxy != NULL);

	ltime_t timelineTime = inHost()->alignment()->now();

	// the protocol message must be of LxcemuMessage type, and the extra info must be of IPOptionToAbove type
	LxcemuMessage* dmsg = (LxcemuMessage*) msg;
	IPOptionToAbove* ipopt = (IPOptionToAbove*) extinfo;
	dmsg->ppkt->outgoingTime = timelineTime;

	EmuPacket* pkt = dmsg->ppkt;
	assert(pkt->outgoingFD == proxy->fd);

	proxy->lxcMan->cwrite(proxy->fd, (char*) pkt->data, pkt->len );

	ltime_t proxyVT = proxy->getElapsedTime();
	long difference = (timelineTime - proxyVT);
	if (difference < 0) //Timeline time < virtual time meaning it was sent early
		proxy->packetsSentLate++;
	else if (difference > 0)
		proxy->packetsSentEarly++;
	else
		proxy->packetsSentOnTime++;

	proxy->totalPacketError += labs(difference);
	proxy->packetsSentOut++;

	delete pkt;
	dmsg->erase_all();

	// returning 0 indicates success
	return 0;
}

void LxcemuSession::injectEvent(EmuPacket* packet, IPADDR srcIP, IPADDR destIP)
{
	/* Here, we got a packet from an LXC by listening to the TAP. We want to inject at its virtual time.
	 * To do that, we need to figure out where the timeline currently is, and figure out the VT of the packet
	 * when it was sent. We then take the difference (PACKET_VIRTUALTIME - SIMULATOR_VIRTUAL_TIME) and schedule it from that moment
	 */

	LXCEventSessionCallbackActivation* oac = new LXCEventSessionCallbackActivation(this, destIP, srcIP, packet);
	Activation ac (oac);

	Host* owner_host = inHost();
	ltime_t timelineTime       = inHost()->alignment()->now();
	ltime_t packetIncomingTime = packet->incomingTime;
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

	//printf("Injecting Event! Timeline %u Time %ld | Incoming Time %ld\n", inHost()->alignment()->s3fid(),
	//		timelineTime, packetIncomingTime );

	if (wait_time < 0)
	{
		wait_time = 0;
	}
	assert(wait_time >= 0);

	// We get a packet from an LXC, send it immediately through the simulation
	// by "Scheduling" it onto the list of events to process.

	//printf("__________________________Scheduling an event for %lu\n", wait_time);
	HandleCode h = owner_host->waitFor( callback_proc, ac, wait_time, owner_host->tie_breaking_seed);
}

LXCEventSessionCallbackActivation::LXCEventSessionCallbackActivation
(ProtocolSession* sess, IPADDR _dst_ip, IPADDR _src_ip, EmuPacket* ppkt) :
	ProtocolCallbackActivation(sess),
	dst_ip(_dst_ip),
	src_ip(_src_ip),
	packet(ppkt)
{

}

}
;
// namespace s3fnet
}
;
// namespace s3f

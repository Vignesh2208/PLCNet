/**
 * \file droptail_queue.cc
 * \brief Source file for the DroptailQueue class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "net/droptail_queue.h"
#include "util/errhandle.h"
#include "net/host.h"
#include "net/network_interface.h"
#include "os/base/protocol_message.h"

#ifdef IFACE_DEBUG
#define IFACE_DUMP(x) printf("NIC: "); x
#else
#define IFACE_DUMP(x)
#endif

namespace s3f {
namespace s3fnet {

S3FNET_REGISTER_QUEUE(DroptailQueue, DROPTAIL_QUEUE_NAME);

DroptailQueue::DroptailQueue(LowestProtocolSession* lowsess) :
  NicQueue(lowsess), last_xmit_time(0), queue_delay(0) {}

void DroptailQueue::init()
{
  NicQueue::init();

  double max_queue_delay_double = 8 * bufsize / bitrate;
  max_queue_delay = phy_sess->inHost()->d2t(max_queue_delay_double, 0)+1;
}

void DroptailQueue::enqueue(Activation pkt)
{
	assert(phy_sess);
	ProtocolMessage* msg = (ProtocolMessage*)pkt;
	int len = msg->totalRealBytes();
	IFACE_DUMP(printf("DroptailQueue::enqueue() pkt_len = %d\n", len););
	
	if (len <= 0)
		error_quit("DroptailQueue::enqueue(), pkt len <= 0\n");
	
	ltime_t jitter = calibrate(len);
	double transmission_time_double = 8.0*len/bitrate;
	ltime_t transmission_time = phy_sess->inHost()->d2t(transmission_time_double, 0)+1;
	ltime_t testqueue_delay = queue_delay + transmission_time;
	
	if(testqueue_delay > max_queue_delay) 
	{
		// drop the entire packet if the queue full.
		IFACE_DUMP(printf("[nic=\"%s\", ip=\"%s\"] %ld: drop packet (queue overflow).\n",
				   ((NetworkInterface*)phy_sess->inGraph())->nhi.toString(),
				   IPPrefix::ip2txt(((NetworkInterface*)phy_sess->inGraph())->getIP()),
				   phy_sess->getNow()));

		pkt->erase_all();
		return;
	}

  // calculate queueing delay
  queue_delay = testqueue_delay+jitter+latency;

  IFACE_DUMP(printf("DroptailQueue::enqueue() jitter = %ld, transmission_time = %ld, "
			"current_queue_delay = %ld, latency = %ld\n",
				jitter, transmission_time, queue_delay, latency););

  // call the interface to send the packet.
  phy_sess->sendPacket(pkt, queue_delay);
}

ltime_t DroptailQueue::calibrate(int len)
{
  ltime_t jitter(0);
  ltime_t now = phy_sess->getNow();

  if(jitter_range > 0) // if jitter is enabled
  {
    double jitter_double = phy_sess->getRandom()->Uniform(-1, 1) * jitter_range * len * 8 / bitrate;
    jitter = phy_sess->inHost()->d2t(jitter_double, 0)+1;
    now += jitter;
  }

  if(now > last_xmit_time) // it's possible jitter makes now < last_xmit_time
  {
    queue_delay -= (now - last_xmit_time);
    if (queue_delay < 0) queue_delay = 0;
    last_xmit_time = now;
  }
  return jitter;
}

}; // namespace s3fnet
}; // namespace s3f

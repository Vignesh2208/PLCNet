/**
 * \file red_queue.cc
 * \brief Source file for the RedQueue class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "net/red_queue.h"
#include "net/host.h"
#include "util/errhandle.h"
#include "net/network_interface.h"
#include "os/simple_mac/simple_mac_message.h"

#ifdef IFACE_DEBUG
#define IFACE_DUMP(x) printf("RED: "); x
#else
#define IFACE_DUMP(x)
#endif

#define RED_QUEUE_DEFAULT_WEIGHT 0.0001
#define RED_QUEUE_DEFAULT_PMAX 0.2
#define RED_QUEUE_DEFAULT_MEAN_PKTSIZ (500*8.0)

namespace s3f {
namespace s3fnet {

S3FNET_REGISTER_QUEUE(RedQueue, RED_QUEUE_NAME);

RedQueue::RedQueue(LowestProtocolSession* lowsess) :
  NicQueue(lowsess),
  weight(RED_QUEUE_DEFAULT_WEIGHT),
  qmin(0), qmax(0), qcap(0), pmax(RED_QUEUE_DEFAULT_PMAX), 
  wait_opt(true), mean_pktsiz(RED_QUEUE_DEFAULT_MEAN_PKTSIZ),
  queue(0), avgque(0), loss(0), crossing(false), interdrop(0),
  last_update_time(0), vacate_time(0) {}

RedQueue::~RedQueue() {}

void RedQueue::config(s3f::dml::Configuration* cfg)
{
  NicQueue::config(cfg);
  
  char* str = (char*)cfg->findSingle("weight");
  if(str) {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: RedQueue::config(), "
		 "invalid INTERFACE.WEIGHT attribute!\n");
    weight = atof(str);
    if(weight < 0 || weight >= 1)
      error_quit("ERROR: RedQueue::config(), "
		 "INTERFACE.WEIGHT (%s) must be between 0 and 1.\n", str);
  }

  str = (char*)cfg->findSingle("qmin");
  if(str) {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: RedQueue::config(), "
		 "invalid INTERFACE.QMIN attribute!\n");
    qmin = 8*atol(str); // bits rather than bytes
    if(qmin <= 0)
      error_quit("ERROR: RedQueue::config(), "
		 "INTERFACE.QMIN (%s) is non-positive.\n", str);
  }

  str = (char*)cfg->findSingle("qmax");
  if(str) {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: RedQueue::config(), "
		 "invalid INTERFACE.QMAX attribute!\n");
    qmax = 8*atol(str); // bits rather than bytes
    if(qmax <= 0)
      error_quit("ERROR: RedQueue::config(), "
		 "INTERFACE.QMAX (%s) is non-positive.\n", str);
  }

  str = (char*)cfg->findSingle("qcap");
  if(str) {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: RedQueue::config(), "
		 "invalid INTERFACE.QCAP attribute!\n");
    qcap = 8.0*atof(str); // bits rather than bytes
    if(qcap <= 0)
      error_quit("ERROR: RedQueue::config(), "
		 "INTERFACE.QCAP (%s) is non-positive.\n", str);
  }

  str = (char*)cfg->findSingle("pmax");
  if(str) {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: RedQueue::config(), "
		 "invalid INTERFACE.PMAX attribute!\n");
    pmax = atof(str);
    if(pmax <= 0 || pmax > 1)
      error_quit("ERROR: RedQueue::config(), "
		 "INTERFACE.PMAX (%s) must be in the range (0, 1].\n", str);
  }

  str = (char*)cfg->findSingle("wait");
  if(str) {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: RedQueue::config(), "
		 "invalid INTERFACE.WAIT attribute!\n");
    if(!strcasecmp(str, "true")) wait_opt = true;
    else if(!strcasecmp(str, "false")) wait_opt = false;
    else error_quit("ERROR: RedQueue::config(), "
		    "INTERFACE.WAIT (%s) must either be true or false.\n", str);
  }

  str = (char*)cfg->findSingle("mean_pktsiz");
  if(str) {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: RedQueue::config(), "
		 "invalid INTERFACE.MEAN_PKTSIZ attribute!\n");
    mean_pktsiz = 8.0*atof(str); // bits rather than bytes
    if(mean_pktsiz <= 0)
      error_quit("ERROR: RedQueue::config(), "
		 "INTERFACE.MEAN_PKTSIZ (%s) must be positive.\n", str);
  }
}

void RedQueue::init()
{
  NicQueue::init();

  if(weight == 0) {
    // FROM ns-2: if weight=0, set it to a reasonable value of
    // 1-exp(-1/C), where C is the link bandwidth in mean packets
    weight = 1.0 - exp(-1.0/(bitrate/8.0/500)); // assuming avg pktsiz=500
  }

  if(qmin == 0) qmin = 0.05*8.0*bufsize;
  if(qmax == 0) qmax = 0.5*8.0*bufsize;
  if(qmin > qmax) {
    error_quit("ERROR: RedQueue::init(), "
	       "qmin %g (b) must be no larger than qmax %g (b).\n", qmin, qmax);
  }
  if(qcap == 0) {
    qcap = bufsize*8.0;
    if(qcap > qmax*2.0) qcap = qmax*2.0;
  }
  if(qmax > qcap) {
    error_quit("ERROR: RedQueue::init(), "
	       "qmax %g (b) must be no larger than qcap %g (b).\n", qmax, qcap);
  }

  printf("[nic=\"%s\", ip=\"%s\"] RedQueue::init(),\n"
	 "  bitrate=%g (bps), latency=%ld (sec), jitter_range=%g,\n"
	 "  bufsize=%ld (B),  weight=%g, qmin=%g (b), qmax=%g (b),\n"
	 "  qcap=%g (b), pmax=%g, wait=%d, mean_pktsiz=%g (b).\n",
	 ((NetworkInterface*)phy_sess->inGraph())->nhi.toString(),
	 IPPrefix::ip2txt(((NetworkInterface*)phy_sess->inGraph())->getIP()),
	 bitrate, latency, jitter_range, bufsize, weight, 
	 qmin, qmax, qcap, pmax, wait_opt, mean_pktsiz);
}

void RedQueue::enqueue(Activation pkt)
{
  // the mac layer header is not counted
  //float len = 8*((ProtocolMessage*)pkt)->totalRealBytes();
  float len = 0;
  //other protocol such as Ethernet, wireless should be added here
  if(phy_sess->getProtocolNumber() == S3FNET_PROTOCOL_TYPE_SIMPLE_PHY)
	len = 8*((SimpleMacMessage*)pkt)->totalRealBytes();
  else
	error_quit("DroptailQueue::enqueue(), pkt has an unknown type\n");

  // update queue size and compute jitter
  ltime_t jitter = calibrate(len);

  if(queue+len > 8.0*bufsize || loss == 1.0 ||
     (loss > 0 && phy_sess->getRandom()->Uniform(0, 1) < loss))
  {
    // if the queue is full, or if the packet is chosen to be dropped
    // according to RED policy, just drop the entire packet
    IFACE_DUMP(printf("[nic=\"%s\", ip=\"%s\"] %s: "
		      "drop packet (queue overflow or RED policy).\n",
		      ((NetworkInterface*)phy_sess->inGraph())->nhi.toString(),
		      IPPrefix::ip2txt(((NetworkInterface*)phy_sess->inGraph())->getIP()),
		      phy_sess->getNowWithThousandSeparator()));

    // reset the count of packet arrivals since the last packet drop
    interdrop = 0;

    return;
  }

  // all we need is to insert this packet into the queue
  queue += len;

  // call the interface to send the packet.
  ltime_t t = (phy_sess->inHost()->d2t(queue/bitrate, 0)+1) + latency + jitter;
  vacate_time = phy_sess->getNow()+t;
  phy_sess->sendPacket(pkt, t);
}

ltime_t RedQueue::calibrate(float len)
{
  ltime_t jitter(0);
  ltime_t now = phy_sess->getNow();

  interdrop += len;

  if(jitter_range > 0) // if jitter is enabled
  {
    double jitter_double = (phy_sess->getRandom()->Uniform(-1, 1)*jitter_range*len)/bitrate;
    jitter = phy_sess->inHost()->d2t(jitter_double, 0)+1;
    now += jitter;
  }

  queue -= bitrate * phy_sess->inHost()->t2d(now-last_update_time, 0);
  if(queue < 0) queue = 0;
  else if(queue > 8.0*bufsize) queue = 8.0*bufsize;
  last_update_time = now;

  int m = 0;
  if(now > vacate_time) // queue is idle?
    m = int(phy_sess->inHost()->t2d(now-vacate_time,0) * bitrate / mean_pktsiz);
  avgque *= pow(1-weight, m+1);
  avgque += weight*queue;

  if(queue == 0 || avgque < qmin)
  {
    crossing = false; // next time is the first time to across the threshold 
    loss = 0; 
  }
  else if(!crossing) // first time crossing the threshold
  {
    crossing = true; // no more the first time
    loss = 0; 
    interdrop = 0;
  }
  else if(avgque < qmax)
  {
    loss = (avgque-qmin)/(qmax-qmin)*pmax;
  }
  else
  {
#ifdef REDMIX_EXPERIMENTAL
    if(avgque < qcap) {
      loss = avgque*(1-pmax)/qmax+(2*pmax-1);
      if(loss > 1) loss = 1;
    } else loss = 1;
#else
    if(avgque < qcap)
      loss = (avgque-qmax)/(qcap-qmax)*(1-pmax)+pmax;
    else loss = 1;
#endif
  }

//#ifdef REDMIX_EXPERIMENTAL
  // make uniform instead of geometric inter-drop periods (ns-2)
  int cnt = int(interdrop/mean_pktsiz);
  if(wait_opt)
  {
    if(cnt*loss < 1.0) loss = 0;
    else if(cnt*loss < 2.0) loss /= (2.0-cnt*loss);
    else loss = 1.0;
  }
  else
  {
    if(cnt*loss < 1.0) loss /= (1.0-cnt*loss);
    else loss = 1.0;
  }
  if(loss < 1.0) loss *= len/mean_pktsiz;
  if(loss > 1.0) loss = 1.0;
//#endif

  return jitter;
}

}; // namespace s3fnet
}; // namespace s3f

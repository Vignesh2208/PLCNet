/**
 * \file simple_phy.cc
 * \brief Source file for the SimplePhy class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "os/simple_phy/simple_phy.h"
#include "util/errhandle.h"
#include "os/base/protocols.h"
#include "net/network_interface.h"
#include "net/host.h"
#include "net/nic_queue.h"

namespace s3f {
namespace s3fnet {

#ifdef SPHY_DEBUG
#define SPHY_DUMP(x) printf("SPHY: "); x
#else
#define SPHY_DUMP(x)
#endif

#define DEFAULT_BUFFER_SIZE INT_MAX/10

S3FNET_REGISTER_PROTOCOL(SimplePhy, SIMPLE_PHY_PROTOCOL_CLASSNAME);

SimplePhy::SimplePhy(ProtocolGraph* graph) :
  LowestProtocolSession(graph), bitrate(0), bufsize(0), latency(0), jitter_range(0), buffer(0)
{
  SPHY_DUMP(printf("[nic=\"%s\"] new simple_phy session.\n", ((NetworkInterface*)inGraph())->nhi.toString()));
}

SimplePhy::~SimplePhy() 
{
  SPHY_DUMP(printf("[nic=\"%s\"] delete simple_phy session.\n", ((NetworkInterface*)inGraph())->nhi.toString()));
  if(buffer) delete buffer;
}

void SimplePhy::config(s3f::dml::Configuration* cfg)
{

  SPHY_DUMP(printf("[nic=\"%s\"] config().\n", ((NetworkInterface*)inGraph())->nhi.toString()));
  LowestProtocolSession::config(cfg);

  // obtain the bandwidth for this nic
  char* bitrate_string = (char*)cfg->findSingle("bitrate");
  if(0 != bitrate_string && !s3f::dml::dmlConfig::isConf(bitrate_string))
  {
    bitrate = atof(bitrate_string);
    if(bitrate < 0)
    {
      error_quit("ERROR: SimplePhy::config(), negative bit rate.\n");
    }
  }
  else error_quit("ERROR: SimplePhy::config(), missing BITRATE attribute.\n");

  // obtain the latency for this nic
  char* latency_string = (char*)cfg->findSingle("latency");
  if(0 != latency_string)
  {
    if(s3f::dml::dmlConfig::isConf(latency_string))
    {
      error_quit("ERROR: SimplyPhy::config(), illegal LATENCY attribute.\n");
    }
    double latency_double = atof(latency_string);
    latency = inHost()->d2t(latency_double, 0);
    if(latency < 0)
    {
      error_quit("ERROR: SimplePhy::config(), negative latency.\n");
    }
  }

  // obtain the jitter range for this nic
  char* jitter_range_string = (char*)cfg->findSingle("jitter_range");
  if(0 != jitter_range_string)
  {
    if(s3f::dml::dmlConfig::isConf(jitter_range_string))
    {
      error_quit("ERROR: SimplyPhy::config(), illegal JITTER_RANGE attribute.\n");
    }
    jitter_range = (float)atof(jitter_range_string);
    if(jitter_range < 0 || jitter_range > 1)
    {
      error_quit("ERROR: SimplePhy::config(), illegal jitter range(%f), should be [0, 1]\n", jitter_range);
    }
  } 

  // obtain the buffer size
  char* buffer_string = (char*)cfg->findSingle("buffer");
  if(0 != buffer_string)
  {
    if(s3f::dml::dmlConfig::isConf(buffer_string))
    {
      error_quit("ERROR: SimplyPhy::config(), illegal BUFFER attribute.\n");
    }
    bufsize = atol(buffer_string);
    if(bufsize < 0)
    {
      error_quit("ERROR: SimplePhy::config(), negative buffer size (%s).\n", buffer_string);
    }
  }
  else
  {
	    bufsize = DEFAULT_BUFFER_SIZE;
  }

  SPHY_DUMP(printf("[nic=\"%s\"] config(): bitrate=%lf, latency=%ld, jitter_range=%f, buffer(size)=%ld.\n",
		   ((NetworkInterface*)inGraph())->nhi.toString(),
		   bitrate, latency, jitter_range, bufsize));

  // obtain the queue type and create the corresponding queue. If the
  // queue has not been indicated, we will use the default mixed
  // queue.
  char* default_queue = "droptail" /*"mixfluid"*/;
  char* queue_string = (char*)cfg->findSingle("queue");
  if(!queue_string) queue_string = default_queue;
  if(s3f::dml::dmlConfig::isConf(queue_string))
    error_quit("ERROR: SimplePhy::config(), illegal QUEUE attribute.\n");
  
  SPHY_DUMP(printf("[nic=\"%s\"] config(): queue is \"%s\".\n",
		   ((NetworkInterface*)inGraph())->nhi.toString(), queue_string));

  // create a new queue based on the type of the queue
  if(!(buffer = NicQueues::newNicQueueInstance(queue_string, this)))
    error_quit("ERROR: SimplePhy::config(), unable to create nic queue: \"%s\"!\n", queue_string);
  buffer->config(cfg);
}

void SimplePhy::init()
{
  SPHY_DUMP(printf("[nic=\"%s\", ip=\"%s\"] init().\n",
		   ((NetworkInterface*)inGraph())->nhi.toString(),
		   IPPrefix::ip2txt(getNetworkInterface()->getIP())));

  LowestProtocolSession::init();

  if(strcasecmp(name, PHY_PROTOCOL_NAME))
    error_quit("ERROR: SimplePhy::init(), unmatched protocol name: \"%s\", expecting \"PHY\".\n", name);
  
  // initialize the buffer itself
  buffer->init();
}

int SimplePhy::push(Activation pkt, ProtocolSession* hi_sess, void* extinfo, size_t extinfo_size)
{
  SPHY_DUMP(printf("[nic=\"%s\", ip=\"%s\"] %s: push(), enqueue packet.\n",
		   ((NetworkInterface*)inGraph())->nhi.toString(),
		   IPPrefix::ip2txt(getNetworkInterface()->getIP()), getNowWithThousandSeparator()));
  assert(pkt);
  assert(buffer);

  buffer->enqueue(pkt);

  return 0;
}

void SimplePhy::sendPacket(Activation pkt, ltime_t delay)
{
  SPHY_DUMP(printf("[nic=\"%s\", ip=\"%s\"] %s: sendPacket(delay=%ld).\n",
		   ((NetworkInterface*)inGraph())->nhi.toString(),
	       IPPrefix::ip2txt(getNetworkInterface()->getIP()), getNowWithThousandSeparator(), delay));

  getNetworkInterface()->sendPacket(pkt, delay);
}

void SimplePhy::receivePacket(Activation pkt)
{
  SPHY_DUMP(printf("[nic=\"%s\", ip=\"%s\"] %s: receivePacket().\n",
		   ((NetworkInterface*)inGraph())->nhi.toString(),
		   IPPrefix::ip2txt(getNetworkInterface()->getIP()), getNowWithThousandSeparator()));

  if(!parent_prot)
  {
    error_quit("ERROR: SimplePhy()::receivePacket(), no parent protocol session set.\n");
  }
  parent_prot->popup(pkt, this);

}

int SimplePhy::control(int ctrltyp, void* ctrlmsg, ProtocolSession* sess)
{
  SPHY_DUMP(printf("[nic=\"%s\"] control(): type=%d.\n", 
		   ((NetworkInterface*)inGraph())->nhi.toString(), ctrltyp));

  switch(ctrltyp) {
  case PHY_CTRL_GET_BITRATE: 
    assert(ctrlmsg);
    *((double*)ctrlmsg) = bitrate;
    return 0;

  case PHY_CTRL_GET_BUFFER_SIZE: 
    assert(ctrlmsg);
    *((long*)ctrlmsg) = bufsize;
    return 0;

  case PHY_CTRL_GET_JITTER_RANGE:
    assert(ctrlmsg);
    *((float*)ctrlmsg) = jitter_range;
    return 0;

  case PHY_CTRL_GET_LATENCY:
    assert(ctrlmsg);
    *((ltime_t*)ctrlmsg) = latency;
    return 0; 

  case PHY_CTRL_GET_QUEUE:
    assert(ctrlmsg);
    *((NicQueue**)ctrlmsg) = buffer;
    return 0;

  default:
    return LowestProtocolSession::control(ctrltyp, ctrlmsg, sess);
  }
}

}; // namespace s3fnet
}; // namespace s3f

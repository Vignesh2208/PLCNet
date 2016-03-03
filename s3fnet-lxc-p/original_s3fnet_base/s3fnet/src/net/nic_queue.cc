/**
 * \file nic_queue.cc
 * \brief Source file for the NicQueue class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "net/nic_queue.h"
#include "os/base/lowest_protocol_session.h"
#include "s3fnet.h"

namespace s3f {
namespace s3fnet {

S3FNET_POINTER_VECTOR* NicQueues::registered_queues = 0;

NicQueue::NicQueue(LowestProtocolSession* lowsess) : 
  phy_sess(lowsess), bitrate(0), bufsize(0), 
  latency(0), jitter_range(0)
{}

NicQueue::~NicQueue() {}

void NicQueue::config(s3f::dml::Configuration* cfg) {}

void NicQueue::init()
{
  phy_sess->control(PHY_CTRL_GET_BITRATE, (void*)&bitrate, 0);
  phy_sess->control(PHY_CTRL_GET_BUFFER_SIZE, (void*)&bufsize, 0);  
  phy_sess->control(PHY_CTRL_GET_LATENCY, (void*)&latency, 0);
  phy_sess->control(PHY_CTRL_GET_JITTER_RANGE, (void*)&jitter_range, 0);
}

int NicQueues::registerNicQueue(NicQueue* (*fct)(char*, LowestProtocolSession*))
{
  // allocate the global data structure if it's not there
  if(registered_queues == 0)
  {
    registered_queues = new S3FNET_POINTER_VECTOR;
    assert(registered_queues);
    registered_queues->reserve(S3FNET_INIT_NIC_QUEUES);
  }

  // add the queue constructor (WE DON'T HAVE DUPLICATION CHECK!)
  registered_queues->push_back((void*)fct);
  return 0;
}

NicQueue* NicQueues::newNicQueueInstance(char* queuename, LowestProtocolSession* _nic)
{
  if(!registered_queues) return 0;
  for(S3FNET_POINTER_VECTOR::iterator iter = registered_queues->begin();
      iter != registered_queues->end(); iter++)
  {
    void* fct = *iter;
    NicQueue* queue = (*(NicQueue*(*)(char*, LowestProtocolSession*))fct)(queuename, _nic);
    if(queue) return queue;
  }

  return 0;
}

}; // namespace s3fnet
}; // namespace s3f

/**
 * \file link.cc
 * \brief Source file for the Link class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "net/link.h"
#include "net/net.h"
#include "net/host.h"
#include "util/errhandle.h"
#include "net/network_interface.h"


namespace s3f {
namespace s3fnet {

#ifdef LINK_DEBUG
#define LINK_DUMP(x) printf("LINK: "); x
#else
#define LINK_DUMP(x)
#endif

Link::Link(Net* theNet) : DmlObject((DmlObject*)theNet), delay(0), owner_net(theNet)
{
  LINK_DUMP(printf("new link.\n"););
}

Link::~Link() 
{
  LINK_DUMP(printf("delete link.\n"););
}

void Link::config(s3f::dml::Configuration* cfg)
{
  LINK_DUMP(printf("config().\n"););

  char* str = (char*)cfg->findSingle("min_delay");
  if(!str || s3f::dml::dmlConfig::isConf(str))
    error_quit("ERROR: Link::config(), missing or invalid link.min_delay attribute.\n");
  min_delay = atof(str);
  if(min_delay < 0)
    error_quit("ERROR: Link::config(), link.min_delay cannot be negative.");

  str = (char*)cfg->findSingle("prop_delay");
  if(!str || s3f::dml::dmlConfig::isConf(str))
    error_quit("ERROR: Link::config(), missing or invalid link.prop_delay attribute.\n");
  prop_delay = atof(str);
  if(prop_delay < 0)
    error_quit("ERROR: Link::config(), link.prop_delay cannot be negative.");

  delay = prop_delay + min_delay;
  LINK_DUMP(printf("config(): min_delay= %f, prop_delay = %f, mapping_delay = %f.\n", min_delay, prop_delay, delay));

  s3f::dml::Enumeration* aenum = cfg->find("attach");
  S3FNET_STRING pnhi = myParent->nhi.toStlString();
  LINK_DUMP(printf("config(): attach\n"));
  while(aenum->hasMoreElements())
  {
    str = (char*)aenum->nextElement();
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: Link::config(), invalid LINK.ATTACH attribute.\n");
    S3FNET_STRING nhi = myParent->myParent ? (pnhi + ":" + str) : str;
    LINK_DUMP(printf("\t %s \n", nhi.c_str()));

    //add all attached network_interfaces
    NetworkInterface* iface = (NetworkInterface*)(owner_net->nicnhi_to_obj(nhi));
    connected_nw_iface_vec.push_back(iface);
  }
  delete aenum;

}

int Link::connect(Net* pNet)
{
  LINK_DUMP(printf("connect().\n"));

  /* the default switched Ethernet type connection, size of connected_nw_iface_vec is always 2
     which is not true for CSMA type connection */
  if(connected_nw_iface_vec.size()!=2)
  {
	  LINK_DUMP(printf("connected_nw_iface_vec has size %d.\n", (int)connected_nw_iface_vec.size()));
	  error_quit("ERROR: Link::connect(), invalid size of connected_nw_iface_vec vector.\n");
  }

  for (unsigned i=0; i<connected_nw_iface_vec.size(); i++)
  {
    // an expensive way to make sure the nhi is of interface type
    /*Nhi nhi;
    char* suffix = sstrdup(nhi_vec[i].c_str());
    if(nhi.convert(suffix, Nhi::NHI_INTERFACE))
      error_quit("ERROR: Link::connect(), invalid nhi address: %s.\n", nhi_vec[i].c_str());
    delete[] suffix;
   */

	    //create an OutChannel per network interface
	    NetworkInterface* iface = connected_nw_iface_vec[i];
	    assert(iface);
	    Host* host = (Host*)iface->myParent;
	    assert(host);

	    ltime_t iface_latency;
	    LowestProtocolSession* phy_sess = (LowestProtocolSession*)iface->getLowestProtocolSession();
	    phy_sess->control(PHY_CTRL_GET_LATENCY, (void*)&iface_latency, 0);

	    iface->oc = new OutChannel(host, iface_latency);
	    iface->attach_to_link(this);
  }

  //map InChannel and OutChannel
  /* use link delay in dml as mapping delay, it can be equal to min_packet_size/max_bandwidth + propagation delay,
  may need to compute here instead of getting directly from dml */
  NetworkInterface* iface1 = connected_nw_iface_vec[0];
  NetworkInterface* iface2 = connected_nw_iface_vec[1];
  iface1->oc->mapto(iface2->ic, iface1->getHost()->d2t(delay, 0));
  iface2->oc->mapto(iface1->ic, iface2->getHost()->d2t(delay, 0));
  iface1->is_oc_connected = true;
  iface1->is_ic_connected = true;
  iface2->is_oc_connected = true;
  iface2->is_ic_connected = true;

  return 0;
}

void Link::init()
{
  LINK_DUMP(printf("init().\n"));
}

void Link::display(int indent)
{
  printf("%*cLink [", indent,' ');
  for (unsigned int i=0; i < connected_nw_iface_vec.size(); i++)
  {
    printf("%s ", connected_nw_iface_vec[i]->nhi.toStlString().c_str());
  }
  printf(" ]\n");
}

Link* Link::createLink(Net* theNet, s3f::dml::Configuration* cfg)
{
  Link* link = new Link(theNet);
  link->config(cfg);
    
  return link;
}

int Link::getNumOfNetworkInterface()
{
	return (int)connected_nw_iface_vec.size();
}

}; // namespace s3fnet
}; // namespace s3f

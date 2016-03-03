/**
 * \file tcp_master.cc
 * \brief Source file for the TCPMaster class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "os/tcp/tcp_master.h"
#include "util/errhandle.h"
#include "net/host.h"
#include "os/base/protocols.h"
#include "os/tcp/tcp_message.h"
#include "os/tcp/tcp_session.h"
#include "os/ipv4/ip_message.h"
#include "os/ipv4/ip_interface.h"

#ifdef TCP_DEBUG
#define TCP_DUMP(x) printf("TCP: "); x
#else
#define TCP_DUMP(x)
#endif

namespace s3f {
namespace s3fnet {

S3FNET_REGISTER_PROTOCOL_WITH_ALIAS(TCPMaster, "S3F.OS.TCP", "S3F.OS.TCP.tcpSessionMaster");

// convert nhi ':' to '.' so that it can be part of a legitimate
// file name.
static char* legit_nhi(char* nhistr)
{
  if(nhistr)
  {
    char* ret = nhistr;
    while(*nhistr)
    {
      if(*nhistr == ':') *nhistr = '.';
      nhistr++;
    }
    return ret;
  } else return 0;
}

#define calc_slow_timeout(x) \
  (slow_timeout_double+slow_timeout_double*((x+boot_time_double)/slow_timeout_double) \
   -x-boot_time_double)

#define calc_fast_timeout(x) \
  (fast_timeout_double+fast_timeout_double*((x+boot_time_double)/fast_timeout_double) \
   -x-boot_time_double)

TCPMaster::TCPMaster(ProtocolGraph* graph) :
		SessionMaster(graph), ip_sess(0), slow_timer_callback_proc(0), fast_timer_callback_proc(0), slow_timer_ac(0)
{
  TCP_DUMP(printf("[host=\"%s\"] new tcp master session.\n", inHost()->nhi.toString()));

  tcp_version    		= TCP_DEFAULT_VERSION;
  iss            		= TCP_DEFAULT_ISS;
  mss           		= TCP_DEFAULT_MSS;
  rcv_wnd_size   		= TCP_DEFAULT_RCVWNDSIZ;
  snd_wnd_size  		= TCP_DEFAULT_SNDWNDSIZ;
  snd_buf_size  		= TCP_DEFAULT_SNDBUFSIZ;
  max_rxmit      		= TCP_DEFAULT_MAXRXMIT;
  slow_timeout_double   = TCP_DEFAULT_SLOW_TIMEOUT;
  slow_timeout 			= inHost()->d2t(slow_timeout_double, 0);
  fast_timeout_double   = TCP_DEFAULT_FAST_TIMEOUT;
  fast_timeout 			= inHost()->d2t(fast_timeout_double, 0);
  idle_timeout_double   = TCP_DEFAULT_IDLE_TIMEOUT;
  idle_timeout 			= inHost()->d2t(idle_timeout_double, 0);
  msl_double            = TCP_DEFAULT_MSL;
  msl		 			= inHost()->d2t(msl_double, 0);
  delayed_ack   		= TCP_DEFAULT_DELAYEDACK;
  max_cong_wnd   		= TCP_DEFAULT_MAXCONWND;

  boot_time_window_double = 0;
  boot_time_window = 0;
}

TCPMaster::~TCPMaster()
{
  // reclaim defunct_sessions first
  delete_defunct_sessions();

  // delete all listening sessions
  for(S3FNET_SET(TCPSession*)::iterator l_iter = listening_sessions.begin();
      l_iter != listening_sessions.end(); l_iter++)
    delete (*l_iter);
  listening_sessions.clear();

  // delete all connected sessions
  for(S3FNET_SET(TCPSession*)::iterator c_iter = connected_sessions.begin();
      c_iter != connected_sessions.end(); c_iter++)
    delete (*c_iter);
  connected_sessions.clear();

  // delete all idle sessions
  for(S3FNET_SET(TCPSession*)::iterator i_iter = idle_sessions.begin();
      i_iter != idle_sessions.end(); i_iter++)
    delete (*i_iter);
  idle_sessions.clear();

  // delete the list kept for fast timeout sessions
  fast_timeout_sessions.clear();
}

void TCPMaster::config(s3f::dml::Configuration* cfg)
{
  SessionMaster::config(cfg);

  s3f::dml::Configuration* init_cfg = (s3f::dml::Configuration*)cfg->findSingle(TCP_DML_TCPINIT);
  if(init_cfg)
  {
    if(!s3f::dml::dmlConfig::isConf(init_cfg))
      error_quit("ERROR: TCPMaster::config(), invalid %s attribute.\n", TCP_DML_TCPINIT);

    char* version_string = (char*)init_cfg->findSingle(TCP_DML_VERSION);
    if(version_string)
    {
      if(s3f::dml::dmlConfig::isConf(version_string))
    	error_quit("ERROR: TCPMaster::config(), invalid %s:%s attribute.\n",
    			TCP_DML_TCPINIT, TCP_DML_VERSION);
      if(!strcasecmp(version_string, "tahoe"))
      {
    	tcp_version = TCP_VERSION_TAHOE;
      }
      else if(!strcasecmp(version_string, "reno"))
      {
    	tcp_version = TCP_VERSION_RENO;
      }
      else if(!strcasecmp(version_string, "new_reno"))
      {
    	tcp_version = TCP_VERSION_NEW_RENO;
      }
      else if(!strcasecmp(version_string, "sack"))
      {
    	tcp_version = TCP_VERSION_SACK;
      }
      else
      {
    	error_quit("ERROR: TCPMaster::config(), unknown TCP version: %s\n.", version_string);
      }
    }
    else // for backward compatibility
    {
      char* fast_recovery_string = (char*)init_cfg->findSingle(TCP_DML_FAST_RECOVERY);
      if(fast_recovery_string)
      {
    	if(s3f::dml::dmlConfig::isConf(fast_recovery_string))
    	  error_quit("ERROR: TCPMaster::config(), invalid %s:%s attribute.\n",
    			  TCP_DML_TCPINIT, TCP_DML_FAST_RECOVERY);
    	if(!strcasecmp(fast_recovery_string, "true"))
    	{
    	  tcp_version = TCP_VERSION_RENO;
    	}
    	else if(!strcasecmp(fast_recovery_string, "false"))
    	{
    	  tcp_version = TCP_VERSION_TAHOE;
    	}
    	else
    	{
    	  error_quit("ERROR: TCPMaster::config(), %s.%s (%s) must be either true or false.\n",
		     TCP_DML_TCPINIT, TCP_DML_FAST_RECOVERY, fast_recovery_string);
    	}
      }
    }

    char* iss_string = (char*)init_cfg->findSingle(TCP_DML_ISS);
    if(iss_string)
    {
      if(s3f::dml::dmlConfig::isConf(iss_string))
    	error_quit("ERROR: TCPMaster::config(), invalid %s:%s attribute.\n",
    			TCP_DML_TCPINIT, TCP_DML_ISS);
      iss = atoi(iss_string);
      if(iss < 0)
      {
    	error_quit("ERROR: TCPMaster::config(), %s.%s is negative: %s.\n",
    			TCP_DML_TCPINIT, TCP_DML_ISS, iss_string);
      }
    }

    char* mss_string = (char*)init_cfg->findSingle(TCP_DML_MSS);
    if(mss_string)
    {
      if(s3f::dml::dmlConfig::isConf(mss_string))
    	error_quit("ERROR: TCPMaster::config(), invalid %s:%s attribute.\n", TCP_DML_TCPINIT, TCP_DML_MSS);
      mss = atoi(mss_string);
      if(mss < 0 || mss > 65536-S3FNET_TCPHDR_LENGTH-S3FNET_IPHDR_LENGTH)
      {
    	error_quit("ERROR: TCPMaster::config(), %s.%s (%s) must be in [0, %d].\n",
		   TCP_DML_TCPINIT, TCP_DML_MSS, mss_string, 65536-S3FNET_TCPHDR_LENGTH-S3FNET_IPHDR_LENGTH);
      }
    }

    char* rwnd_string = (char*)init_cfg->findSingle(TCP_DML_RCVWNDSIZ);
    if(rwnd_string)
    {
      if(s3f::dml::dmlConfig::isConf(rwnd_string))
    	  error_quit("ERROR: TCPMaster::config(), invalid %s:%s attribute.\n",
    			  TCP_DML_TCPINIT, TCP_DML_RCVWNDSIZ);
      rcv_wnd_size = atoi(rwnd_string);
      if(rcv_wnd_size < 0)
      {
    	error_quit("ERROR: TCPMaster::config(), %s.%s is negative: %s.\n",
    			TCP_DML_TCPINIT, TCP_DML_RCVWNDSIZ, rwnd_string);
      } 
    }

    char* swnd_string = (char*)init_cfg->findSingle(TCP_DML_SNDWNDSIZ);
    if(swnd_string)
    {
      if(s3f::dml::dmlConfig::isConf(swnd_string))
    	error_quit("ERROR: TCPMaster::config(), invalid %s:%s attribute.\n",
    			TCP_DML_TCPINIT, TCP_DML_SNDWNDSIZ);
      snd_wnd_size = atoi(swnd_string);
      if(snd_wnd_size < 0)
      {
    	  error_quit("ERROR: TCPMaster::config(), %s.%s is negative: %s.\n",
    			  TCP_DML_TCPINIT, TCP_DML_SNDWNDSIZ, swnd_string);
      }
    }

    char* sbuf_string = (char*)init_cfg->findSingle(TCP_DML_SNDBUFSIZ);
    if(sbuf_string)
    {
      if(s3f::dml::dmlConfig::isConf(sbuf_string))
    	  error_quit("ERROR: TCPMaster::config(), invalid %s:%s attribute.\n",
		   TCP_DML_TCPINIT, TCP_DML_SNDBUFSIZ);
      snd_buf_size = atoi(sbuf_string);
      if(snd_buf_size < 0)
      {
    	error_quit("ERROR: TCPMaster::config(), %s.%s is negative: %s.\n",
    			TCP_DML_TCPINIT, TCP_DML_SNDBUFSIZ, sbuf_string);
      }
    }
    
    char* rxmit_string = (char*)init_cfg->findSingle(TCP_DML_MAXRXMIT);
    if(rxmit_string)
    {
      if(s3f::dml::dmlConfig::isConf(rxmit_string))
    	error_quit("ERROR: TCPMaster::config(), invalid %s:%s attribute.\n",
    			TCP_DML_TCPINIT, TCP_DML_MAXRXMIT);
      max_rxmit = atoi(rxmit_string);
      if(max_rxmit < 0 || max_rxmit > 31)
      {
    	  error_quit("ERROR: TCPMaster::config(), %s.%s (%s) must be in [0, 31].\n",
    			  TCP_DML_TCPINIT, TCP_DML_MAXRXMIT, rxmit_string);
      }
    }

    char* slow_timeout_string = (char*)init_cfg->findSingle(TCP_DML_SLOW_TIMEOUT);
    if(slow_timeout_string)
    {
      if(s3f::dml::dmlConfig::isConf(slow_timeout_string))
    	  error_quit("ERROR: TCPMaster::config(), invalid %s:%s attribute.\n",
		   TCP_DML_TCPINIT, TCP_DML_SLOW_TIMEOUT);
      slow_timeout_double = atof(slow_timeout_string);
      if(slow_timeout_double < 0)
      {
    	  error_quit("ERROR: TCPMaster::config(), %s.%s is negative: %s.\n",
    			  TCP_DML_TCPINIT, TCP_DML_SLOW_TIMEOUT, slow_timeout_string);
      }
      slow_timeout = inHost()->d2t(slow_timeout_double, 0);
    }

    char* fast_timeout_string = (char*)init_cfg->findSingle(TCP_DML_FAST_TIMEOUT);
    if(fast_timeout_string)
    {
      if(s3f::dml::dmlConfig::isConf(fast_timeout_string))
    	  error_quit("ERROR: TCPMaster::config(), invalid %s:%s attribute.\n",
		   TCP_DML_TCPINIT, TCP_DML_FAST_TIMEOUT);
      fast_timeout_double = atof(fast_timeout_string);
      if(fast_timeout_double < 0)
      {
    	error_quit("ERROR: TCPMaster::config(), %s.%s is negative: %s.\n",
    			TCP_DML_TCPINIT, TCP_DML_FAST_TIMEOUT, fast_timeout_string);
      }
      fast_timeout = inHost()->d2t(fast_timeout_double, 0);
    }

    char* idle_timeout_string = (char*)init_cfg->findSingle(TCP_DML_IDLE_TIMEOUT);
    if(idle_timeout_string)
    {
      if(s3f::dml::dmlConfig::isConf(idle_timeout_string))
    	  error_quit("ERROR: TCPMaster::config(), invalid %s:%s attribute.\n",
    			  TCP_DML_TCPINIT, TCP_DML_IDLE_TIMEOUT);
      idle_timeout_double = atof(idle_timeout_string);
      if(idle_timeout_double < 0)
      {
    	error_quit("ERROR: TCPMaster::config(), %s.%s is negative: %s.\n",
		   TCP_DML_TCPINIT, TCP_DML_IDLE_TIMEOUT, idle_timeout_string);
      }
      idle_timeout = inHost()->d2t(idle_timeout_double, 0);
    }

    char* msl_string = (char*)init_cfg->findSingle(TCP_DML_MSL);
    if(msl_string)
    {
      if(s3f::dml::dmlConfig::isConf(msl_string))
    	error_quit("ERROR: TCPMaster::config(), invalid %s:%s attribute.\n", TCP_DML_TCPINIT, TCP_DML_MSL);
      msl_double = atof(msl_string);
      if(msl_double < 0)
      {
    	error_quit("ERROR: TCPMaster::config(), %s.%s is negative: %s.\n",
		   TCP_DML_TCPINIT, TCP_DML_MSL, msl_string);
      }
      msl = inHost()->d2t(msl_double, 0);
    }

    char* delayed_ack_string = (char*)init_cfg->findSingle(TCP_DML_DELAYEDACK);
    if(delayed_ack_string)
    {
      if(s3f::dml::dmlConfig::isConf(delayed_ack_string))
    	  error_quit("ERROR: TCPMaster::config(), invalid %s:%s attribute.\n",
    			  TCP_DML_TCPINIT, TCP_DML_DELAYEDACK);
      if(!strcasecmp(delayed_ack_string, "true")) delayed_ack = true;
      else if(!strcasecmp(delayed_ack_string, "false")) delayed_ack = false;
      else
      {
    	error_quit("ERROR: TCPMaster::config(), %s.%s (%s) must be either true or false.\n",
		   TCP_DML_TCPINIT, TCP_DML_DELAYEDACK, delayed_ack_string);
      }
    }

    char* max_conwnd_string = (char*)init_cfg->findSingle(TCP_DML_MAXCONWND);
    if(max_conwnd_string)
    {
      if(s3f::dml::dmlConfig::isConf(max_conwnd_string))
    	error_quit("ERROR: TCPMaster::config(), invalid %s:%s attribute.\n",
    			TCP_DML_TCPINIT, TCP_DML_MAXCONWND);
      max_cong_wnd = atoi(max_conwnd_string);
      if(max_cong_wnd < 1)
      {
    	error_quit("ERROR: TCPMaster::config(), %s.%s (%s) should be at least one.\n",
    			TCP_DML_TCPINIT, TCP_DML_MAXCONWND, max_conwnd_string);
      }
    }
  } //end init_cfg

  // send window size must be no larger than the sender buffer size
  if(snd_wnd_size > snd_buf_size)
  {
    error_quit("ERROR: TCPMaster::config(), %s (%u) must be no larger than %s (%u).\n",
	       TCP_DML_SNDWNDSIZ, snd_wnd_size, TCP_DML_SNDBUFSIZ, snd_buf_size);
  }

  char* boot_time_string = (char*)cfg->findSingle(TCP_DML_BOOT_TIME);
  if(boot_time_string)
  {
    if(s3f::dml::dmlConfig::isConf(boot_time_string))
      error_quit("ERROR: TCPMaster::config(), invalid %s attribute.\n", TCP_DML_BOOT_TIME);
    boot_time_window_double = atof(boot_time_string);
    if(boot_time_window_double < 0)
    {
      error_quit("ERROR: TCPMaster::config(), %s is negative: %ld.\n", TCP_DML_BOOT_TIME, boot_time_string);
    }
    boot_time_window = inHost()->d2t(boot_time_window_double, 0);
  }

  TCP_DUMP(printf("[host=\"%s\"] config():\n"
		  "  ver=%d, iss=%u, mss=%u, rwnd=%u, swnd=%u, sbuf=%u,\n"
		  "  max_rxmit=%u, slow_timeout=%ld, fast_timeout=%ld, idle_timeout=%ld,\n"
		  "  msl=%ld, delayed_ack=%d, max_cwnd=%u, boot_time_window=%ld.\n",
		  inHost()->nhi.toString(),
		  tcp_version, iss, mss, rcv_wnd_size, snd_wnd_size, snd_buf_size,
		  max_rxmit, slow_timeout, fast_timeout, idle_timeout,
		  msl, delayed_ack, max_cong_wnd, boot_time_window));

  rcv_wnd_size *= mss;
  snd_wnd_size *= mss;
  snd_buf_size *= mss;
  max_cong_wnd *= mss;
}

void TCPMaster::init()
{ 
  TCP_DUMP(printf("[host=\"%s\"] init().\n", inHost()->nhi.toString()));

  SessionMaster::init();

  if(strcasecmp(name, TCP_PROTOCOL_NAME))
    error_quit("ERROR: TCPMaster::init(), unmatched protocol name: \"%s\", expecting \"TCP\".\n", name);

  if(boot_time_window > 0)
  {
    boot_time_double = getRandom()->Uniform(0,1)*boot_time_window_double;
    boot_time = inHost()->d2t(boot_time_double, 0);
  } else boot_time = 0;

  // find out the socket layer protocol session
  sock_master = (SessionMaster*)inHost()->sessionForName(SOCKET_PROTOCOL_NAME);
  if(!sock_master) 
    error_quit("ERROR: TCPMaster::init(), missing socket layer.\n"); 

  // find out the IP layer protocol session
  ip_sess = inHost()->getNetworkLayerProtocol();
  assert(ip_sess);

  // create the slow timer
  Host* owner_host = inHost();
  slow_timer_callback_proc = new Process( (Entity *)owner_host,
		  (void (s3f::Entity::*)(s3f::Activation))&TCPMaster::slow_timer_callback);
  // start the slow timer
  slow_timer_ac = new ProtocolCallbackActivation(this);
  Activation ac (slow_timer_ac);
  double t = calc_slow_timeout(0);
  HandleCode h = owner_host->waitFor( slow_timer_callback_proc, ac, inHost()->d2t(t, 0), owner_host->tie_breaking_seed);
}

int TCPMaster::push(Activation msg, ProtocolSession* hi_sess, void* extinfo, size_t extinfo_size)
{
  TCP_DUMP(printf("[host=\"%s\"] %s: push().\n", inHost()->nhi.toString(), getNowWithThousandSeparator()));

  return ip_sess->pushdown(msg, this, extinfo, extinfo_size);
}

int TCPMaster::pop(Activation msg, ProtocolSession* lo_sess, void* extinfo, size_t extinfo_size)
{
  // to be safe, delete all defunct sessions first
  delete_defunct_sessions();
  
  IPOptionToAbove* ops  = (IPOptionToAbove*)extinfo; 
  TCPMessage* tcphdr = (TCPMessage*)msg;

  // check each connected tcp session
  S3FNET_SET(TCPSession*)::iterator iter;
  for(iter = connected_sessions.begin(); iter != connected_sessions.end(); iter++)
  {
    if((*iter)->dst_ip   == ops->src_ip &&
       (*iter)->src_port == tcphdr->dst_port &&
       (*iter)->dst_port == tcphdr->src_port)
    {

      TCP_DUMP(char buf1[50]; char buf2[50];
	       printf("[host=\"%s\"] %s: pop(), found matching socket "
		      "(src=\"%s:%d\", dst=\"%s:%d\").\n",
		      inHost()->nhi.toString(), getNowWithThousandSeparator(),
		      IPPrefix::ip2txt((*iter)->src_ip, buf1), (*iter)->src_port,
		      IPPrefix::ip2txt((*iter)->dst_ip, buf2), (*iter)->dst_port));
      (*iter)->receive(tcphdr);

      return 0;
    }
  }

  // check each listening tcp session
  for(iter = listening_sessions.begin(); iter != listening_sessions.end(); iter++)
  {
    if((*iter)->src_port == tcphdr->dst_port)
    {
      // if the port is a listening port, this tcp session will turn
      // into a connected tcp session and at the same time, the upper
      // layer will trigger another listening tcp session.
      (*iter)->src_ip   = ops->dst_ip;
      (*iter)->dst_ip   = ops->src_ip;
      (*iter)->dst_port = tcphdr->src_port;

      TCP_DUMP(char buf1[50]; char buf2[50];
	       printf("[host=\"%s\"] %s: pop(), found listening socket "
		      "(src=\"%s:%d\", dst=\"%s:%d\").\n",
		      inHost()->nhi.toString(), getNowWithThousandSeparator(),
		      IPPrefix::ip2txt((*iter)->src_ip, buf1), (*iter)->src_port,
		      IPPrefix::ip2txt((*iter)->dst_ip, buf2), (*iter)->dst_port));
      (*iter)->receive(tcphdr);

      return 0;
    }
  }

  return 0;
}

SocketSession* TCPMaster::createSession(int sock)
{
  TCP_DUMP(printf("[host=\"%s\"] %s: createSession(sock=%d).\n",
		  inHost()->nhi.toString(), getNowWithThousandSeparator(), sock));

  TCPSession* session = new TCPSession(this, sock);
  assert(session);
  idle_sessions.insert(session);
  return session;
}

void TCPMaster::deleteSession(TCPSession* session)
{
  TCP_DUMP(printf("deleteSession(sock=%d).\n", session->socket));

  assert(session);
  separate_session(session);

  defunct_sessions.insert(session);
}

void TCPMaster::separate_session(TCPSession* session)
{
  S3FNET_SET(TCPSession*)::iterator iter;

  if((iter = listening_sessions.find(session)) != listening_sessions.end())
    listening_sessions.erase(iter);

  else if((iter = connected_sessions.find(session)) != connected_sessions.end())
    connected_sessions.erase(iter);

  else if((iter = idle_sessions.find(session)) != idle_sessions.end())
    idle_sessions.erase(iter);

  if((iter = fast_timeout_sessions.find(session)) != fast_timeout_sessions.end())
    fast_timeout_sessions.erase(iter);
}

void TCPMaster::delete_defunct_sessions()
{
  for(S3FNET_SET(TCPSession*)::iterator iter = defunct_sessions.begin();
      iter != defunct_sessions.end(); iter++)
  {
    delete (*iter);
  }
  defunct_sessions.clear();
}

void TCPMaster::setConnected(TCPSession* session)
{ 
  TCP_DUMP(printf("[host=\"%s\"] %s: setConnected(sock=%d).\n",
		  inHost()->nhi.toString(), getNowWithThousandSeparator(), session->socket));

  separate_session(session);
  connected_sessions.insert(session);
}

void TCPMaster::setListening(TCPSession* session)
{
  TCP_DUMP(printf("[host=\"%s\"] %s: setListening(sock=%d).\n",
		  inHost()->nhi.toString(), getNowWithThousandSeparator(), session->socket));

  separate_session(session);
  listening_sessions.insert(session);
}

void TCPMaster::setIdle(TCPSession* session)
{
  TCP_DUMP(printf("[host=\"%s\"] %s: setIdle(sock=%d).\n",
		  inHost()->nhi.toString(), getNowWithThousandSeparator(), session->socket));

  separate_session(session);
  idle_sessions.insert(session);
}

void TCPMaster::enableFastTimeout(TCPSession* session)
{
  assert(session);
  if(!fast_timer_callback_proc)
  {
    // only need to schedule the time at the first time
    // create the fast timer
    Host* owner_host = inHost();
    fast_timer_callback_proc = new Process( (Entity *)owner_host,
  		  (void (s3f::Entity::*)(s3f::Activation))&TCPMaster::fast_timer_callback);
    // start the fast timer
    fast_timer_ac = new ProtocolCallbackActivation(this);
    Activation ac (fast_timer_ac);
    double time = inHost()->t2d(getNow(), 0);
    double t = calc_fast_timeout(time);
    HandleCode h = owner_host->waitFor( fast_timer_callback_proc, ac, inHost()->d2t(t, 0), owner_host->tie_breaking_seed);
  }
  fast_timeout_sessions.insert(session);
}

void TCPMaster::disableFastTimeout(TCPSession* session)
{
  assert(session);
  fast_timeout_sessions.erase(session);
}

void TCPMaster::fast_timer_callback(Activation ac)
{
  TCPMaster* tm = (TCPMaster*)((ProtocolCallbackActivation*)ac)->session;

  tm->delete_defunct_sessions();

  // go through each tcp session that requires fast timer; to prevent
  // changes during iteration, we copy the list to an array.
  int mysize;
  if((mysize = tm->fast_timeout_sessions.size()) > 0)
  {
    TCPSession** mylist  = new TCPSession*[mysize];
    assert(mylist);
    int idx = 0;
    for(S3FNET_SET(TCPSession*)::iterator iter = tm->fast_timeout_sessions.begin();
	iter != tm->fast_timeout_sessions.end(); iter++, idx++)
      mylist[idx] = (*iter);
    for(idx = 0; idx < mysize; idx++)
      mylist[idx]->fastTimeoutHandling();
    delete[] mylist;
  }

  // reschedule for the next timeout
  double time = tm->inHost()->t2d(tm->getNow(), 0);
  //double t = calc_fast_timeout(time);
  double t =  tm->fast_timeout_double +
		      tm->fast_timeout_double * ((time + tm->boot_time_double) / tm->fast_timeout_double)
		      - time - tm->boot_time_double;
  HandleCode h = tm->inHost()->waitFor( tm->fast_timer_callback_proc, ac, tm->inHost()->d2t(t, 0), tm->inHost()->tie_breaking_seed );
}

void TCPMaster::slow_timer_callback(Activation ac)
{
  TCPMaster* tm = (TCPMaster*)((ProtocolCallbackActivation*)ac)->session;

  tm->delete_defunct_sessions();

  // go through each connected session that requires slow timer; to
  // prevent changes during iteration, we copy the list to an array.
  int mysize;
  if((mysize = tm->connected_sessions.size()) > 0)
  {
    TCPSession** mylist  = new TCPSession*[mysize];
    assert(mylist);
    int idx = 0;
    for(S3FNET_SET(TCPSession*)::iterator iter = tm->connected_sessions.begin();
	iter != tm->connected_sessions.end(); iter++, idx++)
      mylist[idx] = (*iter);
    for(idx = 0; idx < mysize; idx++)
      mylist[idx]->slowTimeoutHandling();
    delete[] mylist;
  }

  // reschedule for the next timeout
  double time = tm->inHost()->t2d(tm->getNow(), 0);
  //double t = calc_slow_timeout(time);
  double t =   tm->slow_timeout_double +
		  tm->slow_timeout_double * ( (time + tm->boot_time_double) / tm->slow_timeout_double)
		  - time - tm->boot_time_double;
  HandleCode h = tm->inHost()->waitFor( tm->slow_timer_callback_proc, ac, tm->inHost()->d2t(t, 0), tm->inHost()->tie_breaking_seed );
}

}; // namespace s3fnet
}; // namespace s3f

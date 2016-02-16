/**
 * \file blocking_tcp_client.cc
 * \brief Source file for the BTCPClientSession class.
 *
 * authors : Lenny Winterrowd, Dong (Kevin) Jin
 */

#include "os/tcp/app/blocking_tcp_client.h"
#include "os/base/protocols.h"
#include "os/socket/socket_master.h"
#include "util/errhandle.h"
#include "net/host.h"
#include <netinet/in.h>
#include "net/net.h"
#include "net/traffic.h"
#include "env/namesvc.h"

//#define TCP_DEBUG
#ifdef TCP_DEBUG
#define TCP_DUMP(x) printf("TCPCLT: "); x
#else
#define TCP_DUMP(x)
#endif

#define DEFAULT_SERVER_PORT     10
#define DEFAULT_CLIENT_PORT   2048

namespace s3f {
namespace s3fnet {

S3FNET_REGISTER_PROTOCOL(BTCPClientSession, "S3F.OS.TCP.test.BTCPClient");

/* The continuation used in tcp client. */
class BTCPClientSessionContinuation : public BSocketContinuation {
 public:
  enum {
    BTCP_CLIENT_SESSION_UNITIALIZED = 0,
    BTCP_CLIENT_SESSION_CONNECTING  = 1,
    BTCP_CLIENT_SESSION_RECEIVING   = 2,
    BTCP_CLIENT_SESSION_CLOSING     = 3
  };

  BTCPClientSessionContinuation(BTCPClientSession* client, int sock, ltime_t startime) :
    BSocketContinuation(client), socket(sock),
    status(BTCP_CLIENT_SESSION_UNITIALIZED),
    start_time(startime), user_timer(0) {
		setSCVal(11); // DEBUG
  }

  virtual ~BTCPClientSessionContinuation() {}

  virtual const char* getType() {
    return "BTCPClientSessionContinuation";
  }

  /* When a continuation returns successfully, this function will be
     called. Based on the current status of this continuation, we will
     decide what to do next. */
  virtual void success() {
    BTCPClientSession* client = (BTCPClientSession*)owner;
    switch(status) {
    case BTCP_CLIENT_SESSION_CONNECTING:
      client->server_connected(this);
      break;
    case BTCP_CLIENT_SESSION_RECEIVING:
      client->data_received(this);
      break;
    case BTCP_CLIENT_SESSION_CLOSING:
      client->session_closed(this);
      delete this; // end of the journey!!!
      break;
    default:
      TCP_DUMP(printf("ERROR: cnt=%lx received undefined success() status %d\n",(unsigned long)this,status)); // DEBUG
      TCP_DUMP(printf("cnt->status @ %lx\n",(unsigned long)(&status))); // DEBUG
      assert(0);
      break;
    }
  }

  /* When a continuation returns unsuccessfully, this function will be
     called to print out the error. */
  virtual void failure() {
    BTCPClientSession* client = (BTCPClientSession*)owner;
    switch(status) {
    case BTCP_CLIENT_SESSION_CONNECTING:
      TCP_DUMP(printf("BTCPClientSessionContinuation::failure() on host \"%s\": "
		      "failed to connect to server.\n",
		      client->inHost()->nhi.toString()));
      break;
    case BTCP_CLIENT_SESSION_RECEIVING: {
      TCP_DUMP(printf("BTCPClientSessionContinuation::failure() on host \"%s\": "
		      "failed to receive data.\n",
		      client->inHost()->nhi.toString()));

      //if(user_timer && user_timer->isRunning())
      if(user_timer)
      {
    	// If the timer is running, it means that the failed receive
    	// is not due to time out so that the socket is aborted. In
    	// this case, we cancel the timer and manually abort the socket.
    	HandlePtr hptr(new Handle(user_timer)); //todo not sure isRunning is needed or not
    	hptr->cancel();
    	user_timer = 0;

    	client->sm->abort(socket);
      }

      client->main_proc(true, 0);
      break;
    }
    case BTCP_CLIENT_SESSION_CLOSING:
      TCP_DUMP(printf("BTCPClientSessionContinuation::failure() on host \"%s\": "
		      "failed to close connection.\n",
		      client->inHost()->nhi.toString()));
      break;
    default:
      TCP_DUMP(printf("ERROR: received undefined failure() status %d\n",status));
      TCP_DUMP(printf("cnt->status @ %lx\n",(unsigned long)(&status))); // DEBUG
      assert(0);
      break;
    }

    delete this;
  }

  /* This function is called by the BTCPClientSessionTimer upon the expiration of the timer. */
  void timeout()
  {
    assert(user_timer);
    ((BTCPClientSession*)owner)->timeout(socket);
  }

  void user_timer_callback(Activation ac)
  {
	//BTCPClientSession* client = (BTCPClientSession*)((BTCPClientSessionCallbackActivation*)ac)->session;
	BTCPClientSessionContinuation* cnt = (BTCPClientSessionContinuation*)((BTCPClientSessionCallbackActivation*)ac)->cnt;
	cnt->timeout(); // will reclaim this timer and the continuation
  }

 public:
  int socket; // the socket associated with this continuation
  int status; // the status of the client session
  ltime_t start_time; // the start time of the client session
  uint32 rcvd_bytes; // total bytes received so far
  HandleCode user_timer; // user timer for off time
  Process* user_timer_callback_proc;
  BTCPClientSessionCallbackActivation* user_timer_ac;
};

BTCPClientSession::BTCPClientSession(ProtocolGraph* graph) :
  ProtocolSession(graph), nsess(0), start_timer_ac(0), start_timer_callback_proc(0), server_ip(0), server_port(0)
{
  TCP_DUMP(printf("[host=\"%s\"] new tcp client session.\n", inHost()->nhi.toString()));
}

BTCPClientSession::~BTCPClientSession(){}

void BTCPClientSession::config(s3f::dml::Configuration *cfg)
{
  ProtocolSession::config(cfg);

  char* str = (char*)cfg->findSingle("start_time");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: BTCPClientSession::config(), invalid START_TIME attribute.\n");
    start_time_double = atof(str);
  }
  else start_time_double = 0;
  start_time = inHost()->d2t(start_time_double, 0);

  str = (char*)cfg->findSingle("start_window");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: BTCPClientSession::config(), invalid START_WINDOW attribute.\n");
    start_window_double = atof(str);
  }
  else
	start_window_double = 0;
  start_window = inHost()->d2t(start_window_double);

  double user_timeout_double;
  str = (char*)cfg->findSingle("user_timeout");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: BTCPClientSession::config(), invalid USER_TIMEOUT attribute.\n");
    user_timeout_double = atof(str);
  }
  else user_timeout_double = 100; //default is 100 s
  user_timeout = inHost()->d2t(user_timeout_double, 0);

  str = (char*)cfg->findSingle("off_time");
  if(!str || s3f::dml::dmlConfig::isConf(str))
    error_quit("ERROR: BTCPClientSession::config(), missing or invalid OFF_TIME attribute.\n");
  off_time_double = atof(str);
  off_time = inHost()->d2t(off_time_double, 0);

  str = (char*)cfg->findSingle("off_time_run_first");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: BTCPClientSession::config(), invalid OFF_TIME_RUN_FIRST attribute.\n");
    if(!strcasecmp(str, "true")) off_time_run_first = true;
    else off_time_run_first = false;
  }
  else off_time_run_first = false;

  str = (char*)cfg->findSingle("off_time_exponential");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: BTCPClientSession::config(), invalid OFF_TIME_EXPONENTIAL attribute.\n");
    if(!strcasecmp(str, "true")) off_time_exponential = true;
    else off_time_exponential = false;
  }
  else off_time_exponential = false;

  str = (char*)cfg->findSingle("fixed_server");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: BTCPClientSession::config(), invalid FIXED_SERVER attribute.\n");
    if(!strcasecmp(str, "true")) fixed_server = true;
    else fixed_server = false;
  }
  else fixed_server = false;

  str = (char*)cfg->findSingle("file_size");
  if(!str || s3f::dml::dmlConfig::isConf(str))
    error_quit("ERROR: BTCPClientSession::config(), missing or invalid FILE_SIZE attribute.\n");
  file_size = atoi(str);

  str = (char*)cfg->findSingle("client_port");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: BTCPClientSession::config(), invalid CLIENT_PORT attribute.\n");
    start_port = atoi(str);
  }
  else start_port = DEFAULT_CLIENT_PORT;

  str = (char*)cfg->findSingle("server_list");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: BTCPClientSession::config(), invalid SERVER_LIST attribute.\n");
    server_list = str;
  }
  else server_list = "forTCP";

  str = (char*)cfg->findSingle("SHOW_REPORT");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: BTCPClientSession::config(), invalid SHOW_REPORT attribute.\n");
    if(!strcasecmp(str, "true")) show_report = true;
    else if(!strcasecmp(str, "false")) show_report = false;
    else error_quit("ERROR: BTCPClientSession::config(), invalid SHOW_REPORT attribute (%s).\n", str);
  }
  else show_report = true;

  TCP_DUMP(printf("[host=\"%s\"] config():\n"
		  "  start_time=%ld, start_window=%ld, user_timeout=%ld, off_time=%ld,\n"
		  "  off_time_run_first=%d, off_time_exponential=%d, fixed_server=%d,\n"
		  "  file_size=%u, start_port=%u.\n",
		  inHost()->nhi.toString(), start_time, start_window, user_timeout,
		  off_time, off_time_run_first, off_time_exponential, fixed_server,
		  file_size, start_port));
}

void BTCPClientSession::init()
{
  ProtocolSession::init();

  client_ip = inHost()->getDefaultIP();
  if(IPADDR_INVALID == client_ip)
    error_quit("ERROR: BTCPClientSession::init(), invalid IP address, the host's not connected to network.\n");

  sm = SOCKET_API;
  if(!sm)
	  error_quit("BTCPClientSession::init(), missing socket master on host \"%s\".\n", inHost()->nhi.toString());

  TCP_DUMP(printf("[host=\"%s\"] init(), client_ip=\"%s\".\n",
		  inHost()->nhi.toString(), IPPrefix::ip2txt(client_ip)));

  start_timer_called_once = false;

  // get new start time and wait
  if(start_window > 0)
  {
    start_time_double += getRandom()->Uniform(0, 1)*start_window_double;
    start_time = inHost()->d2t(start_time_double, 0);
  }
  TCP_DUMP(printf("[host=\"%s\"] %s: main_proc(), starting client until %ld.\n",
		  inHost()->nhi.toString(), getNowWithThousandSeparator(), start_time));

  main_proc(off_time_run_first, start_time);
}

void BTCPClientSession::main_proc(int sample_off_time, ltime_t lead_time)
{
  // check whether off time starts first
  ltime_t t = lead_time;
  if(sample_off_time)
  {
    if(off_time == 0)
    {
      TCP_DUMP(printf("[host=\"%s\"] %s: main_proc(), OFF (forever).\n",
		      inHost()->nhi.toString(), getNowWithThousandSeparator()));
      return;
    }
    else
    {
      double vt_double;
      if(off_time_exponential)
    	  vt_double = getRandom()->Exponential(1.0/off_time_double);
      else
    	  vt_double = off_time_double;

      ltime_t vt = inHost()->d2t(vt_double, 0);
      t += vt;

      TCP_DUMP(printf("[host=\"%s\"] %s: main_proc(), OFF (duration=%ld).\n",
		      inHost()->nhi.toString(), getNowWithThousandSeparator(), vt));
    }
  }

  Host* owner_host = inHost();
  start_timer_callback_proc = new Process( (Entity *)owner_host, (void (s3f::Entity::*)(s3f::Activation))&BTCPClientSession::start_timer_callback);
  start_timer_ac = new ProtocolCallbackActivation(this);
  Activation ac (start_timer_ac);
  HandleCode h = owner_host->waitFor( start_timer_callback_proc, ac, t, owner_host->tie_breaking_seed );
}

void BTCPClientSession::start_once()
{
  // if fixed_server is true, the client connects with the same server
  // for the whole simulation; the server is chosen from randomly from
  // dml traffic description
  if(fixed_server)
  {
    if(!get_random_server(server_ip, server_port))
      return; // stop if something's wrong
  }

  start_on();
}

void BTCPClientSession::start_on()
{
  // if the server is not fixed, the client connects with the
  // different servers for the whole simulation
  if(!fixed_server)
  {
    if(!get_random_server(server_ip, server_port))
      return; // stop if something's wrong
  }

  TCP_DUMP(printf("[host=\"%s\"] %s: main_proc(), ON.\n",
		  inHost()->nhi.toString(), getNowWithThousandSeparator()));
  nsess++;

  int sock = sm->socket();
  if(!sm->bind(sock, IPADDR_INADDR_ANY/*client_ip*/, start_port++, TCP_PROTOCOL_NAME, 0))
  {
    TCP_DUMP(printf("start_on() on host \"%s\": failed to bind.\n", inHost()->nhi.toString()));
    return;
  }

  TCP_DUMP(printf("[host=\"%s\"] %s: session_proc(), socket %d connecting to server: \"%s:%d\".\n",
		  inHost()->nhi.toString(), getNowWithThousandSeparator(), sock,
		  IPPrefix::ip2txt(server_ip), server_port));

  // creating a continuation for this session.
  BTCPClientSessionContinuation* cnt = new BTCPClientSessionContinuation(this, sock, getNow());
  cnt->status = BTCPClientSessionContinuation::BTCP_CLIENT_SESSION_CONNECTING;
  TCP_DUMP(printf("DEBUG: cnt(%lx)->status = %d\n",(unsigned long)cnt,cnt->status)); // DEBUG
  TCP_DUMP(printf("DEBUG: cnt->status @ %lx\n",(unsigned long)(&cnt->status))); // DEBUG
  sm->connect(sock, server_ip, server_port, (BSocketContinuation*)cnt);
}

void BTCPClientSession::server_connected(BTCPClientSessionContinuation* const cnt)
{
  TCP_DUMP(printf("[host=\"%s\"] %s: session_proc(), socket %d connected; waiting for data.\n",
		  inHost()->nhi.toString(), getNowWithThousandSeparator(), cnt->socket));

  // time out if the file transfer takes too long
  Host* owner_host = inHost();
  cnt->user_timer_callback_proc =
		  new Process( (Entity *)owner_host, (void (s3f::Entity::*)(s3f::Activation))&BTCPClientSessionContinuation::user_timer_callback);
  cnt->user_timer_ac = new BTCPClientSessionCallbackActivation(this, cnt);
  Activation ac (cnt->user_timer_ac);
  cnt->user_timer = inHost()->waitFor( cnt->user_timer_callback_proc, ac, user_timeout, inHost()->tie_breaking_seed );

  cnt->rcvd_bytes = 0;
  if(cnt->rcvd_bytes < file_size)
  {
    int to_recv = file_size-cnt->rcvd_bytes;
    cnt->status = BTCPClientSessionContinuation::BTCP_CLIENT_SESSION_RECEIVING;
    sm->recv(cnt->socket, to_recv, 0, (BSocketContinuation*)cnt);
  }
}

void BTCPClientSession::data_received(BTCPClientSessionContinuation* const cnt)
{
  cnt->rcvd_bytes += cnt->retval;

  TCP_DUMP(printf("[host=\"%s\"] %s: session_proc(), socket %d "
		  "received %d bytes (%d bytes left).\n",
		  inHost()->nhi.toString(), getNowWithThousandSeparator(),
		  cnt->socket, cnt->retval, file_size-cnt->rcvd_bytes));

  if(cnt->rcvd_bytes < file_size)
  {
    int to_recv = file_size-cnt->rcvd_bytes;
    assert(cnt->status == BTCPClientSessionContinuation::BTCP_CLIENT_SESSION_RECEIVING);
    sm->recv(cnt->socket, to_recv, 0, (BSocketContinuation*)cnt);
    return;
  }

  if(show_report)
  {
    char buf1[50]; char buf2[50];
    double total_time = inHost()->t2d(getNow() - cnt->start_time, 0);
    printf("%s: TCP client \"%s\" downloaded %d bytes from server \"%s\", throughput %f Kb/s.\n",
	   getNowWithThousandSeparator(), IPPrefix::ip2txt(client_ip, buf1), file_size,
	   IPPrefix::ip2txt(server_ip, buf2), (8e-3*file_size/total_time));
  }

  HandlePtr hptr(new Handle(cnt->user_timer));
  hptr->cancel();
  cnt->user_timer = 0;

  // waiting for closing tcp connection.
  cnt->status = BTCPClientSessionContinuation::BTCP_CLIENT_SESSION_CLOSING;
  sm->close(cnt->socket, (BSocketContinuation*)cnt);
}

void BTCPClientSession::session_closed(BTCPClientSessionContinuation* const cnt)
{
  TCP_DUMP(printf("[host=\"%s\"] %s: session_proc(), socket %d connection closed.\n",
		  inHost()->nhi.toString(), getNowWithThousandSeparator(), cnt->socket));
  main_proc(true, 0);

  // the continuation will be reclaimed after this
}

int BTCPClientSession::push(Activation msg, ProtocolSession* hi_sess, void* extinfo, size_t extinfo_size)
{
  error_quit("ERROR: BTCPClientSession::push() should not be called.\n");
  return 1;
}

int BTCPClientSession::pop(Activation msg, ProtocolSession* lo_sess, void* extinfo, size_t extinfo_size)
{
  error_quit("ERROR: BTCPClientSession::pop() should not be called.\n");
  return 1;
}

bool BTCPClientSession::get_random_server(IPADDR& server_ip, uint16& server_port)
{
  Traffic* traffic = 0;
  Host* host = inHost();

  //debug
  /*printf("BTCPClientSession::get_random_server(), now = %s, before inNet()->control, ctrl_msg = %d\n",
		  getNowWithThousandSeparator(), NET_CTRL_GET_TRAFFIC);*/

  host->inNet()->control(NET_CTRL_GET_TRAFFIC, (void*)&traffic);
  if(!traffic) return false;

  S3FNET_VECTOR(TrafficServerData*) servers;
  if(!traffic->getServers(host, servers, server_list.c_str()) || servers.size() == 0)
  {
    if(show_report)
      printf("WARNING: [host=\"%s\"] %s: get_random_server(), found no server for %s.\n",
	     inHost()->nhi.toString(), getNowWithThousandSeparator(), server_list.c_str());
    return false;
  }

  int sidx = (int)floor(getRandom()->Uniform(0, 1) * servers.size());
  server_ip = host->inNet()->getNameService()->nhi2ip(servers[sidx]->nhi->toStlString());
  server_port = servers[sidx]->port;

  if(server_ip == IPADDR_INVALID)
  {
    if(show_report)
      printf("WARNING: [host=\"%s\"] %s: get_random_server(), unresolved server for \"%s\": %s:%d.\n",
	     inHost()->nhi.toString(), getNowWithThousandSeparator(), server_list.c_str(),
	     IPPrefix::ip2txt(server_ip), server_port);
    return false;
  }

  TCP_DUMP(printf("[host=\"%s\"] %s: get_random_server(), selected server: \"%s:%d\".\n",
		  inHost()->nhi.toString(), getNowWithThousandSeparator(), IPPrefix::ip2txt(server_ip), server_port));
  return true;
}

void BTCPClientSession::timeout(int sock)
{
  TCP_DUMP(printf("[host=\"%s\"] %s: timeout(), socket %d abort.\n",
		  inHost()->nhi.toString(), getNowWithThousandSeparator(), sock));
  sm->abort(sock);
}

void BTCPClientSession::start_timer_callback(Activation ac)
{
  BTCPClientSession* client = (BTCPClientSession*)((ProtocolCallbackActivation*)ac)->session;
  if(client->start_timer_called_once == true)
  {
	  client->start_on();
  }
  else
  {
	client->start_timer_called_once = true;
	client->start_once();
  }
}

}; // namespace s3fnet
}; // namespace s3f

/**
 * \file udp_server.cc
 * \brief Source file for the UDPServerSession class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "os/udp/app/udp_server.h"
#include "os/socket/socket_master.h"
#include <netinet/in.h>
#include "util/errhandle.h"
#include "net/host.h"
#include "os/base/protocols.h"


#ifdef UDP_DEBUG
#define UDP_DUMP(x) printf("UDPSVR: "); x
#else
#define UDP_DUMP(x)
#endif

#define DEFAULT_SERVER_PORT 20

namespace s3f {
namespace s3fnet {

S3FNET_REGISTER_PROTOCOL(UDPServerSession, "S3F.OS.UDP.test.UDPServer");

/** The continuation used in UDP server. */
class UDPServerSessionContinuation : public BSocketContinuation {
 public:
  enum {
    UDP_SERVER_SESSION_UNITIALIZED = 0,
    UDP_SERVER_SESSION_RECEIVING   = 1,
    UDP_SERVER_SESSION_CONNECTING  = 2,
    UDP_SERVER_SESSION_SENDING     = 3,
    UDP_SERVER_SESSION_CLOSING     = 4
  };
  
  UDPServerSessionContinuation(UDPServerSession* server, int ssock) : 
    BSocketContinuation(server), server_socket(ssock), 
    status(UDP_SERVER_SESSION_UNITIALIZED) {
    reqbuf = new byte[server->request_size];
    assert(reqbuf);
  }

  virtual ~UDPServerSessionContinuation() {
    delete[] reqbuf;
  }

  /* When a continuation returns successfully, this function will be
     called. Based on the current status of this continuation, we will
     decide what to do next. */
  virtual void success() {
    UDPServerSession* server = (UDPServerSession*)owner;
    switch(status) {
    case UDP_SERVER_SESSION_RECEIVING:
      server->request_received(this);
      break;
    case UDP_SERVER_SESSION_CONNECTING:
      server->client_connected(this);
      break;
    case UDP_SERVER_SESSION_SENDING:
      server->data_sent(this);
      break;
    case UDP_SERVER_SESSION_CLOSING:
      server->session_closed(this);
      delete this; // end of the journey!!!
      break;
    default:
      assert(0);
      break;
    }
  }

  /* When a continuation returns unsuccessfully, this function will be
     called to print out the error. */
  virtual void failure() {
    UDPServerSession* server = (UDPServerSession*)owner;
    switch(status) {
    case UDP_SERVER_SESSION_RECEIVING:
      UDP_DUMP(printf("UDPServerSessionContinuation::failure() on host \"%s\": "
		      "failed to receive request from client.\n",
		      server->inHost()->nhi.toString()));
      server->handle_client(server_socket);
      break;
    case UDP_SERVER_SESSION_CONNECTING:
      UDP_DUMP(printf("UDPServerSessionContinuation::failure() on host \"%s\": "
		      "failed to connect to client.\n",
		      server->inHost()->nhi.toString()));
      // since this session has failed, we should decrease the number
      // of clients in the system.
      if(server->client_limit && server->nclients-- == server->client_limit)
    	server->handle_client(server_socket);
      break;
    case UDP_SERVER_SESSION_SENDING:
      UDP_DUMP(printf("UDPServerSessionContinuation::failure() on host \"%s\": "
		      "failed to send to client.\n",
		      server->inHost()->nhi.toString()));
      // since this session has failed, we should decrease the number
      // of clients in the system.
      if(server->client_limit && server->nclients-- == server->client_limit)
    	server->handle_client(server_socket);
      break;
    case UDP_SERVER_SESSION_CLOSING:
      UDP_DUMP(printf("UDPServerSessionContinuation::failure() on host \"%s\": "
		      "failed to close the connection.\n",
		      server->inHost()->nhi.toString()));
      // since this session has failed, we should decrease the number
      // of clients in the system.
      if(server->client_limit && server->nclients-- == server->client_limit)
    	server->handle_client(server_socket);
      break;
    default:
      assert(0);
      break;
    }

    // delete this continuation.
    delete this;
  }

  void send_timeout()
  {
    ((UDPServerSession*)owner)->interval_elapsed(this);
  }

  void send_time_callback(Activation ac)
  {
	UDPServerSessionContinuation* cnt = (UDPServerSessionContinuation*)((UDPServerSessionCallbackActivation*)ac)->cnt;
	cnt->send_timeout();
  }

 public:
  int server_socket; ///< server socket session on well-known port
  int client_socket; ///< the socket connection with a client
  int status; ///< the status of this socket session
  byte* reqbuf; ///< buffer for storing request from client

  IPADDR client_ip; ///< client ip address of this socket session
  uint16 client_port; ///< client port number of this socket session
  int file_size; ///< number of bytes to be sent to the client
  int sent_bytes; ///< number of bytes already sent

  HandleCode send_timer; ///< user timer for off time
  Process* send_time_callback_proc;
  UDPServerSessionCallbackActivation* send_time_ac;
};

UDPServerSession::UDPServerSession(ProtocolGraph* graph) :
  ProtocolSession(graph)
{
  UDP_DUMP(printf("[host=\"%s\"] new udp server session.\n", inHost()->nhi.toString()));
}

UDPServerSession::~UDPServerSession() {}

void UDPServerSession::config(s3f::dml::Configuration *cfg)
{
  ProtocolSession::config(cfg);

  char* str = (char*)cfg->findSingle("port");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: UDPServerSession::config(), invalid PORT attribute.\n");
    server_port = atoi(str);
  }
  else server_port = DEFAULT_SERVER_PORT;

  str = (char*)cfg->findSingle("request_size");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: UDPServerSession::config(), invalid REQUEST_SIZE attribute.\n");
    request_size = atoi(str);
    if(request_size < sizeof(uint32))
      error_quit("ERROR: UDPServerSession::config(), REQUEST_SIZE must be larger than 4 (bytes).\n");
  }
  else request_size = sizeof(uint32);

  str = (char*)cfg->findSingle("client_limit");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: UDPServerSession::config(), invalid CLIENT_LIMIT attribute.\n");
    client_limit = atoi(str);
  }
  else client_limit = 0; // means infinite

  str = (char*)cfg->findSingle("datagram_size");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: UDPServerSession::config(), invalid DATAGRAM_SIZE attribute.\n");
    datagram_size = atoi(str);
  }
  else datagram_size = 1000;

  double send_interval_double;
  str = (char*)cfg->findSingle("send_interval");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: UDPServerSession::config(), invalid SEND_INTERVAL attribute.\n");
    send_interval_double = atof(str);
  }
  else send_interval_double = 0;
  send_interval = inHost()->d2t(send_interval_double, 0);

  str = (char*)cfg->findSingle("SHOW_REPORT");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: UDPServerSession::config(), invalid SHOW_REPORT attribute.\n");
    if(!strcasecmp(str, "true")) show_report = true;
    else if(!strcasecmp(str, "false")) show_report = false;
    else error_quit("ERROR: UDPServerSession::config(), invalid SHOW_REPORT attribute (%s).\n", str);
  }
  else show_report = true;

  UDP_DUMP(printf("[host=\"%s\"] config():\n"
		  "  server_port=%u, request_size=%u, client_limit=%u,\n"
		  "  datagram_size=%u, send_interval=%ld.\n",
		  inHost()->nhi.toString(), server_port, request_size,
		  client_limit, datagram_size, send_interval));
}

void UDPServerSession::init()
{
  ProtocolSession::init();

  server_ip = inHost()->getDefaultIP();
  if(IPADDR_INVALID == server_ip)
    error_quit("ERROR: UDPServerSession::init(), invalid IP address, the host's not connected to network.\n");

  sm = SOCKET_API;
  if(!sm)
  {
	  error_quit("UDPServerSession::init(), missing socket master on host \"%s\".\n",
		     inHost()->nhi.toString());
  }
  UDP_DUMP(printf("[host=\"%s\"] init(), server_ip=\"%s\".\n",
		  inHost()->nhi.toString(), IPPrefix::ip2txt(server_ip)));

  Host* owner_host = inHost();
  start_timer_callback_proc = new Process( (Entity *)owner_host, (void (s3f::Entity::*)(s3f::Activation))&UDPServerSession::start_timer_callback);
  start_timer_ac = new ProtocolCallbackActivation(this);
  Activation ac (start_timer_ac);
  HandleCode h = inHost()->waitFor( start_timer_callback_proc, ac, 0, inHost()->tie_breaking_seed ); //currently the starting time is 0
}

void UDPServerSession::start_on()
{
  UDP_DUMP(printf("[host=\"%s\"] %s: main_proc(), starting server.\n",
		  inHost()->nhi.toString(), getNowWithThousandSeparator()));

  int ssock = sm->socket();
  if(!sm->bind(ssock, IPADDR_ANYDEST, server_port, UDP_PROTOCOL_NAME, 0))
  {
    UDP_DUMP(printf("start_on() on host \"%s\": failed to bind.\n",
		    inHost()->nhi.toString()));
    return;
  }

  if(!sm->listen(ssock, 0))
  {
    UDP_DUMP(printf("start_on() on host \"%s\": failed to listen.\n",
		    inHost()->nhi.toString()));
    return;
  }
  
  handle_client(ssock);
}

void UDPServerSession::handle_client(int ssock)
{
  UDPServerSessionContinuation* cnt = new UDPServerSessionContinuation(this, ssock);
  cnt->status = UDPServerSessionContinuation::UDP_SERVER_SESSION_RECEIVING;
  sm->recv(ssock, request_size, cnt->reqbuf, cnt);
}

void UDPServerSession::request_received(UDPServerSessionContinuation* const cnt)
{
  // assume the request can be received with one call
  assert(cnt->retval == (int)request_size);

  cnt->file_size = ntohl(*(uint32*)cnt->reqbuf);
  UDP_DUMP(printf("[host=\"%s\"] %s: session_proc(), socket %d received request "
		  "(request_size=%u, file_size=%u).\n",
		  inHost()->nhi.toString(), getNowWithThousandSeparator(),
		  cnt->server_socket, request_size, cnt->file_size));

  if(cnt->file_size <= 0)
  {
    handle_client(cnt->server_socket);
    return; 
  }

  // A side-effect of this 'connected' call is to give remote ip
  // address and port number
  sm->connected(cnt->server_socket, &cnt->client_ip, &cnt->client_port);

  if(!client_limit || nclients++ < client_limit)
    handle_client(cnt->server_socket);
  
  cnt->client_socket = sm->socket();
  if(!sm->bind(cnt->client_socket, server_ip, ++server_port, UDP_PROTOCOL_NAME, 0))
  {
    UDP_DUMP(printf("request_received() on host \"%s\": failed to bind.\n", inHost()->nhi.toString()));
    if(client_limit && nclients-- == client_limit)
      handle_client(cnt->server_socket);
    return;
  }

  UDP_DUMP(printf("[host=\"%s\"] %s: session_proc(), socket %d connecting to client: \"%s:%d\".\n",
		  inHost()->nhi.toString(), getNowWithThousandSeparator(),
		  cnt->client_socket, IPPrefix::ip2txt(cnt->client_ip), cnt->client_port));

  cnt->sent_bytes = 0;

  cnt->status = UDPServerSessionContinuation::UDP_SERVER_SESSION_CONNECTING;
  sm->connect(cnt->client_socket, cnt->client_ip, cnt->client_port, cnt);
}

void UDPServerSession::client_connected(UDPServerSessionContinuation* const cnt)
{
  if(cnt->sent_bytes < cnt->file_size)
  {
    Host* owner_host = inHost();
    cnt->send_time_callback_proc =
  		  new Process( (Entity *)owner_host, (void (s3f::Entity::*)(s3f::Activation))&UDPServerSessionContinuation::send_time_callback);
    cnt->send_time_ac = new UDPServerSessionCallbackActivation(this, cnt);
    Activation ac (cnt->send_time_ac);
    cnt->send_timer = inHost()->waitFor( cnt->send_time_callback_proc, ac, send_interval, inHost()->tie_breaking_seed );
  }
}

void UDPServerSession::interval_elapsed(UDPServerSessionContinuation* const cnt)
{
  uint32 to_send = cnt->file_size-cnt->sent_bytes;
  if(to_send > datagram_size) to_send = datagram_size;

  cnt->status = UDPServerSessionContinuation::UDP_SERVER_SESSION_SENDING;
  sm->send(cnt->client_socket, to_send, 0, cnt);
}

void UDPServerSession::data_sent(UDPServerSessionContinuation* const cnt)
{
  cnt->sent_bytes += cnt->retval;

  UDP_DUMP(printf("[host=\"%s\"] %s: session_proc(), socket %d "
		  "sent %d bytes (%d bytes left).\n",
		  inHost()->nhi.toString(), getNowWithThousandSeparator(),
		  cnt->client_socket, cnt->retval, cnt->file_size-cnt->sent_bytes));

  if(cnt->sent_bytes < cnt->file_size)
  {
    client_connected(cnt);
    return;
  }

  UDP_DUMP(printf("[host=\"%s\"] %s: session_proc(), finished sending %u bytes.\n",
		  inHost()->nhi.toString(), getNowWithThousandSeparator(), cnt->file_size));

  // the server will close the connection.
  cnt->status = UDPServerSessionContinuation::UDP_SERVER_SESSION_CLOSING;
  sm->close(cnt->client_socket, cnt);
}

void UDPServerSession::session_closed(UDPServerSessionContinuation* const cnt)
{
  UDP_DUMP(printf("[host=\"%s\"] %s: session_proc(), socket %d connection closed.\n",
		  inHost()->nhi.toString(), getNowWithThousandSeparator(), cnt->client_socket));

  if(client_limit && nclients-- == client_limit)
    handle_client(cnt->server_socket);

  // the continuation will be reclaimed after this
}

int UDPServerSession::push(Activation msg, ProtocolSession* hi_sess, void* extinfo, size_t extinfo_size)
{
  error_quit("ERROR: UDPServerSession::push() should not be called.\n");
  return 1;
}

int UDPServerSession::pop(Activation msg, ProtocolSession* lo_sess, void* extinfo, size_t extinfo_size)
{
  error_quit("ERROR: UDPServerSession::pop() should not be called.\n");
  return 1;
}

void UDPServerSession::start_timer_callback(Activation ac)
{
  UDPServerSession* server = (UDPServerSession*)((ProtocolCallbackActivation*)ac)->session;
  server->start_on();
}

}; // namespace s3fnet
}; // namespace s3f

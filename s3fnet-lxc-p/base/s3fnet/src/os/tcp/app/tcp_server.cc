/**
 * \file tcp_server.cc
 * \brief Source file for the TCPServerSession class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "os/tcp/app/tcp_server.h"
#include "os/socket/socket_master.h"
#include <netinet/in.h>
#include "util/errhandle.h"
#include "net/host.h"
#include "net/network_interface.h"
#include "os/base/protocols.h"


#ifdef TCP_DEBUG
#define TCP_DUMP(x) printf("TCPSVR: "); x
#else
#define TCP_DUMP(x)
#endif

#define DEFAULT_SERVER_PORT 10

namespace s3f {
namespace s3fnet {

S3FNET_REGISTER_PROTOCOL(TCPServerSession, "S3F.OS.TCP.test.TCPServer");

/** The continuation used in tcp server. */
class TCPServerSessionContinuation : public BSocketContinuation {
 public:
  enum {
    TCP_SERVER_SESSION_UNITIALIZED = 0,
    TCP_SERVER_SESSION_ACCEPTING   = 1,
    TCP_SERVER_SESSION_RECEIVING   = 2,
    TCP_SERVER_SESSION_SENDING     = 3,
    TCP_SERVER_SESSION_CLOSING     = 4
  };

  /** the constructor */
  TCPServerSessionContinuation(TCPServerSession* server, int ssock) :
    BSocketContinuation(server), server_socket(ssock),
    status(TCP_SERVER_SESSION_UNITIALIZED) {
    reqbuf = new byte[server->request_size];
    assert(reqbuf);
  }

  /** the destrcutor */
  virtual ~TCPServerSessionContinuation() {
    delete[] reqbuf;
  }

  /** When a continuation returns successfully, this function will be
     called. Based on the current status of this continuation, we will
     decide what to do next. */
  virtual void success() {
    TCPServerSession* server = (TCPServerSession*)owner;
    switch(status) {
    case TCP_SERVER_SESSION_ACCEPTING:
      server->connection_accepted(this);
      break;
    case TCP_SERVER_SESSION_RECEIVING:
      server->request_received(this);
      break;
    case TCP_SERVER_SESSION_SENDING:
      server->data_sent(this);
      break;
    case TCP_SERVER_SESSION_CLOSING:
      server->session_closed(this);
      delete this; // end of the journey!!!
      break;
    default:
      assert(0);
      break;
    }
  }

  /** When a continuation returns unsuccessfully, this function will be
     called to print out the error. */
  virtual void failure() {
    TCPServerSession* server = (TCPServerSession*)owner;
    switch(status) {
    case TCP_SERVER_SESSION_ACCEPTING:
      TCP_DUMP(printf("TCPServerSessionContinuation::failure() on host \"%s\": failed to accept.\n",
    		  server->inHost()->nhi.toString()));
      server->handle_client(server_socket);
      break;

    case TCP_SERVER_SESSION_RECEIVING:
      TCP_DUMP(printf("TCPServerSessionContinuation::failure() on host \"%s\": "
		      "failed to receive request from client.\n",
		      server->inHost()->nhi.toString()));
      if(server->client_limit && server->nclients-- == server->client_limit
    	 && server->accept_completed == true)
    	server->handle_client(server_socket);
      break;

    case TCP_SERVER_SESSION_SENDING:
      TCP_DUMP(printf("TCPServerSessionContinuation::failure() on host \"%s\": "
		      "failed to send to client.\n",
		      server->inHost()->nhi.toString()));
      // since this session has failed, we should decrease the number
      // of clients in the system.
      if(server->client_limit && server->nclients-- == server->client_limit
    	 && server->accept_completed == true)
    	server->handle_client(server_socket);
      break;

    case TCP_SERVER_SESSION_CLOSING:
      TCP_DUMP(printf("TCPServerSessionContinuation::failure() on host \"%s\": "
		      "failed to close the connection.\n",
		      server->inHost()->nhi.toString()));
      // since this session has failed, we should decrease the number
      // of clients in the system.
      if(server->client_limit && server->nclients-- == server->client_limit
    	 && server->accept_completed == true)
    	server->handle_client(server_socket);
      break;

    default:
      assert(0);
      break;
    }

    // delete this continuation.
    delete this;
  }

 public:
  int server_socket; ///< server socket session on well-known port
  int client_socket; ///< the socket connection with a client (socket returned from accept)
  int status; ///< the status of this socket session
  byte* reqbuf; ///< buffer for storing request from client

  IPADDR client_ip; ///< client ip address of this socket session
  uint16 client_port; ///< client port number of this socket session
  int file_size; ///< number of bytes to be sent to the client
  int sent_bytes; ///< number of bytes already sent
};

TCPServerSession::TCPServerSession(ProtocolGraph* graph) :
  ProtocolSession(graph), accept_completed(true), start_timer_ac(0), start_timer_callback_proc(0)
{
  TCP_DUMP(printf("[host=\"%s\"] new tcp server session.\n", inHost()->nhi.toString()));
}

TCPServerSession::~TCPServerSession() {}

void TCPServerSession::config(s3f::dml::Configuration *cfg)
{
  ProtocolSession::config(cfg);

  char* str = (char*)cfg->findSingle("port");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: TCPServerSession::config(), invalid PORT attribute.\n");
    server_port = atoi(str);
  }
  else server_port = DEFAULT_SERVER_PORT;

  str = (char*)cfg->findSingle("request_size");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: TCPServerSession::config(), invalid REQUEST_SIZE attribute.\n");
    request_size = atoi(str);
    if(request_size < sizeof(uint32))
      error_quit("ERROR: TCPServerSession::config(), REQUEST_SIZE must be larger than 4 (bytes).\n");
  }
  else request_size = sizeof(uint32);

  str = (char*)cfg->findSingle("client_limit");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: TCPServerSession::config(), invalid CLIENT_LIMIT attribute.\n");
    client_limit = atoi(str);
  }
  else client_limit = 0; // means infinite

  str = (char*)cfg->findSingle("SHOW_REPORT");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: TCPServerSession::config(), invalid SHOW_REPORT attribute.\n");
    if(!strcasecmp(str, "true")) show_report = true;
    else if(!strcasecmp(str, "false")) show_report = false;
    else error_quit("ERROR: TCPServerSession::config(), invalid SHOW_REPORT attribute (%s).\n", str);
  }
  else show_report = true;


  TCP_DUMP(printf("[host=\"%s\"] config():\n"
		  "  server_port=%u, request_size=%u, client_limit=%u.\n",
		  inHost()->nhi.toString(),
		  server_port, request_size, client_limit));
}

void TCPServerSession::init()
{
  ProtocolSession::init();

  server_ip = inHost()->getDefaultIP();
  if(IPADDR_INVALID == server_ip)
    error_quit("ERROR: TCPServerSession::init(), invalid IP address, the host's not connected to network.\n");

  sm = SOCKET_API;
  if(!sm)
  {
	  error_quit("TCPServerSession::init(), missing socket master on host \"%s\".\n", inHost()->nhi.toString());
  }
  TCP_DUMP(printf("[host=\"%s\"] init(), server_ip=\"%s\".\n",
		  inHost()->nhi.toString(), IPPrefix::ip2txt(server_ip)));

  Host* owner_host = inHost();
  start_timer_callback_proc = new Process( (Entity *)owner_host, (void (s3f::Entity::*)(s3f::Activation))&TCPServerSession::start_timer_callback);
  start_timer_ac = new ProtocolCallbackActivation(this);
  Activation ac (start_timer_ac);
  HandleCode h = inHost()->waitFor( start_timer_callback_proc, ac, 0, inHost()->tie_breaking_seed ); //currently the starting time is 0
}

void TCPServerSession::start_on()
{
  TCP_DUMP(printf("[host=\"%s\"] %s: main_proc(), starting server.\n",
		  inHost()->nhi.toString(), getNowWithThousandSeparator()));

  if((!client_limit || nclients++ < client_limit) && accept_completed == true) //should always true
  {
	  accept_completed = false;

	  int ssock = sm->socket();
	  if(!sm->bind(ssock, IPADDR_ANYDEST, server_port, TCP_PROTOCOL_NAME, 0))
	  {
	    TCP_DUMP(printf("session_proc() on host \"%s\": failed to bind.\n",
			    inHost()->nhi.toString()));
	    nclients--;
	    accept_completed = true;
	    return;
	  }

	  TCP_DUMP(printf("[host=\"%s\"] %s: waiting for incoming connection on socket %d.\n",
			  inHost()->nhi.toString(), getNowWithThousandSeparator(), ssock));

	  handle_client(ssock); //call accept
  }
}

void TCPServerSession::handle_client(int ssock)
{
  TCPServerSessionContinuation* cnt = new TCPServerSessionContinuation(this, ssock);
  cnt->status = TCPServerSessionContinuation::TCP_SERVER_SESSION_ACCEPTING;
  sm->accept(ssock, true, cnt, 0);
}


void TCPServerSession::connection_accepted(TCPServerSessionContinuation* const cnt)
{
  accept_completed = true;

  //init another accept
  if((!client_limit || nclients++ < client_limit ) && accept_completed == true)
    handle_client(cnt->server_socket);

  cnt->client_socket = cnt->retval;

  cnt->status = TCPServerSessionContinuation::TCP_SERVER_SESSION_RECEIVING;
  sm->recv(cnt->client_socket, request_size, cnt->reqbuf, cnt);
}

void TCPServerSession::request_received(TCPServerSessionContinuation* const cnt)
{
  // assume the request can be received with one call
  assert(cnt->retval == (int)request_size);

  cnt->file_size = ntohl(*(uint32*)cnt->reqbuf);
  TCP_DUMP(printf("[host=\"%s\"] %s: session_proc(), socket %d received request "
		  "(request_size=%u, file_size=%u).\n",
		  inHost()->nhi.toString(), getNowWithThousandSeparator(),
		  cnt->server_socket, request_size, cnt->file_size));

  cnt->sent_bytes = 0;

  sending_data(cnt);
}

void TCPServerSession::sending_data(TCPServerSessionContinuation* const cnt)
{
  if(cnt->sent_bytes < cnt->file_size)
  {
	  uint32 to_send = cnt->file_size-cnt->sent_bytes;

	  cnt->status = TCPServerSessionContinuation::TCP_SERVER_SESSION_SENDING;
	  sm->send(cnt->client_socket, to_send, 0, cnt);
  }
}

void TCPServerSession::data_sent(TCPServerSessionContinuation* const cnt)
{
  cnt->sent_bytes += cnt->retval;

  TCP_DUMP(printf("[host=\"%s\"] %s: session_proc(), socket %d "
		  "sent %d bytes (%d bytes left).\n",
		  inHost()->nhi.toString(), getNowWithThousandSeparator(),
		  cnt->client_socket, cnt->retval, cnt->file_size-cnt->sent_bytes));

  if(cnt->sent_bytes < cnt->file_size)
  {
	sending_data(cnt);
    return;
  }

  TCP_DUMP(printf("[host=\"%s\"] %s: session_proc(), finished sending %u bytes.\n",
		  inHost()->nhi.toString(), getNowWithThousandSeparator(), cnt->file_size));

  // the server will close the connection.
  cnt->status = TCPServerSessionContinuation::TCP_SERVER_SESSION_CLOSING;
  sm->close(cnt->client_socket, cnt);
}

void TCPServerSession::session_closed(TCPServerSessionContinuation* const cnt)
{
  TCP_DUMP(printf("[host=\"%s\"] %s: session_proc(), socket %d connection closed.\n",
		  inHost()->nhi.toString(), getNowWithThousandSeparator(), cnt->client_socket));

  if(client_limit && nclients-- == client_limit && accept_completed == true)
    handle_client(cnt->server_socket);

  // the continuation will be reclaimed after this
}

int TCPServerSession::push(Activation msg, ProtocolSession* hi_sess, void* extinfo, size_t extinfo_size)
{
  error_quit("ERROR: TCPServerSession::push() should not be called.\n");
  return 1;
}

int TCPServerSession::pop(Activation msg, ProtocolSession* lo_sess, void* extinfo, size_t extinfo_size)
{
  error_quit("ERROR: TCPServerSession::pop() should not be called.\n");
  return 1;
}

void TCPServerSession::start_timer_callback(Activation ac)
{
  TCPServerSession* server = (TCPServerSession*)((ProtocolCallbackActivation*)ac)->session;
  server->start_on();
}

}; // namespace s3fnet
}; // namespace s3f

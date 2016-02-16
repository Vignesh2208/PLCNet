/**
 * \file nb_tcp_client.cc
 * \brief Source file for the NBTCPClientSession class.
 *
 * authors : Lenny Winterrowd
 */

/*
 * Copyright (c) 2013 University of Illinois
 *
 * Permission is hereby granted, free of charge, to any individual or
 * institution obtaining a copy of this software and associated
 * documentation files (the "Software"), to use, copy, modify, and
 * distribute without restriction, provided that this copyright and
 * permission notice is maintained, intact, in all copies and supporting
 * documentation.
 */

#include "os/tcp/app/nb_tcp_client.h"
#include "os/socket/nb_socket_master.h"
#include <netinet/in.h>
#include "util/errhandle.h"
#include "net/host.h"
#include "net/network_interface.h"
#include "os/base/protocols.h"
#include "net/net.h"
#include "net/traffic.h"
#include "env/namesvc.h"

//#define TCP_DEBUG
#ifdef TCP_DEBUG
#define TCP_DUMP(x) printf("TCPCLNT: "); x
#else
#define TCP_DUMP(x)
#endif

#define DEFAULT_SERVER_PORT   10
#define DEFAULT_CLIENT_PORT   2048

namespace s3f {
namespace s3fnet {

S3FNET_REGISTER_PROTOCOL(NBTCPClientSession, "S3F.OS.TCP.test.NBTCPClient");

/* The continuation used in tcp client. */
class NBTCPClientSessionContinuation : public NBSocketContinuation {
 public:
  enum {
    NB_TCP_CLIENT_STATUS_IDLE      = 0,
    NB_TCP_CLIENT_STATUS_RECEIVING = 1
  };

  enum {
    NB_TCP_CLIENT_SESSION_UNINITIALIZED = 0,
    NB_TCP_CLIENT_SESSION_INIT          = 1,
    NB_TCP_CLIENT_SESSION_MAIN_LOOP     = 2
  };

  NBTCPClientSessionContinuation(NBTCPClientSession* client, int ssock) :
    NBSocketContinuation(client), client_socket(ssock),
    status(NB_TCP_CLIENT_STATUS_IDLE),
    p_state(NB_TCP_CLIENT_SESSION_UNINITIALIZED) {
	//setSCVal(1337); // For debugging identification
  }

  virtual ~NBTCPClientSessionContinuation() {
    // N/A
  }

  virtual const char* getType () {
    return "NBTCPClientSessionContinuation";
  }

  /* When a continuation returns successfully, this function will be
     called. Based on the current status of this continuation, we will
     decide what to do next. */
  virtual void success() {
	NBTCPClientSession* client = (NBTCPClientSession*)owner;
    switch(status) {
      case NB_TCP_CLIENT_STATUS_RECEIVING:
	    client->non_blocking_loop(this);
        break;
    default:
      //assert(0);
      break;
    }
  }

  /* When a continuation returns unsuccessfully, this function will be
     called to print out the error. */
  virtual void failure() {
    NBTCPClientSession* client = (NBTCPClientSession*)owner;
    switch(status) {
    // NULL for now
    default:
      //assert(0);
      break;
    }

    // delete this continuation.
    delete this;
  }

  virtual void resume() {
    NBTCPClientSession* client = (NBTCPClientSession*)owner;
	TCP_DUMP(printf("NBTCPClientSessionContinuation::resume() on host \"%s\": "
		      "p_state = %d\n",
		      client->inHost()->nhi.toString(),p_state));
	fflush(stdout);
    switch(p_state) {
      case NB_TCP_CLIENT_SESSION_INIT:
        client->start_on(this);
        break;
      case NB_TCP_CLIENT_SESSION_MAIN_LOOP:
        client->non_blocking_loop(this);
        break;
      default:
        assert(0);
        break;
    }
  }

 public:
  int client_socket; // client socket session on well-known port
  int status; // the status of this socket session
  int p_state; // the current program state (where to resume execution)
  int connected; // buffer for storing request from client

  IPADDR client_ip; // client ip address of this socket session
  uint16 client_port; // client port number of this socket session
  int file_size; // number of bytes to be sent to the client
  int sent_bytes; // number of bytes already sent
};

NBTCPClientSession::NBTCPClientSession(ProtocolGraph* graph) :
  ProtocolSession(graph), start_timer_ac(0), start_timer_callback_proc(0),
  connected(false), bytes_received(0)
{
  TCP_DUMP(printf("[host=\"%s\"] new tcp server session.\n", inHost()->nhi.toString()));
}

NBTCPClientSession::~NBTCPClientSession() {}

bool NBTCPClientSession::get_random_server(IPADDR& server_ip, uint16& server_port)
{
  Traffic* traffic = 0;
  Host* host = inHost();

  //debug
  /*printf("NBTCPClientSession::get_random_server(), now = %s, before inNet()->control, ctrl_msg = %d\n",
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

#define DEFAULT_REQ_SIZE 1000000
void NBTCPClientSession::start_on(NBTCPClientSessionContinuation* const caller) {
    if(!get_random_server(server_ip, server_port))
      return; // stop if something's wrong

    // TODO: Implement request processing to work with tcp_client, for now, DEFAULT_REQ_SIZE
    // Note: This request behavior is only compatible with blocking_tcp_client.*
    int ssock = sm->socket();

	NBTCPClientSessionContinuation* cnt;
    if(caller == NULL) {
      cnt = new NBTCPClientSessionContinuation(this,ssock);
    } else {
      cnt = caller;
      cnt->client_socket = ssock;
    }
    cnt->p_state = NBTCPClientSessionContinuation::NB_TCP_CLIENT_SESSION_INIT;

    if(!sm->bind(ssock, IPADDR_ANYDEST, client_port, TCP_PROTOCOL_NAME, 0)) {
        TCP_DUMP(printf("start_on() on host \"%s\": failed to bind.\n",
                inHost()->nhi.toString()));
        return;
    }

	non_blocking_loop(cnt);
}

void NBTCPClientSession::non_blocking_loop(NBTCPClientSessionContinuation* const caller) {
        caller->p_state = NBTCPClientSessionContinuation::NB_TCP_CLIENT_SESSION_MAIN_LOOP;
        caller->status = NBTCPClientSessionContinuation::NB_TCP_CLIENT_STATUS_IDLE; // see comment below
        int res = sm->connect(caller->client_socket, server_ip, server_port, (NBSocketContinuation*)caller);
        if(res == NBSocketMaster::ESUCCESS) {
            connected = true;
        }

        if(connected && file_size > bytes_received) {
            int to_recv = file_size-bytes_received;
        	res = sm->recv(caller->client_socket, to_recv, 0, (NBSocketContinuation*)caller);
			if(res == NBSocketMaster::ESUCCESS) {
            	bytes_received += caller->retval;
				TCP_DUMP(printf("[host=\"%s\"] received %d of %d bytes\n",
                        inHost()->nhi.toString(),bytes_received,file_size));
                /* This is a convoluted (and arguably very bad) way to do receive looping. It
                 * DOES, however, mirror the blocking socket's looping method. Ideally, this
                 * should just be called by caller->success() within recv(). Unfortunately,
                 * we cannot then predicate our looping on a successful receive, because success()
                 * would be called before we could see the return status, and with nonblocking
                 * sockets, we cannot guarantee that recv() will be called in the correct state.
                 * For example, recv() could be called before connect(), and we DO NOT want to set
                 * our continuation status to RECEIVING in that case.
                 */
                caller->status = NBTCPClientSessionContinuation::NB_TCP_CLIENT_STATUS_RECEIVING;
                caller->success();
            } else if (res != NBSocketMaster::EWOULDBLOCK) {
                TCP_DUMP(printf("[host=\"%s\"] encountered error %d\n",
                        inHost()->nhi.toString(),res));
            }
		}

        if(bytes_received >= file_size) {
            caller->status = NBTCPClientSessionContinuation::NB_TCP_CLIENT_STATUS_IDLE;
			sm->close(caller->client_socket, (NBSocketContinuation*)caller);
		}
}

void NBTCPClientSession::config(s3f::dml::Configuration *cfg)
{
  ProtocolSession::config(cfg);

  char* str = (char*)cfg->findSingle("client_port");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: NBTCPClientSession::config(), invalid PORT attribute.\n");
    client_port = atoi(str);
  }
  else client_port = DEFAULT_CLIENT_PORT;

  str = (char*)cfg->findSingle("file_size");
  if(!str || s3f::dml::dmlConfig::isConf(str))
    error_quit("ERROR: NBTCPClientSession::config(), missing or invalid FILE_SIZE attribute.\n");
  file_size = atoi(str);

  str = (char*)cfg->findSingle("SHOW_REPORT");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: NBTCPClientSession::config(), invalid SHOW_REPORT attribute.\n");
    if(!strcasecmp(str, "true")) show_report = true;
    else if(!strcasecmp(str, "false")) show_report = false;
    else error_quit("ERROR: NBTCPClientSession::config(), invalid SHOW_REPORT attribute (%s).\n", str);
  }
  else show_report = true;

  str = (char*)cfg->findSingle("server_list");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: BTCPClientSession::config(), invalid SERVER_LIST attribute.\n");
    server_list = str;
  }
  else server_list = "forTCP";


  TCP_DUMP(printf("[host=\"%s\"] config():\n"
		  "  client_port=%u, file_size=%u.\n",
		  inHost()->nhi.toString(),
		  client_port, file_size));
}

void NBTCPClientSession::init()
{
  ProtocolSession::init();

  server_ip = inHost()->getDefaultIP();
  if(IPADDR_INVALID == server_ip)
    error_quit("ERROR: NBTCPClientSession::init(), invalid IP address, the host's not connected to network.\n");

  sm = SOCKET_API;
  if(!sm)
  {
	  error_quit("NBTCPClientSession::init(), missing socket master on host \"%s\".\n", inHost()->nhi.toString());
  }
  TCP_DUMP(printf("[host=\"%s\"] init(), server_ip=\"%s\".\n",
		  inHost()->nhi.toString(), IPPrefix::ip2txt(server_ip)));

  Host* owner_host = inHost();
  start_timer_callback_proc = new Process( (Entity *)owner_host, (void (s3f::Entity::*)(s3f::Activation))&NBTCPClientSession::start_timer_callback);
  start_timer_ac = new ProtocolCallbackActivation(this);
  Activation ac (start_timer_ac);
  HandleCode h = inHost()->waitFor( start_timer_callback_proc, ac, 0, inHost()->tie_breaking_seed ); //currently the starting time is 0
}

int NBTCPClientSession::push(Activation msg, ProtocolSession* hi_sess, void* extinfo, size_t extinfo_size)
{
  error_quit("ERROR: NBTCPClientSession::push() should not be called.\n");
  return 1;
}

int NBTCPClientSession::pop(Activation msg, ProtocolSession* lo_sess, void* extinfo, size_t extinfo_size)
{
  error_quit("ERROR: NBTCPClientSession::pop() should not be called.\n");
  return 1;
}

void NBTCPClientSession::start_timer_callback(Activation ac)
{
  NBTCPClientSession* server = (NBTCPClientSession*)((ProtocolCallbackActivation*)ac)->session;
  server->start_on(NULL);
}

}; // namespace s3fnet
}; // namespace s3f

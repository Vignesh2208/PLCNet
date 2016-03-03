/**
 * \file nb_tcp_server.cc
 * \brief Source file for the NBTCPServerSession class.
 *
 * authors : Lenny Winterrowd
 */

#include "os/tcp/app/nb_tcp_server.h"
#include "os/socket/nb_socket_master.h"
#include <netinet/in.h>
#include "util/errhandle.h"
#include "net/host.h"
#include "net/network_interface.h"
#include "os/base/protocols.h"

//#define TCP_DEBUG
#ifdef TCP_DEBUG
#define TCP_DUMP(x) printf("TCPSVR: "); x
#else
#define TCP_DUMP(x)
#endif

#define DEFAULT_SERVER_PORT 10

namespace s3f {
namespace s3fnet {

S3FNET_REGISTER_PROTOCOL(NBTCPServerSession, "S3F.OS.TCP.test.NBTCPServer");

/* The continuation used in tcp server. */
class NBTCPServerSessionContinuation : public NBSocketContinuation {
 public:
  enum {
    NB_TCP_SERVER_STATUS_IDLE    = 0,
    NB_TCP_SERVER_STATUS_SENDING = 1
  };

  enum {
    NB_TCP_SERVER_SESSION_UNINITIALIZED = 0,
    NB_TCP_SERVER_SESSION_INIT          = 1,
    NB_TCP_SERVER_SESSION_MAIN_LOOP     = 2
  };

  NBTCPServerSessionContinuation(NBTCPServerSession* server, int ssock) :
    NBSocketContinuation(server), server_socket(ssock), status(0),
    p_state(NB_TCP_SERVER_SESSION_UNINITIALIZED) {
    reqbuf = new byte[server->request_size];
    assert(reqbuf);
	//setSCVal(4141); // For debugging identification
  }

  virtual ~NBTCPServerSessionContinuation() {
    delete[] reqbuf;
  }

  virtual const char* getType () {
    return "NBTCPServerSessionContinuation";
  }

  /* When a continuation returns successfully, this function will be
     called. Based on the current status of this continuation, we will
     decide what to do next. */
  virtual void success() {
    NBTCPServerSession* server = (NBTCPServerSession*)owner;
    TCP_DUMP(printf("NBTCPServerSessionContinuation::success() on host \"%s\": "
		      "status = %d\n",
		      server->inHost()->nhi.toString(),status));
    switch(status) {
      case NB_TCP_SERVER_STATUS_SENDING:
	    server->non_blocking_loop(this);
        break;
    default:
      //assert(0);
      break;
    }
  }

  /* When a continuation returns unsuccessfully, this function will be
     called to print out the error. */
  virtual void failure() {
    NBTCPServerSession* server = (NBTCPServerSession*)owner;
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
    NBTCPServerSession* server = (NBTCPServerSession*)owner;
	TCP_DUMP(printf("NBTCPServerSessionContinuation::resume() on host \"%s\": "
		      "p_state = %d\n",
		      server->inHost()->nhi.toString(),p_state));
	fflush(stdout);
    switch(p_state) {
      case NB_TCP_SERVER_SESSION_INIT:
        server->start_on(this);
        break;
      case NB_TCP_SERVER_SESSION_MAIN_LOOP:
        server->non_blocking_loop(this);
        break;
      default:
        assert(0);
        break;
    }
  }

 public:
  int server_socket; // server socket session on well-known port
  int status; // the status of this socket session
  int p_state; // the current program state (where to resume execution)
  byte* reqbuf; // buffer for storing request from client

  IPADDR client_ip; // client ip address of this socket session
  uint16 client_port; // client port number of this socket session
  int file_size; // number of bytes to be sent to the client
  int sent_bytes; // number of bytes already sent
};

void FileSender::sendFile(int fsize, int socket) {
    int nread; 
    int nwrite, i;

    /* Start sending it */
	mFileSize = fsize;
    mBytesSent = 0;
    mBufLen = 0;
    mBufUsed = 0;
    mSocket = socket;
    mState = SENDING;
}

int FileSender::handleIO(NBSocketMaster* sm, NBTCPServerSessionContinuation* cnt) {
    TCP_DUMP(printf("called handleIO() (socket=%d,mState=%d)\n",mSocket,mState));
    if(mState == IDLE)
        return 1;     /* nothing to do */

    /* If buffer empty, fill it */
    if(mBufUsed == mBufLen) {
        /* Get one (fake) chunk of the file*/
        mBufLen = mFileSize-mBytesSent;
/* We need to send the entire filesize at once with send(). Otherwise, we don't wake up properly.
 * This behavior is consistent with the blocking implementation too. Alternatively, we could add
 * a periodic callback to wake up this application and send more data.
        if(mBufLen > BUFSIZE) {
            mBufLen = BUFSIZE;
        }
 */
        if(mBufLen == 0) {
            /* All done; close the socket. */
            int res = sm->close(mSocket,cnt);
            TCP_DUMP(printf("close (#1) socket %d returned %d\n",mSocket,res));
            mState = IDLE;
            return 3;
        }
        mBufUsed = 0;
    }

    /* Send one chunk of the file */
    assert(mBufLen > mBufUsed);
    int res = sm->send(mSocket, mBufLen-mBufUsed, 0, (NBSocketContinuation*)cnt);
    //nwrite = write(mSocket, mBuf + mBufUsed, mBufLen - mBufUsed);
    if(res == NBSocketMaster::ESUCCESS) {
        if(cnt->retval < 0) {
            TCP_DUMP(printf("send failed (error %d, retval = %d)!\n",res,cnt->retval));
        } else {
            TCP_DUMP(printf("%d bytes of %d bytes sent\n",mBytesSent+cnt->retval,mFileSize));
            mBufUsed += cnt->retval;
            mBytesSent += cnt->retval;
        }
    } else if(res != NBSocketMaster::EWOULDBLOCK) {
        TCP_DUMP(printf("send failed (error %d, retval = %d)!\n",res,cnt->retval));
    } else {
      return 2;
    }

    /* Due to lack of callbacks, we need this - with callbacks, the earlier check suffices */
    if(mBytesSent >= mFileSize) {
        /* All done; close the socket. */
        int res = sm->close(mSocket,cnt);
            TCP_DUMP(printf("close (#2) socket %d returned %d\n",mSocket,res));
        mState = IDLE;
        return 3;
    }

    return 0;
}

NBTCPServerSession::NBTCPServerSession(ProtocolGraph* graph) :
  ProtocolSession(graph), accept_completed(true), start_timer_ac(0), start_timer_callback_proc(0)
{
  TCP_DUMP(printf("[host=\"%s\"] new tcp server session.\n", inHost()->nhi.toString()));
}

NBTCPServerSession::~NBTCPServerSession() {}

// 30k works in all tests; 1M can still be problematic
//#define DEFAULT_REQ_SIZE 30000
#define DEFAULT_REQ_SIZE 1000000
void NBTCPServerSession::start_on(NBTCPServerSessionContinuation* const caller) {
    // TODO: Implement request processing to work with tcp_client, for now, DEFAULT_REQ_SIZE
    // Note: This request behavior is only compatible with blocking_tcp_client.*
    int ssock = sm->socket();

	NBTCPServerSessionContinuation* cnt;
    if(caller == NULL) {
      cnt = new NBTCPServerSessionContinuation(this,ssock);
    } else {
      cnt = caller;
      cnt->server_socket = ssock;
    }
    cnt->p_state = NBTCPServerSessionContinuation::NB_TCP_SERVER_SESSION_INIT;

    if(!sm->bind(ssock, IPADDR_ANYDEST, server_port, TCP_PROTOCOL_NAME, 0)) {
        TCP_DUMP(printf("start_on() on host \"%s\": failed to bind.\n",
                inHost()->nhi.toString()));
        return;
    }

    /* Wait for connections, and send anyone who connects 
     * the fake payload.
     */
    nclients = 0;
	non_blocking_loop(cnt);
}

/* NOTE: Without callback, only makes one pass at a time - sends 1 block to each connected instead
of looping, but this mirrors the blocking behavior as well; TCP handles the wake-and-send behavior */
void NBTCPServerSession::non_blocking_loop(NBTCPServerSessionContinuation* const caller) {
        caller->p_state = NBTCPServerSessionContinuation::NB_TCP_SERVER_SESSION_MAIN_LOOP;
        caller->status = NBTCPServerSessionContinuation::NB_TCP_SERVER_STATUS_IDLE; // see last comment in function
        /* If we don't have a full set of clients already,
         * listen for a new connection.
         */
        if(nclients < MAXCLIENTS) {
            int res = sm->accept(caller->server_socket, true, (NBSocketContinuation*)caller, 0);
            if(res == NBSocketMaster::ESUCCESS) {
                int fd = caller->retval;
				if(fd != -1) {
                    /* Someone connected.  Send them the file */
                    TCP_DUMP(printf("[host=\"%s\"] new connection.\n", inHost()->nhi.toString()));
		            mFilesenders[nclients].sendFile(DEFAULT_REQ_SIZE,fd);
		            nclients++;
				}
            }
        }
		// TODO: Need to release filesenders at some point for reuse - exhausts after MAXCLIENTS
        /* Pump data out to all the connected clients */
        bool idle = true;
        for (int i=0; i<nclients; i++) {
            if(!mFilesenders[i].handleIO(sm,caller)) {
              idle = false;
            }
        }

        /* NOTE: Somewhat similar to nb_tcp_client, this could be done MUCH better. This looping
         * behavior, while correct, is convoluted and not at all elegant. */
        if(!idle) {
          caller->status = NBTCPServerSessionContinuation::NB_TCP_SERVER_STATUS_SENDING;
		  caller->success();
        }
}

void NBTCPServerSession::config(s3f::dml::Configuration *cfg)
{
  ProtocolSession::config(cfg);

  char* str = (char*)cfg->findSingle("port");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: NBTCPServerSession::config(), invalid PORT attribute.\n");
    server_port = atoi(str);
  }
  else server_port = DEFAULT_SERVER_PORT;

  str = (char*)cfg->findSingle("request_size");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: NBTCPServerSession::config(), invalid REQUEST_SIZE attribute.\n");
    request_size = atoi(str);
    if(request_size < sizeof(uint32))
      error_quit("ERROR: NBTCPServerSession::config(), REQUEST_SIZE must be larger than 4 (bytes).\n");
  }
  else request_size = sizeof(uint32);

  str = (char*)cfg->findSingle("client_limit");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: NBTCPServerSession::config(), invalid CLIENT_LIMIT attribute.\n");
    client_limit = atoi(str);
  }
  else client_limit = 0; // means infinite

  str = (char*)cfg->findSingle("SHOW_REPORT");
  if(str)
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: NBTCPServerSession::config(), invalid SHOW_REPORT attribute.\n");
    if(!strcasecmp(str, "true")) show_report = true;
    else if(!strcasecmp(str, "false")) show_report = false;
    else error_quit("ERROR: NBTCPServerSession::config(), invalid SHOW_REPORT attribute (%s).\n", str);
  }
  else show_report = true;


  TCP_DUMP(printf("[host=\"%s\"] config():\n"
		  "  server_port=%u, request_size=%u, client_limit=%u.\n",
		  inHost()->nhi.toString(),
		  server_port, request_size, client_limit));
}

void NBTCPServerSession::init()
{
  ProtocolSession::init();

  server_ip = inHost()->getDefaultIP();
  if(IPADDR_INVALID == server_ip)
    error_quit("ERROR: NBTCPServerSession::init(), invalid IP address, the host's not connected to network.\n");

  sm = SOCKET_API;
  if(!sm)
  {
	  error_quit("NBTCPServerSession::init(), missing socket master on host \"%s\".\n", inHost()->nhi.toString());
  }
  TCP_DUMP(printf("[host=\"%s\"] init(), server_ip=\"%s\".\n",
		  inHost()->nhi.toString(), IPPrefix::ip2txt(server_ip)));

  Host* owner_host = inHost();
  start_timer_callback_proc = new Process( (Entity *)owner_host, (void (s3f::Entity::*)(s3f::Activation))&NBTCPServerSession::start_timer_callback);
  start_timer_ac = new ProtocolCallbackActivation(this);
  Activation ac (start_timer_ac);
  HandleCode h = inHost()->waitFor( start_timer_callback_proc, ac, 0, inHost()->tie_breaking_seed ); //currently the starting time is 0
}

int NBTCPServerSession::push(Activation msg, ProtocolSession* hi_sess, void* extinfo, size_t extinfo_size)
{
  error_quit("ERROR: NBTCPServerSession::push() should not be called.\n");
  return 1;
}

int NBTCPServerSession::pop(Activation msg, ProtocolSession* lo_sess, void* extinfo, size_t extinfo_size)
{
  error_quit("ERROR: NBTCPServerSession::pop() should not be called.\n");
  return 1;
}

void NBTCPServerSession::start_timer_callback(Activation ac)
{
  NBTCPServerSession* server = (NBTCPServerSession*)((ProtocolCallbackActivation*)ac)->session;
  server->start_on(NULL);
}

}; // namespace s3fnet
}; // namespace s3f

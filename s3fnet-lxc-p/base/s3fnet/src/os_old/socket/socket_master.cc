/**
 * \file socket_master.cc
 * \brief Source file for the SocketMaster class.
 *
 * authors : Dong (Kevin) Jin, Lenny Winterrowd
 */

#include "os/socket/socket_master.h"
#include "util/errhandle.h"
#include "net/host.h"
#include "os/base/protocols.h"


#ifdef SOCK_DEBUG
#define SOCK_DUMP(x) printf("SOCK: "); x
#else
#define SOCK_DUMP(x)
#endif

namespace s3f {
namespace s3fnet {

S3FNET_REGISTER_PROTOCOL(SocketMaster, "S3F.OS.Socket.socketMaster");

SocketMaster::SocketMaster(ProtocolGraph* graph) : ProtocolSession(graph), new_sockid(0)
{
  SOCK_DUMP(printf("[host=\"%s\"] new socket master session.\n", inHost()->nhi.toString()));
}

SocketMaster::~SocketMaster()
{
  for(S3FNET_MAP(int, Socket*)::iterator iter = bound_socks.begin(); iter != bound_socks.end(); iter++)
  {
    // the sessions are reclaimed by the tcp/udp master; sever the
    // link here prevents the destructor of Socket class to double
    // reclaim the sessions.
    (*iter).second->session = 0;
    (*iter).second->release();
  }
}

void SocketMaster::init()
{  
  SOCK_DUMP(printf("[host=\"%s\"] init.\n", inHost()->nhi.toString()));
  ProtocolSession::init(); 

  if(strcasecmp(name, SOCKET_PROTOCOL_NAME))
    error_quit("ERROR: SocketMaster::init(), unmatched protocol name: \"%s\", "
	       "expecting \"SOCKET\".\n", name);
}

int SocketMaster::control(int ctrltyp, void* ctrlmsg, ProtocolSession* sess)
{
  SocketSignal* socksig = (SocketSignal*)ctrlmsg;
  Socket* mysocket = 0;
  if(ctrltyp == SOCK_CTRL_SET_SIGNAL ||
     ctrltyp == SOCK_CTRL_CLEAR_SIGNAL ||
     ctrltyp == SOCK_CTRL_DETACH_SESSION ||
     ctrltyp == SOCK_CTRL_REINIT ||
     ctrltyp == SOCK_CTRL_GET_APP_SESSION)
  {
    assert(socksig);
    S3FNET_MAP(int, Socket*)::iterator iter = bound_socks.find(socksig->sockid);
    if(iter != bound_socks.end()) mysocket = (*iter).second;
    if(!mysocket)
    {
      error_retn("WARNING: SocketMaster::control(), unknown socket: %d (type %d)\n", socksig->sockid, ctrltyp);
      return 1;
    }
  }

  if(ctrltyp == SOCK_CTRL_SET_SIGNAL)
  {
    SOCK_DUMP(printf("[host=\"%s\"] %ld: control(), SOCK_CTRL_SET_SIGNAL (before): socket=%d "
		     "(mask=%08x, state=%08x), signal=%08x, nbytes=%u.\n",
		     inHost()->nhi.toString(), inHost()->now(),
		     socksig->sockid, mysocket->mask, mysocket->state, 
		     socksig->signal, socksig->nbytes));

    if((socksig->signal & SocketSignal::DATA_AVAILABLE) ||
       (socksig->signal & SocketSignal::OK_TO_SEND))
    {
      mysocket->bytes_completed += socksig->nbytes;
    }

    // if there's something new that the state doesn't include
    if(~mysocket->state & socksig->signal)
    {
      mysocket->state |= socksig->signal; // include it
      // if the newly added is what we're waiting for
      if(socksig->signal & mysocket->mask)
      {
    	  // clear up the data available flag for next receive
        if(mysocket->state & SocketSignal::DATA_AVAILABLE)
          mysocket->state &= ~SocketSignal::DATA_AVAILABLE;

        SOCK_DUMP(printf("unblocking: signal=%08x, state=%08x\n", socksig->signal, mysocket->state));
        assert(mysocket->continuation && ((BSocketContinuation*)mysocket->continuation)->next_stage);
        void (SocketMaster::*fct)(int, BSocketContinuation*) = ((BSocketContinuation*)mysocket->continuation)->next_stage;
        ((BSocketContinuation*)mysocket->continuation)->next_stage = 0;
        (this->*fct)(socksig->sockid, ((BSocketContinuation*)mysocket->continuation));
      }
    }

    SOCK_DUMP(printf("[host=\"%s\"] %ld: control(), SOCK_CTRL_SET_SIGNAL (after): socket=%d "
		     "(mask=%08x, state=%08x), signal=%08x, nbytes=%u.\n",
		     inHost()->nhi.toString(), inHost()->now(),
		     socksig->sockid, mysocket->mask, mysocket->state, 
		     socksig->signal, socksig->nbytes));
    return 0;
  }
  else if(ctrltyp == SOCK_CTRL_CLEAR_SIGNAL)
  {
    SOCK_DUMP(printf("[host=\"%s\"] %ld: control(), SOCK_CTRL_CLEAR_SIGNAL (before): socket=%d "
		     "(mask=%08x, state=%08x), signal=%08x.\n",
		     inHost()->nhi.toString(), inHost()->now(),
		     socksig->sockid, mysocket->mask, mysocket->state, socksig->signal));
    mysocket->state &= ~socksig->signal;
    SOCK_DUMP(printf("[host=\"%s\"] %ld: control(), SOCK_CTRL_CLEAR_SIGNAL (after): socket=%d "
		     "(mask=%08x, state=%08x), signal=%08x.\n",
		     inHost()->nhi.toString(), inHost()->now(),
		     socksig->sockid, mysocket->mask, mysocket->state, socksig->signal));
    return 0;
  }
  else if(ctrltyp == SOCK_CTRL_DETACH_SESSION)
  {
    /*SOCK_DUMP(printf("[host=\"%s\"] %ld: control(), SOCK_CTRL_DETACH_SESSION: socket=%d.\n",
      inHost()->nhi.toString(), inHost()->now(), socksig->sockid));*/
    mysocket->session = 0;
    return 0;
  }
  else if(ctrltyp == SOCK_CTRL_REINIT)
  {
    /*SOCK_DUMP(printf("[host=\"%s\"] %ld: control(), SOCK_CTRL_REINIT: socket=%d.\n",
      inHost()->nhi.toString(), inHost()->now(), socksig->sockid));*/
    mysocket->session->reinit();
    mysocket->reinit();
    return 0;
  }
  else if(ctrltyp == SOCK_CTRL_GET_APP_SESSION)
  {
    /*SOCK_DUMP(printf("[host=\"%s\"] %ld: control(), SOCK_CTRL_GET_APP_SESSION: socket=%d.\n",
      inHost()->nhi.toString(), inHost()->now(), socksig->sockid));*/
    /*...*/
    return 0;
  }
  else return ProtocolSession::control(ctrltyp, ctrlmsg, sess);
}

int SocketMaster::push(Activation msg, ProtocolSession* hi_sess, void* extinfo, size_t extinfo_size)
{
  error_quit("ERROR: SocketMaster::push() should not be called.\n");
  return 1;
}

int SocketMaster::pop(Activation msg, ProtocolSession* lo_sess, void* extinfo, size_t extinfo_size)
{
  error_quit("ERROR: SocketMaster::pop() should not be called.\n");
  return 1;
}

int SocketMaster::socket()
{
  bool overlap = false;
  do
  { // go linearly till we find a socket id not used
    new_sockid++;
    if(new_sockid < 0)
    {
      new_sockid = 1; // start from one if wrapped around
      if(overlap) return -1;
      else overlap = true;
    }
  } while(bound_socks.find(new_sockid) != bound_socks.end() ||
	  unbound_socks.find(new_sockid) != unbound_socks.end());

  unbound_socks.insert(new_sockid);
  SOCK_DUMP(printf("[host=\"%s\"] %ld: socket(), new socket %d.\n",
		   inHost()->nhi.toString(), getNow(), new_sockid));
  return new_sockid;
}

bool SocketMaster::bind(int sock, IPADDR src_ip, uint16 src_port, char* protocol, uint32 options)
{
  SOCK_DUMP(printf("[host=\"%s\"] %ld: bind(): socket=%d, src_ip=\"%s\", src_port=%d, "
		   "prot=\"%s\", opt=%u.\n", inHost()->nhi.toString(),
		   inHost()->now(), sock, IPPrefix::ip2txt(src_ip), src_port,
		   ((protocol == 0) ? "<NULL>" : protocol), options));

  SessionMaster* proto = (SessionMaster*)inHost()->sessionForName(protocol);
  if(!proto)
  {
    SOCK_DUMP(printf("bind(), protocol %s not found on host \"%s\".\n",
		     ((protocol == 0) ? "<NULL>" : protocol), inHost()->nhi.toString()));
    return false;
  }

  Socket* mysocket = 0;
  S3FNET_MAP(int, Socket*)::iterator iter = bound_socks.find(sock);
  if(iter != bound_socks.end()) return false;

  S3FNET_SET(int)::iterator setiter = unbound_socks.find(sock);
  if(setiter != unbound_socks.end())
  {
    // create a new socket
    mysocket = new Socket(); 
    assert(mysocket);

    // get new session
    mysocket->session = proto->createSession(sock);
    assert(mysocket->session);

    // set the src in session
    if(!mysocket->session->setSource(src_ip, src_port))
    {
      mysocket->release();
      return false;
    }
      
    // move from unbound to bound socket list
    unbound_socks.erase(setiter);
    bound_socks.insert(S3FNET_MAKE_PAIR(sock, mysocket));
    return true;
  }
  else return false;
}

bool SocketMaster::listen(int sock, uint32 queuesize)
{
  SOCK_DUMP(printf("[host=\"%s\"] %ld: listen(): socket=%d, queuesize=%u.\n",
		   inHost()->nhi.toString(), inHost()->now(), sock, queuesize));
  Socket* mysocket = 0;
  S3FNET_MAP(int, Socket*)::iterator iter = bound_socks.find(sock);
  if(iter != bound_socks.end()) mysocket = (*iter).second;
  if(mysocket && mysocket->session && !mysocket->session->connected())
    return mysocket->session->listen();
  else return false;
}

bool SocketMaster::connected(int sock, IPADDR* ip, uint16* port)
{
  SOCK_DUMP(printf("[host=\"%s\"] %ld: connected(): socket=%d.\n",
		   inHost()->nhi.toString(), inHost()->now(), sock));
  Socket* mysocket = 0;
  S3FNET_MAP(int, Socket*)::iterator iter = bound_socks.find(sock);
  if(iter != bound_socks.end()) mysocket = (*iter).second;
  if(mysocket)
  {
    if(mysocket->session && mysocket->session->connected())
    {
      if(ip != 0) *ip = mysocket->session->dst_ip;
      if(port != 0) *port = mysocket->session->dst_port;
      SOCK_DUMP(printf("destination ip=\"%s\", port=%d.\n",
		       IPPrefix::ip2txt(mysocket->session->dst_ip),
		       mysocket->session->dst_port));
      return true;
    }
  }
  return false;
}

bool SocketMaster::eof(int sock)
{
  SOCK_DUMP(printf("[host=\"%s\"] %ld: eof(): socket=%d.\n",
		   inHost()->nhi.toString(), inHost()->now(), sock));
  Socket* mysocket = 0;
  S3FNET_MAP(int, Socket*)::iterator iter = bound_socks.find(sock);
  if(iter != bound_socks.end()) mysocket = (*iter).second;
  if(mysocket)
  {
    if((mysocket->state & Socket::SOCK_EOF) && 
       !(mysocket->state & Socket::DATA_AVAILABLE))
    {
      SOCK_DUMP(printf("true.\n"));
      return true;
    }
    return false;
  }
  SOCK_DUMP(printf("true.\n"));
  return true; // if we can't find the open socket, it's true
}

void SocketMaster::abort(int sock)
{
  SOCK_DUMP(printf("[host=\"%s\"] %ld: abort(): socket=%d.\n",
		   inHost()->nhi.toString(), inHost()->now(), sock));
  Socket* mysocket = 0;
  S3FNET_MAP(int, Socket*)::iterator iter = bound_socks.find(sock);
  if(iter != bound_socks.end()) mysocket = (*iter).second;
  if(mysocket)
  {
    if(mysocket->session) mysocket->session->abort();
    if(mysocket->is_active()) mysocket->die_slowly();
    else
    {
      // the destructor will call release method---which will detach
      // the socket session from the socket, remove the socket session
      // from the session master, and then reclaim the socket
      // session---before the socket is reclaimed
      mysocket->release();
      bound_socks.erase(sock);
    }
  }
}

void SocketMaster::block_till(int sock, uint32 mask, bool anysignal, BSocketContinuation* caller)
{
  Socket* mysocket = 0;
  S3FNET_MAP(int, Socket*)::iterator iter = bound_socks.find(sock);
  if(iter != bound_socks.end()) mysocket = (*iter).second;
  if(!mysocket) return;

  SOCK_DUMP(printf("[host=\"%s\"] %ld: block_till(): socket=%d (state=%08x), mask=%08x, any=%d.\n",
		   inHost()->nhi.toString(), inHost()->now(), sock, mysocket->state, mask, anysignal));

  if(!(mask & mysocket->state)) // no match
  {
    if(anysignal) mysocket->mask = 0xffffffff;
    else mysocket->mask = (mask & ~mysocket->state);
    SOCK_DUMP(printf("suspend: mask=%08x, state=%08x.\n", mysocket->mask, mysocket->state));
    mysocket->activate();
    mysocket->continuation = (SocketContinuation*)caller;
  }
  else // no blocking, continue to the next stage
  {
    assert(caller->next_stage);
    void (SocketMaster::*fct)(int, BSocketContinuation*) = caller->next_stage;
    caller->next_stage = 0;
    (this->*fct)(sock, caller);
  }
}

void SocketMaster::connect(int sock, IPADDR ip, uint16 port, BSocketContinuation* caller)
{
  assert(caller);
  Socket* mysocket = 0;
  S3FNET_MAP(int, Socket*)::iterator iter = bound_socks.find(sock);
  if(iter != bound_socks.end()) mysocket = (*iter).second;
  if(!mysocket || // if the socket is not bound
     !mysocket->session || // this shouldn't happen
     mysocket->session->connected() ||  // if the socket has been connected
     (mysocket->state & Socket::CONNECT_ACTIVE)) // if the socket is connecting
  {
    caller->failure();
    return;
  }

  SOCK_DUMP(printf("[host=\"%s\"] %ld: connect(): socket=%d (state=%08x), ip=\"%s\", port=%d.\n",
		   inHost()->nhi.toString(), inHost()->now(), sock, mysocket->state, IPPrefix::ip2txt(ip), port));

  // initiate the connection
  if(!mysocket->session->connect(ip, port))
  {
    caller->failure();
    return;
  }

  // set the connect active flag up
  mysocket->state |= Socket::CONNECT_ACTIVE;

  caller->next_stage = &SocketMaster::connect1;
  block_till(sock, Socket::OK_TO_SEND|Socket::CONN_RESET, false, caller);
}

void SocketMaster::connect1(int sock, BSocketContinuation* caller)
{
  assert(caller);

  Socket* mysocket = 0;
  S3FNET_MAP(int, Socket*)::iterator iter = bound_socks.find(sock);
  if(iter != bound_socks.end()) mysocket = (*iter).second;
  if(!mysocket) { caller->failure(); return; }

  mysocket->deactivate();
  mysocket->mask = 0;

  // flip the connect active flag
  mysocket->state &= ~Socket::CONNECT_ACTIVE;

  if(mysocket->has_died() && !mysocket->is_active())
  {
    mysocket->release();
    bound_socks.erase(sock);
    caller->failure();
    return;
  }

  // the socket could have been reset
  if(mysocket->state & Socket::CONN_RESET)
  {
    SOCK_DUMP(printf("[host=\"%s\"] %ld: connect(): socket=%d (state=%08x), reset.\n",
		     inHost()->nhi.toString(), inHost()->now(), sock, mysocket->state));
    mysocket->reinit();
    caller->failure();
    return;
  }

  SOCK_DUMP(printf("[host=\"%s\"] %ld: connect(): socket=%d (state=%08x), continuing.\n",
		   inHost()->nhi.toString(), inHost()->now(), sock, mysocket->state));
  caller->success();
}

void SocketMaster::accept(int sock, bool make_new_socket, BSocketContinuation *caller,
			  BSocketContinuation *first_conn_notify)
{
  assert(caller);

  Socket* mysocket = 0;
  S3FNET_MAP(int, Socket*)::iterator iter = bound_socks.find(sock);
  if(iter != bound_socks.end()) mysocket = (*iter).second;
  if(!mysocket || // if the socket is not bound
     !mysocket->session || // this shouldn't happen
     mysocket->session->connected() || // if the socket has been connected
     (mysocket->state & Socket::CONNECT_ACTIVE)) // if the socket is connecting
  {
    caller->failure();
    return;
  }

  SOCK_DUMP(printf("[host=\"%s\"] %ld: accept(), socket=%d (state=%08x), make_new_sock=%d.\n",
		   inHost()->nhi.toString(), inHost()->now(), sock, mysocket->state, make_new_socket));

  // initiate the passive listen
  if(!mysocket->session->listen())
  {
    caller->failure();
    return;
  }

  // flip the connect active flag
  mysocket->state |= Socket::CONNECT_ACTIVE;

  // we capture the first connection signal as an indication the
  // application process should fork a new process to handle the
  // incoming connection.

  if(make_new_socket) caller->next_stage = &SocketMaster::accept1a;
  else caller->next_stage = &SocketMaster::accept1b;
  caller->chain_continuation = first_conn_notify;
  block_till(sock, Socket::FIRST_CONNECTION|Socket::CONN_RESET, false, caller);
}

void SocketMaster::accept1a(int sock, BSocketContinuation* caller)
{
  assert(caller);

  Socket* mysocket = 0;
  S3FNET_MAP(int, Socket*)::iterator iter = bound_socks.find(sock);
  if(iter != bound_socks.end()) mysocket = (*iter).second;
  if(!mysocket) { caller->failure(); return; }

  mysocket->deactivate();
  mysocket->mask = 0;

  // flip the active connection flag
  mysocket->state &= ~Socket::CONNECT_ACTIVE;

  if(mysocket->has_died() && !mysocket->is_active())
  {
    mysocket->release();
    bound_socks.erase(sock);
    caller->failure();
    return;
  }

  if(mysocket->state & Socket::CONN_RESET)
  {
    SOCK_DUMP(printf("[host=\"%s\"] %ld: accept(): socket=%d (state=%08x), reset.\n",
		     inHost()->nhi.toString(), inHost()->now(), sock, mysocket->state));
    mysocket->reinit();
    caller->failure();
    return;
  }

  SOCK_DUMP(printf("[host=\"%s\"] %ld: accept(), socket=%d (state=%08x), first connection.\n",
		   inHost()->nhi.toString(), inHost()->now(), sock, mysocket->state));

  // notify first connection
  if(caller->chain_continuation)
    ((BSocketContinuation*)caller->chain_continuation)->success();

  // flip the connect active flag
  mysocket->state |= Socket::CONNECT_ACTIVE;

  caller->next_stage = &SocketMaster::accept2a;
  block_till(sock, Socket::OK_TO_SEND|Socket::CONN_RESET, false, caller);
}

void SocketMaster::accept1b(int sock, BSocketContinuation* caller)
{
  assert(caller);

  Socket* mysocket = 0;
  S3FNET_MAP(int, Socket*)::iterator iter = bound_socks.find(sock);
  if(iter != bound_socks.end()) mysocket = (*iter).second;
  if(!mysocket) { caller->failure(); return; }

  mysocket->deactivate();
  mysocket->mask = 0;

  // flip the active connection flag
  mysocket->state &= ~Socket::CONNECT_ACTIVE;

  if(mysocket->has_died() && !mysocket->is_active())
  {
    mysocket->release();
    bound_socks.erase(sock);
    caller->failure();
    return;
  }

  if(mysocket->state & Socket::CONN_RESET)
  {
    SOCK_DUMP(printf("[host=\"%s\"] %ld: accept(): socket=%d (state=%08x), reset.\n",
		     inHost()->nhi.toString(), inHost()->now(), sock, mysocket->state));
    mysocket->reinit();
    caller->failure();
    return;
  }

  SOCK_DUMP(printf("[host=\"%s\"] %ld: accept(), socket=%d (state=%08x), first connection.\n",
		   inHost()->nhi.toString(), inHost()->now(), sock, mysocket->state));

  // notify first connection
  if(caller->chain_continuation)
    ((BSocketContinuation*)caller->chain_continuation)->success();

  // flip the connect active flag
  mysocket->state |= Socket::CONNECT_ACTIVE;

  caller->next_stage = &SocketMaster::accept2b;
  block_till(sock, Socket::OK_TO_SEND|Socket::CONN_RESET, false, caller);
}

void SocketMaster::accept2a(int sock, BSocketContinuation* caller)
{
  assert(caller);

  Socket* mysocket = 0;
  S3FNET_MAP(int, Socket*)::iterator iter = bound_socks.find(sock);
  if(iter != bound_socks.end()) mysocket = (*iter).second;
  if(!mysocket) { caller->failure(); return; }

  mysocket->deactivate();
  mysocket->mask = 0;

  // flip the active connection flag
  mysocket->state &= ~Socket::CONNECT_ACTIVE;

  if(mysocket->has_died() && !mysocket->is_active())
  {
    mysocket->release();
    bound_socks.erase(sock);
    caller->failure(); return;
  }

  if(mysocket->state & Socket::CONN_RESET)
  {
    SOCK_DUMP(printf("[host=\"%s\"] %ld: accept(): socket=%d (state=%08x), reset.\n",
		     inHost()->nhi.toString(), inHost()->now(), sock, mysocket->state));
    mysocket->reinit();
    caller->failure(); return;
  }

  SOCK_DUMP(printf("[host=\"%s\"] %ld: accept(), socket=%d (state=%08x), continuing.\n",
		   inHost()->nhi.toString(), inHost()->now(), sock, mysocket->state));

  // create a new session for the original socket
  SocketSession* session = mysocket->session->getSessionMaster()->createSession(sock);
  assert(session);
  if(!session->setSource(mysocket->session->src_ip, mysocket->session->src_port))
  {
    delete session;
    caller->failure();
    return;
  }
  
  int new_sock = socket(); // unbound at first
  Socket* new_mysocket = new Socket();
  assert(new_mysocket);
  
  // copy the old socket to the new one and put it in the
  // bound_socks list
  new_mysocket->session = mysocket->session;
  new_mysocket->state   = mysocket->state;
  new_mysocket->session->setSocketID(new_sock);
  unbound_socks.erase(new_sock);
  bound_socks.insert(S3FNET_MAKE_PAIR(new_sock, new_mysocket));

  // assign the newly created session to old socket (old socket
  // remains in bound table) and reset its state
  mysocket->session = session; 
  mysocket->reinit();
  
  caller->retval = new_sock;
  caller->success();
  return;
}

void SocketMaster::accept2b(int sock, BSocketContinuation* caller)
{
  assert(caller);

  Socket* mysocket = 0;
  S3FNET_MAP(int, Socket*)::iterator iter = bound_socks.find(sock);
  if(iter != bound_socks.end()) mysocket = (*iter).second;
  if(!mysocket) { caller->failure(); return; }

  mysocket->deactivate();
  mysocket->mask = 0;

  // flip the active connection flag
  mysocket->state &= ~Socket::CONNECT_ACTIVE;

  if(mysocket->has_died() && !mysocket->is_active())
  {
    mysocket->release();
    bound_socks.erase(sock);
    caller->failure(); return;
  }

  if(mysocket->state & Socket::CONN_RESET)
  {
    SOCK_DUMP(printf("[host=\"%s\"] %ld: accept(): socket=%d (state=%08x), reset.\n",
		     inHost()->nhi.toString(), inHost()->now(), sock, mysocket->state));
    mysocket->reinit();
    caller->failure(); return;
  }

  SOCK_DUMP(printf("[host=\"%s\"] %ld: accept(), socket=%d (state=%08x), continuing.\n",
		   inHost()->nhi.toString(), inHost()->now(), sock, mysocket->state));

  caller->retval = sock;
  caller->success();
}

void SocketMaster::send(int sock, uint32 length, byte* msg, BSocketContinuation *caller)
{
  assert(caller);

  Socket* mysocket = 0;
  S3FNET_MAP(int, Socket*)::iterator iter = bound_socks.find(sock);
  if(iter != bound_socks.end()) mysocket = (*iter).second;
  if(!mysocket || !mysocket->session || !mysocket->session->connected())
  {
    caller->failure();
    return;
  }

  SOCK_DUMP(printf("[host=\"%s\"] %ld: send(), socket=%d (state=%08x), length=%u (%s).\n",
		   inHost()->nhi.toString(), inHost()->now(), sock, mysocket->state, length, msg?"real":"virtual"));

  // initiate the send
  mysocket->bytes_completed = mysocket->session->send(length, msg);
  if(mysocket->bytes_completed < 0)
  {
    caller->failure();
    return;
  }

  // if initially the message has been sent completely in all, no more
  // waiting is necessary; otherwise, one must wait for the OK_TO_SEND
  // (or CONN_RESET) signal to get the true number of bytes that have
  // completed the transmission
  if(mysocket->bytes_completed < length)
  {
    caller->next_stage = &SocketMaster::send1;
    block_till(sock, Socket::OK_TO_SEND|Socket::CONN_RESET, false, caller);
  }
  else
  {
    SOCK_DUMP(printf("[host=\"%s\"] %ld: send(), socket=%d (state=%08x), sent all %u bytes, immediately.\n",
		     inHost()->nhi.toString(), inHost()->now(), sock, mysocket->state, length));
    caller->retval = length;
    caller->success();
  }
}

void SocketMaster::send1(int sock, BSocketContinuation* caller)
{
  assert(caller);

  Socket* mysocket = 0;
  S3FNET_MAP(int, Socket*)::iterator iter = bound_socks.find(sock);
  if(iter != bound_socks.end()) mysocket = (*iter).second;
  if(!mysocket) { caller->failure(); return; }

  mysocket->deactivate();
  mysocket->mask = 0;

  if(mysocket->has_died() && !mysocket->is_active())
  {
    mysocket->release();
    bound_socks.erase(sock);
    caller->failure(); return;
  }

  if(mysocket->state & Socket::CONN_RESET)
  {
    SOCK_DUMP(printf("[host=\"%s\"] %ld: send(): socket=%d (state=%08x), reset.\n",
		     inHost()->nhi.toString(), inHost()->now(), sock, mysocket->state));
    mysocket->reinit();
    // we don't bail out here; we need to return the number of bytes
    // sent so far unless nothing's been sent
    if(mysocket->bytes_completed == 0)
    {
      caller->failure(); return;
    }
  }

  SOCK_DUMP(printf("[host=\"%s\"] %ld: send(), socket=%d (state=%08x), sent %u bytes.\n",
		   inHost()->nhi.toString(), inHost()->now(), sock, mysocket->state, mysocket->bytes_completed));
  caller->retval = mysocket->bytes_completed;
  caller->success();
}

void SocketMaster::recv(int sock, uint32 bufsiz, byte* buffer, BSocketContinuation *caller)
{
  assert(caller);

  Socket* mysocket = 0;
  S3FNET_MAP(int, Socket*)::iterator iter = bound_socks.find(sock);
  if(iter != bound_socks.end()) mysocket = (*iter).second;
  if(!mysocket || !mysocket->session || !mysocket->session->connected())
  {
    caller->failure();
    return;
  }

  SOCK_DUMP(printf("[host=\"%s\"] %ld: recv(), socket=%d (state=%08x), bufsiz=%u.\n",
		   inHost()->nhi.toString(), inHost()->now(), sock, mysocket->state, bufsiz));

  // initiate the receive; it's possible we received nothing at this stage
  mysocket->bytes_completed = mysocket->session->recv(bufsiz, buffer);
  if(mysocket->bytes_completed < 0)
  {
    caller->failure();
    return;
  }

  // if initially the message has been received completely in all, no
  // more waiting is necessary; otherwise, one must wait for the
  // SOCK_EOF or DATA_AVAILABLE (or CONN_RESET) signal to get the true
  // number of bytes that have completed the transmission
  if(mysocket->bytes_completed < bufsiz)
  {
    caller->next_stage = &SocketMaster::recv1;
    block_till(sock, Socket::SOCK_EOF|Socket::DATA_AVAILABLE|Socket::CONN_RESET, false, caller);
  }
  else
  {
    SOCK_DUMP(printf("[host=\"%s\"] %ld: send(), socket=%d (state=%08x), received all %u bytes, immediately.\n",
		     inHost()->nhi.toString(), inHost()->now(), sock, mysocket->state, bufsiz));
    caller->retval = bufsiz;
    caller->success();
  }
}

void SocketMaster::recv1(int sock, BSocketContinuation* caller)
{
  assert(caller);

  Socket* mysocket = 0;
  S3FNET_MAP(int, Socket*)::iterator iter = bound_socks.find(sock);
  if(iter != bound_socks.end()) mysocket = (*iter).second;
  if(!mysocket) { caller->failure(); return; }

  mysocket->deactivate();
  mysocket->mask = 0;

  if(mysocket->has_died() && !mysocket->is_active())
  {
    mysocket->release();
    bound_socks.erase(sock);
    caller->failure(); return;
  }

  if(mysocket->state & Socket::CONN_RESET)
  {
    SOCK_DUMP(printf("[host=\"%s\"] %ld: recv(): socket=%d (state=%08x), reset.\n",
		     inHost()->nhi.toString(), inHost()->now(), sock, mysocket->state));
    mysocket->reinit();
    // we don't bail out here; we need to return the number of bytes
    // received so far
    if(mysocket->bytes_completed == 0)
    {
      caller->failure(); return;
    }
  }

  if((mysocket->state & Socket::SOCK_EOF) && mysocket->bytes_completed == 0)
  {
    caller->failure(); return;
  }

  SOCK_DUMP(printf("[host=\"%s\"] %ld: recv(), socket=%d (state=%08x), received %u bytes.\n",
		   inHost()->nhi.toString(), inHost()->now(), sock, mysocket->state, mysocket->bytes_completed));
  caller->retval = mysocket->bytes_completed;
  caller->success();
}

void SocketMaster::close(int sock, BSocketContinuation *caller)
{
  assert(caller);

  Socket* mysocket = 0;
  S3FNET_MAP(int, Socket*)::iterator iter = bound_socks.find(sock);
  if(iter != bound_socks.end()) mysocket = (*iter).second;
  if(!mysocket)
  {
    caller->failure();
    return;
  }

  SOCK_DUMP(printf("[host=\"%s\"] %ld: close(), socket=%d (state=%08x).\n",
		   inHost()->nhi.toString(), inHost()->now(), sock, mysocket->state));

  if(mysocket->session && mysocket->session->connected())
  {
    // proceed to disconnect
    mysocket->session->disconnect();
      
    caller->next_stage = &SocketMaster::close1;
    block_till(sock, Socket::CONN_RESET|Socket::CONN_CLOSED|Socket::SESSION_RELEASED, false, caller);
  }
  else
  {
    SOCK_DUMP(printf("[host=\"%s\"] %ld: close(), socket=%d (state=%08x), not connected (no blocking).\n",
		     inHost()->nhi.toString(), inHost()->now(), sock, mysocket->state));

    mysocket->reinit();
    if(mysocket->is_active()) mysocket->die_slowly();
    else
    {
      // the destructor will call release method---which will detach
      // the socket session from the socket, remove the socket session
      // from the session master, and then reclaim the socket
      // session---before the socket is reclaimed
      mysocket->release();
      bound_socks.erase(sock);
    }
    caller->success();
  }
}

void SocketMaster::close1(int sock, BSocketContinuation* caller)
{
  assert(caller);

  Socket* mysocket = 0;
  S3FNET_MAP(int, Socket*)::iterator iter = bound_socks.find(sock);
  if(iter != bound_socks.end()) mysocket = (*iter).second;
  if(!mysocket) { caller->failure(); return; }

  mysocket->deactivate();
  mysocket->mask = 0;

  SOCK_DUMP(printf("[host=\"%s\"] %ld: close(), socket=%d (state=%08x), continuing.\n",
		   inHost()->nhi.toString(), inHost()->now(), sock, mysocket->state));

  if(mysocket->has_died() && !mysocket->is_active())
  {
    mysocket->release();
    bound_socks.erase(sock);
    caller->failure(); return;
  }

  if(mysocket->session && (mysocket->state & Socket::SESSION_RELEASED))
  {
    mysocket->session->release();
  }

  mysocket->reinit();
  if(mysocket->is_active()) mysocket->die_slowly();
  else
  {
    // the destructor will call release method---which will detach
    // the socket session from the socket, remove the socket session
    // from the session master, and then reclaim the socket
    // session---before the socket is reclaimed
    mysocket->release();
    bound_socks.erase(sock);
  }
  caller->success();
}

}; // namespace s3fnet
}; // namespace s3f

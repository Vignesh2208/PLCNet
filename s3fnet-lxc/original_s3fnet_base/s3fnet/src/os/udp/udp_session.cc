/**
 * \file udp_session.cc
 * \brief Source file for the UDPSession class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "os/udp/udp_session.h"
#include "util/errhandle.h"
#include "net/host.h"
#include "os/base/data_message.h"
#include "os/udp/udp_message.h"
#include "os/ipv4/ip_interface.h"

#ifdef UDP_DEBUG
#define UDP_DUMP(x) printf("UDPSESS: "); x
#else
#define UDP_DUMP(x)
#endif

namespace s3f {
namespace s3fnet {

UDPSession::UDPSession(UDPMaster* master, int sock) :
  SocketSession(S3FNET_PROTOCOL_TYPE_UDP),
  udp_master(master), socket(sock), is_connected(false),
  rcvbuf_len(0), rcvbuf_offset(0),
  appl_rcvbuf(0), appl_rcvbuf_size(0), appl_data_rcvd(0)
{
  UDP_DUMP(printf("[host=\"%s\"] %s: new udp session.\n",
		  udp_master->inHost()->nhi.toString(),
		  udp_master->getNowWithThousandSeparator()));
}

UDPSession::~UDPSession()
{
  /*UDP_DUMP(printf("delete udp session on host \"%s\".\n",
    udp_master->inHost()->nhi.toString()));*/
}

bool UDPSession::connect(IPADDR destip, uint16 destport) 
{
  UDP_DUMP(printf("[host=\"%s\"] %s: connect(), socket %d, dest_ip=\"%s\", dest_port=%d.\n",
		  udp_master->inHost()->nhi.toString(),
		  udp_master->getNowWithThousandSeparator(), socket,
		  IPPrefix::ip2txt(destip), destport));

  // UDP is connectionless protocol; we simple set the destination ip
  // and port number to make a complete address of the peer
  dst_ip = destip;
  dst_port = destport;

  is_connected = true;
  wake_app(SocketSignal::OK_TO_SEND);
  return true;
}

void UDPSession::disconnect()
{
  UDP_DUMP(printf("[host=\"%s\"] %s: disconnect(), socket %d.\n",
		  udp_master->inHost()->nhi.toString(),
		  udp_master->getNowWithThousandSeparator(), socket));

  is_connected = false;
  wake_app(SocketSignal::CONN_CLOSED | SocketSignal::SESSION_RELEASED);
}

bool UDPSession::listen()
{
  UDP_DUMP(printf("[host=\"%s\"] %s: listen(), socket %d.\n",
		  udp_master->inHost()->nhi.toString(),
		  udp_master->getNowWithThousandSeparator(), socket));

  is_connected = true;
  wake_app(SocketSignal::FIRST_CONNECTION);
  return true;
}

bool UDPSession::connected()
{
  return is_connected;
}

bool UDPSession::setSource(IPADDR ip, uint16 port)
{
  UDP_DUMP(printf("[host=\"%s\"] %s: setSource(), socket %d, src_ip=\"%s\", src_port=%d.\n",
		  udp_master->inHost()->nhi.toString(),
		  udp_master->getNowWithThousandSeparator(), socket, IPPrefix::ip2txt(ip), port));

  src_ip   = ip;
  src_port = port;
  return true;
}

void UDPSession::abort()
{
  UDP_DUMP(printf("[host=\"%s\"] %s: abort(), socket %d.\n",
		  udp_master->inHost()->nhi.toString(),
		  udp_master->getNowWithThousandSeparator(), socket));

  wake_app(SocketSignal::CONN_RESET);
}

void UDPSession::release()
{
  /*UDP_DUMP(printf("[host=\"%s\"] %s: release(), socket %d.\n",
    udp_master->inHost()->nhi.toString(),
    udp_master->getNowWithThousandSeparator(), socket));*/

  // detach the udp session from the socket master
  SocketSignal socksig(socket, 0);
  udp_master->getSocketMaster()->control(SOCK_CTRL_DETACH_SESSION, (void*)&socksig, NULL);

  // then remove from the udp master
  udp_master->deleteSession(this);
}
  
void UDPSession::wake_app(int signal)
{
  UDP_DUMP(printf("[host=\"%s\"] %s: wake_app(), signal=%08x.\n",
		  udp_master->inHost()->nhi.toString(),
		  udp_master->getNowWithThousandSeparator(), signal));

  SocketSignal socksig(socket, signal);
  if(signal & SocketSignal::DATA_AVAILABLE)
  {
    socksig.nbytes = appl_data_rcvd;
    appl_rcvbuf = 0;
    appl_rcvbuf_size = 0;
  }
  udp_master->getSocketMaster()->control(SOCK_CTRL_SET_SIGNAL, &socksig, NULL);
}

void UDPSession::clear_app_state(int signal)
{
  UDP_DUMP(printf("[host=\"%s\"] %s: clear_app_state(), signal=%08x.\n",
		  udp_master->inHost()->nhi.toString(),
		  udp_master->getNowWithThousandSeparator(), signal));

  SocketSignal socksig(socket, signal);
  udp_master->getSocketMaster()->control(SOCK_CTRL_CLEAR_SIGNAL, &socksig, NULL);
}

int UDPSession::send(int length, byte* msg)
{
  int offset = 0;
  while(offset < length) // if there's anything left to be sent
  {
    int to_send = mymin(udp_master->max_datagram_size, length-offset);

    UDP_DUMP(printf("[host=\"%s\"] %s: send(length=%d), send a message of %d bytes, offset=%d.\n",
		    udp_master->inHost()->nhi.toString(),
		    udp_master->getNowWithThousandSeparator(), length, to_send, offset));

    DataMessage* dmsg;
    if(msg) dmsg = new DataMessage(to_send, &msg[offset], true);
    else dmsg = new DataMessage(to_send);
    offset += to_send;

    UDPMessage* udphdr = new UDPMessage(src_port, dst_port);
    udphdr->carryPayload(dmsg);
    Activation ac(udphdr);
    IPPushOption ops(src_ip, dst_ip, S3FNET_PROTOCOL_TYPE_UDP, DEFAULT_IP_TIMETOLIVE);
    udp_master->pushdown(ac, 0, (void*)&ops, sizeof(IPPushOption));
  }

  wake_app(SocketSignal::OK_TO_SEND);
  return length;
}

int UDPSession::recv(int length, byte* msg)
{
  assert(length > 0);
  uint32 got = generate(length, msg);
  if(got == 0)
  {
    // if no data available, the socket will be suspended waiting for
    // the DATA_AVAILABLE signal, we set the receive parameters so
    // that the socket will be notified when message arrives
    assert(appl_rcvbuf == 0 && appl_rcvbuf_size == 0);
    appl_rcvbuf = msg;
    appl_rcvbuf_size = length;

    clear_app_state(SocketSignal::DATA_AVAILABLE); 
  }
  return got;
}

void UDPSession::receive(UDPMessage* udphdr)
{
  DataMessage* dmsg = (DataMessage*)udphdr->dropPayload();
  assert(dmsg && dmsg->real_length > 0);

  rcvbuf.push_back(dmsg);
  rcvbuf_len += dmsg->realByteCount();

  UDP_DUMP(printf("[host=\"%s\"] %s: receive(), incoming %d bytes; order-to-fill %d bytes.\n",
		  udp_master->inHost()->nhi.toString(),
		  udp_master->getNowWithThousandSeparator(), dmsg->realByteCount(),
		  appl_rcvbuf_size));

  if(appl_rcvbuf_size > 0)
  {
    appl_data_rcvd = generate(appl_rcvbuf_size, appl_rcvbuf);
    if(appl_data_rcvd > 0)
    {
      UDP_DUMP(printf("[host=\"%s\"] %s: generated %d bytes.\n",
		      udp_master->inHost()->nhi.toString(),
		      udp_master->getNowWithThousandSeparator(), appl_data_rcvd));
      appl_rcvbuf = 0;
      appl_rcvbuf_size = 0; // reset receive parameters
      wake_app(SocketSignal::DATA_AVAILABLE);
    }
  }
}

int UDPSession::generate(int length, byte* msg)
{
  int rcvd = 0;
  // scan thru the received data blocks
  while(!rcvbuf.empty())
  {
    DataMessage* dmsg = rcvbuf.front();

    // data received not enough to fill application receive buffer
    if(dmsg->realByteCount()-rcvbuf_offset <= length)
    {
      int to_recv = dmsg->realByteCount()-rcvbuf_offset;
      if(msg && dmsg->payload)
      {
    	memcpy(&msg[rcvd], (byte*)dmsg->payload+rcvbuf_offset, to_recv);
      }
      rcvbuf_len -= to_recv;
      rcvbuf_offset = 0;
      rcvd += to_recv;
      rcvbuf.pop_front();
      delete dmsg;

      if(to_recv == length) break;
    }

    // the data message contains more bytes than needed by application
    else
    {
      int to_recv = length;
      if(msg && dmsg->payload)
      {
    	memcpy(&msg[rcvd], (byte*)dmsg->payload+rcvbuf_offset, to_recv);
      }
      rcvbuf_len -= to_recv;
      rcvbuf_offset += to_recv;
      rcvd += to_recv;
      break;
    }
  }
  return rcvd;
}

}; // namespace s3fnet
}; // namespace s3f

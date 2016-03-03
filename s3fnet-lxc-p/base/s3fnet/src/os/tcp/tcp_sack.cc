/**
 * \file tcp_sack.cc
 * \brief Methods implementing SACK in the TCPSession class.
 */

//
// The big picture is as follows:
//  
//
//                  a packet
//   Sender  ------------------------->  Receiver                          
//   * Handling:
//     - Sender: keep which packets are sent out  
//               sack enabled or disabled if SYN is set on
//     - Receiver: keep which packets are received
//
//
//                  a packet
//   Sender <--------------------------  Receiver
//   * Handling
//     - Receiver: in sack option, indicate which packets
//                 it has received
//     - Sender:   if in fast recovery stage, selective 
//                 retransmission
//

#include "os/tcp/tcp_session.h"
#include "util/errhandle.h"


#ifdef TCP_DEBUG
#define TCP_DUMP(x) printf("TCPSESS: "); x
#else
#define TCP_DUMP(x)
#endif

namespace s3f {
namespace s3fnet {

int TCPSession::sender_send_sack(int sack_flag, byte mask, byte*& options)
{
  // sender notifies the other side that sack is permitted.
  if(sack_flag == TCP_SACK_PERMITTED)
  {
    // TCP_SACK_PERMITTED when SYN is set,otherwise terminate the program.
    if(!(TCPMessage::TCP_FLAG_SYN & mask))
      error_quit("ERROR: TCPSession::sender_send_sack(), "
		 "TCP_SACK_PERMITTED must be used together with SYN mask.\n");
    
    // because the header should be a multiple of 32 bits, so 4 bytes are here.
    byte* sack_permitted_option = new byte[4];
    
    sack_permitted_option[0] = TCP_SACK_PERMITTED_KIND;         // Kind   = 4
    sack_permitted_option[1] = 2;                           // Length = 2
    options = sack_permitted_option;
    
    return 4;   // add 4 bytes.
  } else return 0;
}

int TCPSession::receiver_send_sack(int sack_flag, byte mask, uint32 ackno, byte*& options)
{
  // as RFC 2018, SACK options should be included in all ACKs which do not ACK
  // the highest seqnuence number in the data receiver's queue.
  if((TCP_SACK_OPTION == sack_flag) &&(ackno < rcvwnd->getHighestSeqno()))
  {
    // the first 2 bytes are empty. 2 -> Kind(5), length; 4 * (left, right)
    // important: make it's multiple of 32 bits.
    //      byte tcp_option[4+8*4];
    byte* tcp_option = new byte[4 + 8 * TCP_SACK_MAX_BLOCKS];
    
    // kind = 5
    tcp_option[0]  = TCP_SACK_OPTION_KIND;
    uint32 *buffer =  (uint32 *)&tcp_option[2];
    
    // before sending out attached sack blocks, remove lowest
    rcv_scoreboard->clear_blocks(ackno);
    
    // we can convey TCP_SACK_MAX_BLOCKS blocks
    int real_num = rcv_scoreboard->fetch_blocks(buffer, TCP_SACK_MAX_BLOCKS);
    
    // update the length of valid sack option. totally, there are real_num blocks.
    tcp_option[1] = 2 + 8 * real_num;
    
    // return added length
    // attention: 4 + 8 * real_num includes the 2 padding bytes, which make
    // the length be a multiple of 4 bytes.
    // data->insert_at_front(tcp_option, 4 + 8 * real_num);
    // return 1 + 2 * real_num;         // (1 + 2 * real_num) * (32 bits)
    //
    // [03/23/04] always carry the same size.
    options = tcp_option;
    return 4 + 8 * TCP_SACK_MAX_BLOCKS;
  }
  
  return 0;
}

void TCPSession::sender_recv_sack(TCPMessage* tmsg)
{
  // update the sack information obtained.
  if(tmsg->options && TCP_SACK_OPTION_KIND == tmsg->options[0])
  {
    TCP_DUMP(printf("sender_recv_sack(), SACK option set!\n"));
    assert(tcp_master->getVersion() == TCPMaster::TCP_VERSION_SACK);
    
    // first if ack advances, we update the mySack_Entries.
    if(tmsg->flags & TCPMessage::TCP_FLAG_ACK)
    {
    	snd_scoreboard->clear_blocks(tmsg->ackno);
    }
    
    // sack_length is 2 + 8 * num
    // int num = ((tcp_hdr_option_length(msg)-2)/8);
    assert(tmsg->options);
    int num = (tmsg->options[1] - 2) / 8;
    uint32 left,right;

    // now we sack the entries 
    for(int i=0; i<num; i++)
    {
      // 2  is (kind, length)
      left  = *((uint32 *)&tmsg->options[2 + 4 * 2 * i]);
      right = *((uint32 *)&tmsg->options[2 + 4 * (2 * i + 1)]);      
      snd_scoreboard->insert_block(left, right - left);
    }
  }
}

void TCPSession::receiver_recv_sack(TCPMessage* tmsg)
{
  if(tmsg->options && (TCP_SACK_PERMITTED_KIND == tmsg->options[0]))
    sack_permitted = true;  
  
  DataMessage* payload = (DataMessage*)tmsg->payload();
  if(!payload || !payload->realByteCount()) return;

  uint32 real_seqno  = tmsg->seqno;
  uint32 real_length = payload->realByteCount();
  
  if(rcv_scoreboard->is_new(&real_seqno, &real_length))
    rcv_scoreboard->insert_block(real_seqno, real_length);
}

bool TCPSession::sack_resend_segments(bool first_flag)
{
  // we only support fixed-sized packet length
  if(first_flag)  rxmit_seq = snd_scoreboard->unavailable(sndwnd->start());
  else rxmit_seq = snd_scoreboard->unavailable(rxmit_seq); 
  
  if(rxmit_seq < recover_seq)
  {
    // here note that rxmit_seq will be updated in resend_segments
    resend_segments(rxmit_seq, 1);
    return true;
  }
  
  // can't find a packet to be retransmitted.
  return false;
}

void TCPSession::sack_send_in_fast_recovery()
{
  bool found = true;
  while(sack_pipe < cwnd && found)
  {
    if(!sack_resend_segments(false)) found = false;
    else sack_pipe += mss;
  }
  
  if(!found && sndwnd->unused() >= mss)
  {
    sack_pipe += segment_and_send(sndwnd->firstUnused(), cwnd - sack_pipe);
  }
}

}; // namespace s3fnet
}; // namespace s3f

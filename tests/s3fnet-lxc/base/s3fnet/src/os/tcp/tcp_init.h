/**
 * \file tcp_init.h
 * \brief Default values for TCP.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __TCP_INIT_H__
#define __TCP_INIT_H__

//
// DML identifiers
//
#define TCP_DML_TCPINIT		"tcpinit"
#define TCP_DML_VERSION		"version"
#define TCP_DML_FAST_RECOVERY	"fast_recovery"
#define TCP_DML_ISS		"iss"
#define TCP_DML_MSS		"mss"
#define TCP_DML_RCVWNDSIZ	"rcvwndsize"
#define TCP_DML_SNDWNDSIZ	"sendwndsize"
#define TCP_DML_SNDBUFSIZ	"sendbuffersize"
#define TCP_DML_MAXRXMIT	"maxrexmittimes"
#define TCP_DML_SLOW_TIMEOUT	"tcp_slow_interval"
#define TCP_DML_FAST_TIMEOUT	"tcp_fast_interval"
#define TCP_DML_IDLE_TIMEOUT	"maxidletime"
#define TCP_DML_MSL		"msl"
#define TCP_DML_DELAYEDACK	"delayed_ack"
#define TCP_DML_MAXCONWND	"maxconwnd"
#define TCP_DML_BOOT_TIME	"boot_time"
#define TCP_DML_RTT_DUMP	"rttdump"
#define TCP_DML_CWND_DUMP	"cwnddump"
#define TCP_DML_REX_DUMP	"rexdump"
#define TCP_DML_SACK_DUMP	"sackdump"

//
// Default initialization constants
//
#define TCP_DEFAULT_VERSION	TCPMaster::TCP_VERSION_RENO
#define TCP_DEFAULT_ISS		1000
#define TCP_DEFAULT_MSS		1024 // RFC uses 536
#define TCP_DEFAULT_RCVWNDSIZ	128      
#define TCP_DEFAULT_SNDWNDSIZ	128
#define TCP_DEFAULT_SNDBUFSIZ	128
#define TCP_DEFAULT_MAXRXMIT	12
#define TCP_DEFAULT_SLOW_TIMEOUT	0.500
#define TCP_DEFAULT_FAST_TIMEOUT	0.200
#define TCP_DEFAULT_IDLE_TIMEOUT	600.00
#define TCP_DEFAULT_MSL		60.0
#define TCP_DEFAULT_DELAYEDACK	false
#define TCP_DEFAULT_MAXCONWND	0 // infinite
#define TCP_DEFAULT_INIT_THRESH	65536
#define TCP_DEFAULT_MAX_DUPACKS	3
#define TCP_DEFAULT_RXMIT_MIN_TIMEOUT	1.0
#define TCP_DEFAULT_RXMIT_MAX_TIMEOUT	64.0
#define TCP_DEFAULT_PERSIST_MIN_TIMEOUT	5.0
#define TCP_DEFAULT_PERSIST_MAX_TIMEOUT	60.0
#define TCP_DEFAULT_THRESH_FACTOR	2
#define TCP_DEFAULT_RTT_VAR_FACTOR	4

#define TCP_DEFAULT_RTT_SHIFT       3 
#define TCP_DEFAULT_RTTVAR_SCALE    16
#define TCP_DEFAULT_RTTVAR_SHIFT    2

#define TCP_SACK_NO             0x0000
#define TCP_SACK_PERMITTED      0x0001
#define TCP_SACK_OPTION         0x0002

#define TCP_SACK_PERMITTED_KIND 4
#define TCP_SACK_OPTION_KIND    5

#define TCP_SACK_MAX_BLOCKS     4 // 8*N+2<=40 => N<=4.

#define TCP_DEFAULT_MSL_TIMEOUT_FACTOR 2  // as per RFC 793
#define TCP_DEFAULT_BACKOFF_LIMIT TCP_DEFAULT_MAXRXMIT

#endif /*__TCP_INIT_H__*/

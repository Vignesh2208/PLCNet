/**
 * \file pktheader.h
 * \brief header file for the packet header parser
 */

#ifndef __PKT_HEADER_H__
#define __PKT_HEADER_H__

#include <netinet/in.h>

/* Ethernet addresses are 6 bytes */

#define ETHER_ADDR_LEN	6

/* ethernet headers are 14 bytes */
#define SIZE_ETHERNET 14

/* Ethernet header */
typedef struct sniff_ethernet {
	u_char ether_dhost[ETHER_ADDR_LEN];	// Destination host address
	u_char ether_shost[ETHER_ADDR_LEN];	// Source host address
	u_short ether_type;			        // IP? ARP? RARP? etc
} sniff_ethernet;

#endif /* __PKT_HEADER_H__ */


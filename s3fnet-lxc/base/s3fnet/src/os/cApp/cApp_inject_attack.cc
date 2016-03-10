/**
 * \file cApp_inject_attack.cc
 * \brief Source file for the cAppsession inject_attack method.
 *
 * authors : Vignesh Babu
 */

#include "os/cApp/cApp_session.h" // "s3fnet-lxc/base/s3fnet/src/os/cApp/cApp_session.h" or "inject_attack.h"


namespace s3f {
namespace s3fnet { 
/*
 // For reference

 class EmuPacket
 {
	public:

		int     len;                        // packet length in bytes
		ltime_t outgoingTime;				// unsigned long. do not modify
		ltime_t incomingTime;  				// unsigned long. do not modify

		u_short ethernetType;				// ethernetType - see inject_attack.h
		int     incomingFD;					// used internally by simulator - do not modify
		int     outgoingFD;					// used internally by simulator - do not modify

		unsigned char *data;                // packet payload pointer

		EmuPacket(int len);                 // contructor
		virtual ~EmuPacket();               // destructor
		EmuPacket* duplicate();             // generate an extra copy of this packet, for multicast
 };

*/ 

/*
 * called for each packet that is received by the compromised router. 
 * pkt->data = character buffer containing packet contents (entire ethernet message)
 * pkt->len  = len of packet in bytes
 * srcIP 	 = IP of generating PLC
 * dstIP 	 = IP of destination PLC/IDS
 * call sendPacket(pkt,srcIP,dstIP) after appropriate modification to forward the packet
 */
void cAppSession::inject_attack(EmuPacket * pkt, unsigned int srcIP, unsigned int destIP){


}

// called during initialization. Initialize any attack specific variables here.
void cAppSession::inject_attack_init(void){


}

};
};

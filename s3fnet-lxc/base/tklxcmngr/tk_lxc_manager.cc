/**
 * \file tk_lxc_manager.cc
 *
 * \brief LXC Manager
 *
 * authors : Vladimir Adam
 */

#include <s3f.h>
#include <string.h>

#include <sstream>
#include <iomanip>

#include <s3fnet/src/net/host.h>
#include <algorithm>

#include <errno.h>
#include <arpa/inet.h>
#include "pktheader.h"
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include "TimeKeeper_functions.h"
#include "utility_functions.h"
#include <fstream>
#include <cstdarg>


#define ETHER_TYPE_IP    (0x0800)
#define ETHER_TYPE_ARP   (0x0806)
#define ETHER_TYPE_8021Q (0x8100)
#define ETHER_TYPE_IPV6  (0x86DD)
#define PACKET_SIZE 1600

	//----------------------------------------------------------------------------------------------------------------------
	// 												LXC Manager Class Functions
	//----------------------------------------------------------------------------------------------------------------------

void* manageIncomingPacketsThreadHelper(void * ptr) { return ((LxcManager*)ptr)->manageIncomingPackets();     }
void* manageIncomingPacketsByTimelineThreadHelper(void * ptr) { 
	launchThreadInfo* lTI = (launchThreadInfo*)ptr;
	return lTI->lxcManager->manageIncomingPacketsByTimeLine(lTI->timelineID);     

}
void* manageInitTearDownLXCThreadHelper(void * ptr)
{
	launchThreadInfo* ltI = (launchThreadInfo*)ptr;
	return ltI->lxcManager->setUpTearDownLXCs(ltI->timelineID, ltI->typeFlag);

}

LxcManager::LxcManager(Interface* inf)
{
	pthread_mutex_init(&statistic_mutex, NULL);

	siminf = inf;

	dataPoints.clear();

	isSimulatorRunning      = false;
	totalAdvanceError       = 0;
	timesAdvanced           = 0;
	totalPacketInaccuracy   = 0;
	minimumAdvanceError     = 2000000000;
	maximumAdvanceError     = 0;
	listOfProxiesByTimeline = NULL;

	fpLogFile               = NULL;
	fpAdvanceErrorFile      = NULL;

	timesAdvancementWentOver  = 0;
	timesAdvancementWentUnder = 0;
	timesAdvancementExact     = 0;

	incomingThread = -1;
}

void LxcManager::init(char* outputDir)
{
	struct timeval runTimestamp;
	gettimeofday(&runTimestamp, NULL);
	std::stringstream temp;
	temp << outputDir << "/" << runTimestamp.tv_sec;
	logFolder = temp.str();
	setupLog();

	// Initialize hash table of LXC proxies for quick lookup!
	unsigned int numTimelines = siminf->get_numTimelines();

	debugPrint("|==================================================\n");
	debugPrint("| LXC Manager Created\n");
	debugPrint("| Simulation has %d timelines\n", numTimelines);
	debugPrint("|==================================================\n");

	listOfProxiesByTimeline = new vector<LXC_Proxy*>*[numTimelines];
	for (unsigned int i = 0; i < numTimelines; i++)
	{
		listOfProxiesByTimeline[i] = new vector<LXC_Proxy*>();
		vectorOfTotalTimesSpentAdvancing.push_back(0);
		vectorOfHowManyTimesTimelineAdvanced.push_back(0);
		vectorOfHowManyTimesTimelineCalledProgress.push_back(0);
	}

	assert(vectorOfTotalTimesSpentAdvancing.size() == numTimelines);
}

LxcManager::~LxcManager() {}

	//----------------------------------------------------------------------------------------------------------------------
	// 												Thread Functions
	//----------------------------------------------------------------------------------------------------------------------

void* LxcManager::setUpTearDownLXCs(unsigned int timelineID, int typeFlag)
{
	vector<LXC_Proxy*>* proxiesOnTimeline = listOfProxiesByTimeline[timelineID];

	assert(typeFlag == START_LXCS || typeFlag == STOP_LXCS );

	for (unsigned int i = 0; i < proxiesOnTimeline->size(); i++)
	{
		LXC_Proxy* proxyOnTimeline = (*proxiesOnTimeline)[i];
		assert(proxyOnTimeline->timelineLXCAlignedOn->s3fid() == timelineID);

		if (typeFlag == START_LXCS) // SET UP
		{
			proxyOnTimeline->launchLXC();
			printf("[%u] Creating %s ", timelineID, proxyOnTimeline->lxcName);
		}
		else // TEAR DOWN
		{
			cout << timelineID << " Deleting " << proxyOnTimeline->lxcName << "\n";
			proxyOnTimeline->exec_LXC_command(LXC_STOP);
			close(proxyOnTimeline->fd);
			proxyOnTimeline->exec_LXC_command(LXC_DESTROY);
		}
	}
	return (void*)NULL;
}

void* LxcManager::manageIncomingPacketsByTimeLine(int timelineID)
{
	//cpu_set_t cpuset;
	//int cpu = 7;
	//CPU_ZERO(&cpuset);       //clears the cpuset
	//CPU_SET( cpu , &cpuset); //set CPU 2 on cpuset
	//sched_setaffinity(0, sizeof(cpuset), &cpuset);
	// while(1){}

	int numAdvancing = 0;
	int ret = 0;

	vector<LXC_Proxy*> proxiesBeingAdvanced;
	vector<LXC_Proxy*>* proxiesOnTimeline = listOfProxiesByTimeline[timelineID];

	if (proxiesOnTimeline->size() == 0){
		printf("No proxies on timeline = %d. Packet management thread not needed\n",timelineID);
		return 0;
	}


	struct pollfd ufds[proxiesOnTimeline->size()];
	char* packetBuffer = (char*)malloc(sizeof(char) * PACKET_SIZE);
	int j = 0;

	// Do not begin polling LXCs until the entire model has bee completely initialized
	while (isSimulatorRunning == false){}

	while(1)
	{
		if (isSimulatorRunning == false){
			printf("Incoming packet handler thread exited for timeLine %d\n",timelineID);
			break;
		}

		int fd = open("/dev/s3fserial0",0);

		for (unsigned int i = 0; i < proxiesOnTimeline->size(); i++)
		{
			//LXC_Proxy* proxy = listOfProxies[i];
			LXC_Proxy* proxy = (*proxiesOnTimeline)[i];
			s3f::s3fnet::Host* owner_host = (s3f::s3fnet::Host*)proxy->ptrToHost;

			assert(owner_host != NULL);
			ufds[i].fd = proxy->fd;
			ufds[i].events  = POLLIN;
			ufds[i].revents = 0;


			
			struct ioctl_conn_param ioctl_conn;
			int mask = 0;
			ltime_t temp_arrival_time = proxy->getElapsedTime();

			
			if(fd >= 0){

				/* Temporarily commented out for now */
				/*for(j = 0; j < 100; j++){
					ioctl_conn.owner_lxc_name[j] = '\0';
					ioctl_conn.dst_lxc_name[j] = '\0';
				}
				strcpy(ioctl_conn.owner_lxc_name,proxy->lxcName);
				mask = ioctl(fd,S3FSERIAL_GETACTIVECONNS,&ioctl_conn);	
				if(mask > 0){
					j = 0;
					while( j < 8){
						if( mask & (1 << j)){
							printf("Connection %d active on lxc : %s\n",j,proxy->lxcName);
							owner_host->inNet()->getTopNet()->injectSerialEvent(owner_host,temp_arrival_time,j);
						}
						j = j + 1;
					}
				}*/
				close(fd);

			}
			else{
				//debugPrint("Could not open s3fserial");
				close(fd);
			}
			


		}
		
		int ret = poll(ufds, proxiesOnTimeline->size(), 3500);
		
		if (ret == 0){
			continue; // no file descriptor has data ready to read

		} 

		// Get Poll Timestamp
		struct timeval selectTimestamp;
		gettimeofday(&selectTimestamp, NULL);
		long selectTimeMicroSec = selectTimestamp.tv_sec * 1000000 + selectTimestamp.tv_usec;

		if (ret < 0 && ret == EINTR){
			printf("EINTR\n");
			continue;
		}

		if (ret < 0){
			perror("LXC Manager Poll Error");
			exit(1);
		}

		handleIncomingPacket(packetBuffer, proxiesOnTimeline, selectTimeMicroSec, ufds);
	}

	printf("LXC Incoming Thread Finished\n");
	return (void*)NULL;
}

void* LxcManager::manageIncomingPackets()
{
	//cpu_set_t cpuset;
	//int cpu = 7;
	//CPU_ZERO(&cpuset);       //clears the cpuset
	//CPU_SET( cpu , &cpuset); //set CPU 2 on cpuset
	//sched_setaffinity(0, sizeof(cpuset), &cpuset);
	// while(1){}

	struct pollfd ufds[listOfProxies.size()];
	char* packetBuffer = (char*)malloc(sizeof(char) * PACKET_SIZE);
	int j = 0;

	// Do not begin polling LXCs until the entire model has bee completely initialized
	while (isSimulatorRunning == false){}

	while(1)
	{
		if (isSimulatorRunning == false) break;

		int fd = open("/dev/s3fserial0",0);

		for (unsigned int i = 0; i < listOfProxies.size(); i++)
		{
			LXC_Proxy* proxy = listOfProxies[i];
			s3f::s3fnet::Host* owner_host = (s3f::s3fnet::Host*)proxy->ptrToHost;

			assert(owner_host != NULL);
			ufds[i].fd = proxy->fd;
			ufds[i].events  = POLLIN;
			ufds[i].revents = 0;


			
			struct ioctl_conn_param ioctl_conn;
			int mask = 0;
			ltime_t temp_arrival_time = proxy->getElapsedTime();

			
			if(fd >= 0){

				/* Temporarily commented out for now */
				/*for(j = 0; j < 100; j++){
					ioctl_conn.owner_lxc_name[j] = '\0';
					ioctl_conn.dst_lxc_name[j] = '\0';
				}
				strcpy(ioctl_conn.owner_lxc_name,proxy->lxcName);
				mask = ioctl(fd,S3FSERIAL_GETACTIVECONNS,&ioctl_conn);	
				if(mask > 0){
					j = 0;
					while( j < 8){
						if( mask & (1 << j)){
							printf("Connection %d active on lxc : %s\n",j,proxy->lxcName);
							owner_host->inNet()->getTopNet()->injectSerialEvent(owner_host,temp_arrival_time,j);
						}
						j = j + 1;
					}
				}*/
				close(fd);

			}
			else{
				//debugPrint("Could not open s3fserial");
				close(fd);
			}
			


		}
		
		int ret = poll(ufds, listOfProxies.size(), 3500);
		
		if (ret == 0){
			continue; // no file descriptor has data ready to read

		} 

		// Get Poll Timestamp
		struct timeval selectTimestamp;
		gettimeofday(&selectTimestamp, NULL);
		long selectTimeMicroSec = selectTimestamp.tv_sec * 1000000 + selectTimestamp.tv_usec;

		if (ret < 0 && ret == EINTR){
			printf("EINTR\n");
			continue;
		}

		if (ret < 0){
			perror("LXC Manager Poll Error");
			exit(1);
		}

		handleIncomingPacket(packetBuffer, &listOfProxies, selectTimeMicroSec, ufds);
	}

	printf("LXC Incoming Thread Finished\n");
	return (void*)NULL;
}

	//----------------------------------------------------------------------------------------------------------------------
	// 												Other Functions
	//----------------------------------------------------------------------------------------------------------------------

void LxcManager::handleIncomingPacket(char* buffer, vector<LXC_Proxy*>* proxiesToCheck, ltime_t selectTime, struct pollfd* fdSet)
{
	// find out to see which FD got something
	for (unsigned int i = 0; i < proxiesToCheck->size(); i++)
	{
		LXC_Proxy* proxy = (*proxiesToCheck)[i];
		int incomingFD = proxy->fd;		// lets check this Proxy's FD if there is some data awaiting on it
		
		ltime_t arrivalTime = 0;
		ltime_t temp_arrival_time = 0;
		string lxc_file_path;
		long hash;
		int count;
		
		assert(incomingFD > 0);
		

		if(fdSet[i].revents & POLLIN)
		{
			// figure out the time taken from POLL to identifing which file descriptor is set.
			// currently only measures this difference.
			// TODO: optimize for case when thousands of LXCs
			
			
			lxc_file_path = string("/proc/") + string(HOOK_DIR) + string("/") + string(HOOK_FILE);
			

			assert(incomingFD == fdSet[i].fd);
			struct timeval selectTimestamp;
			gettimeofday(&selectTimestamp, NULL);
			int nread = cread(incomingFD, buffer, PACKET_SIZE);
			u_short ethT = 0;
			temp_arrival_time = proxy->getElapsedTime();

			

			std::pair<int, unsigned int> res = analyzePacket(buffer, nread, &ethT);
			unsigned int destIP = res.second;
			int packet_status   = res.first;
			arrivalTime = temp_arrival_time;

			if (packet_status == PACKET_PARSE_IGNORE_PACKET || packet_status ==  PACKET_PARSE_UNKNOWN_PACKET)
			{
				//debugPrint("Ignored\n");
				continue;
			}
			else if(packet_status == PARSE_PACKET_SUCCESS_ARP){

				arrivalTime = temp_arrival_time;
   			    proxy->last_arrival_time = arrivalTime;

			}
			else{

				
				
				if(load_lxc_latest_info(proxy->lxcName) == -1){ // Sends a command to Socket Hook.
					// some error
					arrivalTime = temp_arrival_time; 
				   	proxy->last_arrival_time = arrivalTime;

				}
				else{
				FILE * fptr;
			    char * line = NULL;
				size_t len = 0;
				int line_no = 1;
				long secElapsed;
				long microSecElapsed;
				long elapsedMicroSec;
				ssize_t read;
				char content_read[KERN_BUF_SIZE];
				int k = 0;
				for(k =0; k < KERN_BUF_SIZE; k++)
					content_read[k] = '\0';

			    fptr = fopen(lxc_file_path.c_str(), "r");

			    
				
				if(fptr == NULL){
				   //debugPrint("fptr is null, lxc_file_path = %s\n",lxc_file_path.c_str());
			       arrivalTime = temp_arrival_time; 
				   proxy->last_arrival_time = arrivalTime;
				}
				else{
				        
					fread(content_read,sizeof(char),KERN_BUF_SIZE,fptr);
					if(strcmp(content_read,"NULL") == 0){
					    //debugPrint("Using default method of timestamp estimation\n");
						arrivalTime = temp_arrival_time; 
		  			    proxy->last_arrival_time = arrivalTime;
					}
					else{
						line = content_read;
						
						
	   					while(line)
						{
						      char * nextLine = strchr(line, '\n');
						      if (nextLine) *nextLine = '\0';  // temporarily terminate the current line
						      
						      if(line_no == 1){
							 	//debugPrint("Secs : %lu\n",atol(line));
							 	secElapsed = atol(line) - proxy->simulationStartSec;
						      }
						      
   						      if(line_no == 2){
								//debugPrint("uSecs : %lu\n",atol(line));
								microSecElapsed = atol(line) - proxy->simulationStartMicroSec;
								break;
						      }
						      line_no ++;
						      
						      if (nextLine)*nextLine = '\n';  // then restore newline-char, just to be tidy    
						      line = nextLine ? (nextLine+1) : NULL;
						}
						elapsedMicroSec = secElapsed * 1000000 + microSecElapsed;
						
						if(elapsedMicroSec > temp_arrival_time) // some problem in the buffer read
							elapsedMicroSec = temp_arrival_time;
						//debugPrint("Elapsed Microsecs = %lu\n",elapsedMicroSec);
						if(line_no == 2)
							arrivalTime = elapsedMicroSec;
						else
							arrivalTime = temp_arrival_time;
						
						if(arrivalTime < proxy->last_arrival_time){
							//debugPrint("Arrival Time is less. Compensated. \n");
							proxy->last_arrival_time = temp_arrival_time;
							arrivalTime = temp_arrival_time;
						}
						else
							proxy->last_arrival_time = arrivalTime;

						
						
						
					}

				        fclose(fptr);
				        
				}

				}

				// disabling socket hook for now
				//arrivalTime = temp_arrival_time; 
				//proxy->last_arrival_time = arrivalTime;

				//debugPrint("Arrival Time : %lu\n",arrivalTime);
				if((arrivalTime % 1000) < 100 )
					printf("%lu milliseconds elapsed\n", arrivalTime/1000);
				//debugPrint("######################################\n");
				
			}
			
			
			
			
						
	
			
			long diff = (selectTimestamp.tv_sec * 1000000 + selectTimestamp.tv_usec) - selectTime;
			//long adjustedError = diff/proxy->TDF;

			if(arrivalTime == 0){
				arrivalTime = proxy->getElapsedTime();// - adjustedError;
			}
			
			// TODO: use std::map to optimize Lookup
			LXC_Proxy* destinationProxy = findDestProxy(destIP);
			if (destinationProxy == NULL)
			{
				printf("However no one to give it to.\n");
				cout << print_packet(buffer, nread);
			}
			else
			{
				//debugPrintFileOnly("from LXC %s, %s", proxy->lxcName, print_packet(buffer, nread).c_str());
				//TODO: needs to be critical section if this function called by individual timelines
				totalPacketInaccuracy += diff;

				EmuPacket* pkt = new EmuPacket(nread);
				memcpy(pkt->data, buffer, nread);
				pkt->incomingFD   = incomingFD;
				pkt->outgoingFD   = destinationProxy->fd;
				pkt->incomingTime = arrivalTime;
				pkt->ethernetType = ethT;

				assert(pkt->incomingFD != pkt->outgoingFD);

				s3f::s3fnet::Host* destHost = (s3f::s3fnet::Host*)proxy->ptrToHost;
				destHost->inNet()->getTopNet()->injectEmuEvent(destHost, pkt, destIP);
			}
		}
	}
}

LXC_Proxy* LxcManager::findDestProxy(unsigned int dstIP)
{
	for (unsigned int i = 0; i < listOfProxies.size(); i++)
	{
		LXC_Proxy* potentialDestinationProxy = listOfProxies[i];
		if (potentialDestinationProxy->intlxcIP == dstIP)
		{
			return potentialDestinationProxy;
		}
	}
	return NULL;
}

LxcManager* LxcManager::get_lxc_manager(Interface* inf)
{
	LxcManager* controller = new LxcManager(inf);
	return controller;
}

void LxcManager::syncUpLXCs()
{
	threadArray = new pthread_t[siminf->get_numTimelines()];
	#ifndef TAP_DISABLED
	//pthread_create(&incomingThread           , NULL, manageIncomingPacketsThreadHelper   , this);
	for (unsigned int i = 0; i < siminf->get_numTimelines(); i++)
	{
		launchThreadInfo* lti = new launchThreadInfo;
		lti->timelineID = i;
		lti->lxcManager = this;
		lti->typeFlag   =  START_LXCS;
		pthread_create(&threadArray[i], NULL, manageIncomingPacketsByTimelineThreadHelper, lti);
	}
	#endif

	long lxcTimestampSec;
	long lxcTimestampMicroSec;
	struct timeval tv_lxcTimestamp;

	printInfoAboutHashTable();

	debugPrint("|==================================================\n");
	debugPrint("| Creating and Launching LXCs                      \n");
	debugPrint("|==================================================\n\n");


	/*pthread_t* threadArray = new pthread_t[siminf->get_numTimelines()];
	// Attempt to parallelize
	for (unsigned int i = 0; i < siminf->get_numTimelines(); i++)
	{
		launchThreadInfo* lti = new launchThreadInfo;
		lti->timelineID = i;
		lti->lxcManager = this;
		lti->typeFlag   =  START_LXCS;
		pthread_create(&threadArray[i]           , NULL, manageInitTearDownLXCThreadHelper   , lti);
	}

	for (unsigned int i = 0; i < siminf->get_numTimelines(); i++)
		pthread_join(threadArray[i], NULL);*/

	for (unsigned int i = 0; i < listOfProxies.size(); i++)
	{
		LXC_Proxy* proxy = listOfProxies[i];
		proxy->launchLXC();
	}

	sleep(1);

	debugPrint("|==================================================\n");
	debugPrint("| Dilating and Adding LXCs to Experiment           \n");
	debugPrint("|==================================================\n\n");

	for (unsigned int i = 0; i < listOfProxies.size(); i++)
	{
		LXC_Proxy* proxy = listOfProxies[i];
		proxy->dilateLXCAndAddToExperiment();
	}

	debugPrint("|==================================================\n");
	debugPrint("| Calling SynchronizeAndFreeze. Syncing up LXCs\n");
	debugPrint("|==================================================\n\n");

	if (listOfProxies.size() != 0){
		synchronizeAndFreeze();
	}

	for (unsigned int i = 0; i < listOfProxies.size(); i++)
	{
		LXC_Proxy* proxy = listOfProxies[i];
		gettimepid(proxy->PID, &tv_lxcTimestamp, NULL);
		lxcTimestampSec      = tv_lxcTimestamp.tv_sec;
		lxcTimestampMicroSec = tv_lxcTimestamp.tv_usec;

		proxy->simulationStartSec      = lxcTimestampSec;
		proxy->simulationStartMicroSec = lxcTimestampMicroSec;

		//#ifdef LXC_INIT_DEBUG
		//ltime_t elapsedTime = proxy->getElapsedTime();
		debugPrint("| %s Frozen at [ %ld sec %ld usec ]\n", proxy->lxcName, lxcTimestampSec, lxcTimestampMicroSec);
		// debugPrint("| %s Frozen at %ld\n", proxy->lxcName, elapsedTime);
		//#endif
		// proxy->sendCommandToLXC();
	}
	debugPrint("\n|==================================================\n");

	// checking to make sure all are frozen at the same time
	for (unsigned int i = 0; i < listOfProxies.size(); i++)
	{
		LXC_Proxy* proxy = listOfProxies[i];
		assert(proxy->simulationStartSec      == lxcTimestampSec);
		assert(proxy->simulationStartMicroSec == lxcTimestampMicroSec);
	}
}

void LxcManager::insertProxy(LXC_Proxy* p)
{
	int timelineID = p->timelineLXCAlignedOn->s3fid();
	assert(timelineID >= 0 && timelineID < (int)siminf->get_numTimelines());
	listOfProxiesByTimeline[timelineID]->push_back(p);
	listOfProxies.push_back(p);
}

void LxcManager::printLXCstats()
{
	long inject_past_total_packets = 0;			// Total number of emulation packets injected into simulator's past
	long inject_past_total_error   = 0;			// Total error (sum of differences) during injection to past
	long inject_total_packets      = 0;			// Total number of emulation packets injected into simulator

	long send_total_packets        = 0;
	long send_total_error          = 0;

	for (unsigned int i = 0; i < listOfProxies.size(); i++)
	{
		LXC_Proxy* proxy = listOfProxies[i];

		double average = (double)proxy->totalPacketError/(double)proxy->packetsSentOut;

		double averageErrorInjectedPast    = (double)proxy->totalTimeInjectedIntoPast  /(double)proxy->packetsInjectedIntoPast;
		double averageErrorInjectedFuture  = (double)proxy->totalTimeInjectedIntoFuture/(double)proxy->packetsInjectedIntoFuture;

		inject_past_total_error   += proxy->totalTimeInjectedIntoPast;
		inject_past_total_packets += proxy->packetsInjectedIntoPast;

		send_total_packets     +=  proxy->packetsSentOut;
		send_total_error       +=  proxy->totalPacketError;

		inject_total_packets   += (proxy->packetsInjectedAtCorrectTime +
				                   proxy->packetsInjectedIntoFuture    +
				                   proxy->packetsInjectedIntoPast);

		#ifdef LXC_INDIVIDUAL_STATS
		debugPrint("\t|============================================================|\n");
		debugPrint("\t| LXC %s Stats\n", proxy->lxcName );
		debugPrint("\t| OUT | Packets Sent Early   : %ld\n" , proxy->packetsSentEarly );
		debugPrint("\t| OUT | Packets Sent Late    : %ld\n" , proxy->packetsSentLate );
		debugPrint("\t| OUT | Packets Sent On Time : %ld\n" , proxy->packetsSentOnTime );
		debugPrint("\t| OUT | Total Packets Sent   : %ld\n" , proxy->packetsSentOut );
		debugPrint("\t| OUT | Total Error          : %ld\n" , proxy->totalPacketError );
		debugPrint("\t| OUT | Average Error        : %.5f\n", average );
		debugPrint("\t|------------------------------------------------------------|\n");
		debugPrint("\t| IN  | Pkts injected to timeline's future : %ld\n" ,   proxy->packetsInjectedIntoFuture );
		debugPrint("\t| IN  | Total future error                 : %ld\n" , proxy->totalTimeInjectedIntoFuture );
		debugPrint("\t| IN  | Average future pkts error          : %.8f\n",         averageErrorInjectedFuture );
		debugPrint("\t|------------------------------------------------------------|\n");
		debugPrint("\t| IN  | Pkts injected to timeline's past   : %ld\n" ,   proxy->packetsInjectedIntoPast );
		debugPrint("\t| IN  | Total past error                   : %ld\n" , proxy->totalTimeInjectedIntoPast );
		debugPrint("\t| IN  | Average past pkts error            : %.8f\n",         averageErrorInjectedPast );
		debugPrint("\t| IN  | Pkts sent correctly                : %ld\n" , proxy->packetsInjectedAtCorrectTime);
		debugPrint("\t|============================================================|\n\n");
		#endif
	}

	double send_average_error       = (double)send_total_error        / (double)send_total_packets;
	double inject_past_average_time = (double)inject_past_total_error / (double)inject_past_total_packets;
	double average_advance_accuracy = (double)totalAdvanceError/(double)timesAdvanced;

	double averege_packet_timestamp_inaccuracy = (double)totalPacketInaccuracy/(double)inject_total_packets;

	// ROOT MEAN SQUARED CALCULATION
	double squaredTotal = 0;
	assert((long)dataPoints.size() == timesAdvanced);
	for (unsigned int j = 0; j < dataPoints.size(); j++)
	{
		fprintf(fpAdvanceErrorFile, "%ld,\n", dataPoints[j]);
		long advanceSquared = dataPoints[j] * dataPoints[j];
		squaredTotal += advanceSquared;
	}

	double rootMeanSquaredError = sqrt(squaredTotal/timesAdvanced);

	// STANDARD DEVIATION CALCULATION
	double variance = 0;
	double standardDeviation = 0;

	double stdDevTemp = 0;
	for (unsigned int j = 0; j < dataPoints.size(); j++)
	{
		double stdDiff = dataPoints[j] -  average_advance_accuracy;
		stdDevTemp += stdDiff * stdDiff;

	}
	variance = stdDevTemp/timesAdvanced;
	standardDeviation = sqrt(variance);

	debugPrint("|============================================================|\n");
	debugPrint("| Overall Stats\n");
	debugPrint("|------------------------------------------------------------|\n");
	debugPrint("| OUT | Total Pkts Sent to LXCs       : %ld\n" , send_total_packets );
	debugPrint("| OUT | Total Pkts error late to LXC  : %ld\n" , send_total_error  );
	debugPrint("| OUT | Avg Pkts error late to LXC    : %.8f\n", send_average_error );
	debugPrint("|------------------------------------------------------------|\n");
	debugPrint("| IN  | Total Pkts Injected to past   : %ld\n" , inject_past_total_packets );
	debugPrint("| IN  | Total Error Injected to past  : %ld\n" , inject_past_total_error);
	debugPrint("| IN  | Average past error            : %.8f\n", inject_past_average_time);
	debugPrint("|============================================================|\n");
	debugPrint("| TOTAL Advance ERROR        : %ld\n", totalAdvanceError );
	debugPrint("| TOTAL Times Advanced       : %ld\n", timesAdvanced );
	debugPrint("| TOTAL Times Over           : %ld\n", timesAdvancementWentOver );
	debugPrint("| TOTAL Times Under          : %ld\n", timesAdvancementWentUnder );
	debugPrint("| TOTAL Times Exact          : %ld\n", timesAdvancementExact );
	debugPrint("| Average Advance ERROR      : %.8f\n", average_advance_accuracy );
	debugPrint("|============================================================|\n");
	debugPrint("| TOTAL Pkt Timestamp ERROR  : %ld\n", totalPacketInaccuracy );
	debugPrint("| TOTAL Pkts Injected        : %ld\n", inject_total_packets );
	debugPrint("| Average Packet Inaccuracy  : %.8f\n", averege_packet_timestamp_inaccuracy );
	debugPrint("|============================================================|\n");
	debugPrint("| MIN Advance Error          : %ld\n", minimumAdvanceError);
	debugPrint("| MAX Advance Error          : %ld\n", maximumAdvanceError);
	debugPrint("| Advance Error RMS          : %.8f\n", rootMeanSquaredError);
	debugPrint("| Advance Error Variance     : %.8f\n", variance);
	debugPrint("| Advance Error Std Dev      : %.8f\n", standardDeviation);
	debugPrint("|============================================================|\n");

	#ifdef LOGGING
	for (unsigned int i = 0; i < listOfProxies.size(); i++)
	{
		string cmd  = "cp " +  string(PATH_TO_S3FNETLXC) + "/data/" + string(listOfProxies[i]->lxcName) + " " + logFolder ;
		system(cmd.c_str());
	}
	#endif

	unsigned int nT = siminf->get_numTimelines();

	debugPrint("\n|============================================================|\n");
	double totalSecondSpentAdvancing = 0;

	for (unsigned int ii = 0; ii < nT; ii++)
	{
		double seconds = vectorOfTotalTimesSpentAdvancing[ii] / 1e6;
		totalSecondSpentAdvancing += seconds;
		debugPrint("| Timeline %d advanced %ld times ( Progress %ld ) for a total of %f seconds\n",
				ii, vectorOfHowManyTimesTimelineAdvanced[ii], vectorOfHowManyTimesTimelineCalledProgress[ii], seconds);
	}
	debugPrint("|============================================================|\n");
	debugPrint("| Cumulative emulation seconds %f\n", totalSecondSpentAdvancing);
	debugPrint("| Simulation run time is %g seconds\n", siminf->sim_exc_time()/1e6);
	debugPrint("| Total run time is %g seconds\n", siminf->full_exc_time()/1e6);
	debugPrint("|============================================================|\n");
}

bool LxcManager::advanceLXCsOnTimeline(unsigned int timelineID, ltime_t timeToAdvanceTo)
{
	// Keep track of how many LXCs need to be advanced
	int numAdvancing = 0;
	int ret = 0;

	vector<LXC_Proxy*> proxiesBeingAdvanced;
	vector<LXC_Proxy*>* proxiesOnTimeline = listOfProxiesByTimeline[timelineID];

	//printf("Progress call made successfully. timeline = %d\n",timelineID);

	// this timeline does not have any proxies - don't advance it
	if (proxiesOnTimeline->size() == 0){
		//printf("No proxies on timeline = %d\n",timelineID);
		return false;
	}

	for (unsigned int i = 0; i < proxiesOnTimeline->size(); i++)
	{
		LXC_Proxy* proxyOnTimeline = (*proxiesOnTimeline)[i];

		ltime_t lxc_actual_vt          = proxyOnTimeline->getElapsedTime();
		ltime_t desired_vt             = timeToAdvanceTo;
		ltime_t time_needed_to_advance = desired_vt - lxc_actual_vt;

		#ifndef TAP_DISABLED
		//if (lxc_actual_vt > 8000 * timelineID)
			proxyOnTimeline->sendCommandToLXC();
		#endif

		// time_needed_to_advance is <= 0 and therefore theres no need to advance
		if (time_needed_to_advance <= 0)
			continue;

		if (time_needed_to_advance * proxyOnTimeline->TDF >= 10)
		{
			#ifdef ADVANCE_DEBUG
			debugPrint("%s (VT %ld) wants to advance to (VT %ld), on Timeline %u [%ld]\n",
						proxyOnTimeline->lxcName, lxc_actual_vt, desired_vt, timelineID, time_needed_to_advance);
			#endif
			numAdvancing++;
			assert(timelineID == proxyOnTimeline->timelineLXCAlignedOn->s3fid());
			proxyOnTimeline->advanceLXCBy(time_needed_to_advance);
			proxiesBeingAdvanced.push_back(proxyOnTimeline);
		}
		else //(time_needed_to_advance * proxyOnTimeline->TDF < 10)
		{
			//printf("accumulating time\n");
			//accumulate virtual time
			//leap(proxyOnTimeline->PID, time_needed_to_advance);
			//if (time_needed_to_advance > 0)
			//debugPrint("[Interval too small] %s (VT %ld) wants to advance to (VT %ld), on Timeline %u [%ld]\n",
			//		proxyOnTimeline->lxcName, lxc_actual_vt, desired_vt, timelineID, time_needed_to_advance);
		}
	}

	assert(numAdvancing == (int)proxiesBeingAdvanced.size());
	assert(numAdvancing <= (int)proxiesOnTimeline->size());

	if (numAdvancing == 0){
	 //printf("No proxies advancing on timeline = %d\n",timelineID);	
	 return false;
	}


	//debugPrint("Progress call made successfully. timeline = %d\n", timelineID);	
	unsigned long startTime = getWallClockTime();
	ret = progress((int)timelineID, PROGRESS_FLAG);		// 0 Don't FORCE | 1 FORCE
	//ret = progress((int)timelineID, 1);		// 0 Don't FORCE | 1 FORCE
	unsigned long finishTime = getWallClockTime();
	
	

	vectorOfHowManyTimesTimelineCalledProgress[timelineID]++;
	vectorOfTotalTimesSpentAdvancing[timelineID] += (finishTime - startTime);
	vectorOfHowManyTimesTimelineAdvanced[timelineID] += numAdvancing;

	// see how well the LXC advanced
	for (unsigned int i = 0; i < proxiesBeingAdvanced.size(); i++)
	{
		LXC_Proxy* proxyOnTimeline = proxiesBeingAdvanced[i];

		ltime_t lxc_actual_vt          = proxyOnTimeline->getElapsedTime();
		ltime_t desired_vt             = timeToAdvanceTo;
		ltime_t advanceDifference      = labs(lxc_actual_vt - desired_vt);

		if (advanceDifference > 100)
		{
			//debugPrint("[TL %u %s ], DESIRED TIME %ld | ACTUAL TIME %ld | DIFFERENCE %ld\n",
			//		  (timelineID), (proxyOnTimeline->lxcName), (desired_vt), (lxc_actual_vt), (advanceDifference));
			//debugPrint("[TL %u %s ], DESIRED TIME %ld | ACTUAL TIME %ld | DIFFERENCE %ld\n",
			//		  (timelineID), (proxyOnTimeline->lxcName), (desired_vt), proxyOnTimeline->getElapsedTime(), (advanceDifference));
		}

		if(advanceDifference > 1000){
			fix_timeline(timelineID);
		}

		#ifdef ADVANCE_DEBUG
			//debugPrint("[TL %u %s ], DESIRED TIME %ld | ACTUAL TIME %ld | DIFFERENCE %ld\n",
			//		 (timelineID), (proxyOnTimeline->lxcName), (desired_vt), (lxc_actual_vt), (advanceDifference));
		#endif

		pthread_mutex_lock(&statistic_mutex);

		if (lxc_actual_vt > desired_vt)
			timesAdvancementWentOver++;
		else if (lxc_actual_vt < desired_vt)
			timesAdvancementWentUnder++;
		else
			timesAdvancementExact++;

		totalAdvanceError += advanceDifference;

		if (advanceDifference < minimumAdvanceError)
			minimumAdvanceError = advanceDifference;

		if (advanceDifference > maximumAdvanceError)
			maximumAdvanceError = advanceDifference;

		dataPoints.push_back(advanceDifference);
		pthread_mutex_unlock(&statistic_mutex);
	}

	pthread_mutex_lock(&statistic_mutex);
	timesAdvanced += numAdvancing;
	pthread_mutex_unlock(&statistic_mutex);

	if(ret == -1){
		//debugPrint("Progress call returned with error. timeline = %d\n", timelineID);
	}
	else{
		//debugPrint("Progress call returned successfully. timeline = %d\n", timelineID);
	}

	reset(timelineID);
	return true;
}

LXC_Proxy* LxcManager::getLXCProxyWithNHI(string nhi)
{
	for (unsigned int i = 0; i < listOfProxies.size(); i++)
	{
		LXC_Proxy* potentialDestinationProxy = listOfProxies[i];
		if (potentialDestinationProxy->Nhi.compare(nhi) == 0)
		{
			return potentialDestinationProxy;
		}
	}
	printf("Attempted to get proxy of NHI (%s), that doesn't exist - please fix DML file\n", nhi.c_str());
	exit(0);
	return NULL;
}

	//-------------------------------------------------------------------------------------------------------
	// 										Helper Methods
	//-------------------------------------------------------------------------------------------------------

void LxcManager::printInfoAboutHashTable()
{
	//debugPrint("|==================================================\n");
	for (unsigned int i = 0; i < siminf->get_numTimelines(); i++)
	{
		vector<LXC_Proxy*>* proxies = listOfProxiesByTimeline[i];
		debugPrint(" ____________________________________________\n");
		debugPrint("|\n");
		debugPrint("| Printing out Info about Timeline %ld ...\n", i);
		debugPrint("|_____\n");
		if ((int)proxies->size() == 0)
			debugPrint("      | Timeline %d has has no LXC proxies\n", i);

		for (int j = 0; j < (int)proxies->size(); j++)
		{
			LXC_Proxy* prx = (*proxies)[j];
			debugPrint("      | Timeline %d has Proxy LXC %s\n", i, prx->lxcName);
		}
		debugPrint("      |______________________________________\n");
	}
}

std::pair<int, unsigned int> LxcManager::analyzePacket(char* pkt_ptr, int len, u_short* ethT)
{
	assert(ethT != NULL);
	sniff_ethernet* ether = (sniff_ethernet*)pkt_ptr;
	u_short ether_type    = ntohs(ether->ether_type);
	unsigned short sum;

	int ether_offset     = 0;
	int dstIPAddr_offset = 0;
	int srcIPAddr_offset = 0;
	int parse_packet_type = PARSE_PACKET_SUCCESS_IP;

	struct icmphdr* hdr;
	struct iphdr* tk_ip_header;
	struct udphdr* tk_udp_header;
	struct tcphdr* tk_tcp_header;
	int is_tcp = 0;
	int is_udp = 0;

	u_short udp_src_port;
	(*ethT) = ether_type;

	switch(ether_type)
	{
		case ETHER_TYPE_IP:
			ether_offset = 14;
			dstIPAddr_offset = 16;
			srcIPAddr_offset = 12;
			hdr  = (struct icmphdr*)(pkt_ptr + ether_offset + 20);
			assert(hdr != NULL);
			//if      (hdr->type == ICMP_ECHO)      printf("REQUEST ---%hu----", ntohs(hdr->un.echo.sequence));
			//else if (hdr->type == ICMP_ECHOREPLY) printf("REPLY ---%hu----", ntohs(hdr->un.echo.sequence));

			// These statements deal with a packet that is sent out when an LXC is started as a DAEMON.
			// It try to send out some sort DHCP/ Bootstrap protocol to get an IP address (or something)
			// We ignore this packet and do not send it through the simulator.
			tk_ip_header = (struct iphdr*)(pkt_ptr + ether_offset);
			if (tk_ip_header->protocol == 0x11)	// UDP
			{
				tk_udp_header = (struct udphdr*)(pkt_ptr + ether_offset + 20);
				udp_src_port = ntohs(tk_udp_header->source);
				sum = (unsigned short)tk_udp_header->check;
				unsigned short udpLen = htons(tk_udp_header->len);
				is_udp = 1;
				
				// For dubugging
				//debugPrint("UDP Checksum = %d, UDP Len = %d\n",sum,udpLen);

				if (udp_src_port == 68)
					return pair<int, unsigned int>(PACKET_PARSE_IGNORE_PACKET, -1);
			}
			if (tk_ip_header->protocol == 0x06)	// TCP
			{
				is_tcp = 1;
				tk_tcp_header = (struct tcphdr*)(pkt_ptr + ether_offset + 20);
				

			}
			parse_packet_type = PARSE_PACKET_SUCCESS_IP;
			break;

		case ETHER_TYPE_8021Q:
			ether_offset = 18;
			assert(false);
			parse_packet_type = PACKET_PARSE_IGNORE_PACKET;
			break;

		case ETHER_TYPE_ARP:
			ether_offset = 14;
			dstIPAddr_offset = 24;
			srcIPAddr_offset = 14;
			parse_packet_type = PARSE_PACKET_SUCCESS_ARP;
			break;

		case ETHER_TYPE_IPV6:
			//printf("FOUND A IPV6 PACKET - WANT TO IGNORE\n");
			//cout << print_packet(pkt_ptr, len);
			//assert(false);
			return pair<int, unsigned int>(PACKET_PARSE_IGNORE_PACKET, -1);
			break;

		default:
			fprintf(stderr, "Unknown ethernet type, %04x %d, skipping...\n", ether_type, ether_type );
			cout << print_packet(pkt_ptr, len);
			assert(false);
			parse_packet_type = PACKET_PARSE_IGNORE_PACKET;
			break;
	}

	char* ptrToIPAddr = (pkt_ptr + ether_offset + dstIPAddr_offset);
	unsigned int dstIP = *(unsigned int*)(ptrToIPAddr);

	char* ptrToSrcIPAddr = (pkt_ptr + ether_offset + srcIPAddr_offset);
	unsigned int srcIP = *(unsigned int*)(ptrToSrcIPAddr);

	char buffer[20];
	const char* result=inet_ntop(AF_INET, &dstIP,buffer,sizeof(buffer));
	char buffer_2[20];
	const char* result_2 = inet_ntop(AF_INET, &srcIP,buffer_2,sizeof(buffer_2));
	const u_char *pkt = (const u_char *)(pkt_ptr);
	long hash = 0;
	hash = (long)packet_hash(pkt,len);
	
	// For dubugging


	if(is_tcp){
		//debugPrint("######################################\n");
		//debugPrint("\n~~~~~~~~~~~~~~~~~~TCP :	");
		//debugPrint("Ether Type: 0x%.4x\n", ether_type);
		//debugPrint("Src IP string: %s\n",result_2);
		//debugPrint("Dest IP string: %s\n", result);

		//debugPrint("TCP source port : %d\n", tk_tcp_header->source);
		//debugPrint("TCP dest port : %d\n", tk_tcp_header->dest);
		//debugPrint("TCP seq : %d\n", tk_tcp_header->seq);
		//debugPrint("TCP ack_seq : %d\n", tk_tcp_header->ack_seq);

		
		//debugPrint("TCP syn : %d\n", tk_tcp_header->syn);
		//debugPrint("TCP ack : %d\n", tk_tcp_header->ack);
		//debugPrint("TCP rst : %d\n", tk_tcp_header->rst);
		//debugPrint("TCP urg : %d\n", tk_tcp_header->urg);
		//debugPrint("TCP fin : %d\n", tk_tcp_header->fin);
		//debugPrint("TCP window : %d\n", tk_tcp_header->window);
		//debugPrint("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
	


	}

	if(is_udp){
		//debugPrint("######################################\n");
		//debugPrint("\n~~~~~~~~~~~~~~~~~~UDP :	");
		//debugPrint("Ether Type: 0x%.4x\n", ether_type);
		//debugPrint("Src IP string: %s\n",result_2);
		//debugPrint("Dest IP string: %s\n", result);
		//debugPrint("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");

	}
	//debugPrint("Hash = %d, pkt_len = %d\n",hash,len);
	const u_char * ptr = pkt;
	int i = 0;

	if(!is_tcp && !is_udp){

		//debugPrint("######################################\n");
		//if(parse_packet_type == PARSE_PACKET_SUCCESS_ARP)
		//	debugPrint("~~~~~~~~~~~~~~~~ARP~~~~~~~~~~~~~~~~~~\n");
		//else
		//	debugPrint("~~~~~~~~~~~~~~~~UNKNOWN~~~~~~~~~~~~~~~~~~\n");

		//debugPrint("Ether Type: 0x%.4x\n", ether_type);
		//debugPrint("Src IP string: %s\n",result_2);
		//debugPrint("Dest IP string: %s\n", result);
		//debugPrint("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");		

	}
	

	assert(dstIP > 0);
	return pair<int, unsigned int>(parse_packet_type, ntohl(dstIP));
}

string LxcManager::print_packet(char* pkt_ptr, int len)
{
	std::stringstream ss;
	ss << "Packet: ";
	for (int i = 0; i < len; i++)
	{
	    ss << hex << setw(2) << setfill('0') << (int)(unsigned char)pkt_ptr[i] << " ";
	}
	ss << "\n\n";
	return ss.str();
}

int LxcManager::cread(int fd, char *buf, int n)
{
	int nread;
	if ((nread = read(fd, buf, n)) < 0) {
		perror("Reading data");
		exit(1);
	}
	return nread;
}

int LxcManager::cwrite(int fd, char *buf, int n)
{
	int nwrite;
	if((nwrite=write(fd, buf, n)) < 0)
	{
	  perror("Writing data");
	  exit(1);
	}
	return nwrite;
}

void LxcManager::createFileWithLXCNames()
{
	ofstream myfile;
	string outFileStr = string(PATH_TO_S3FNETLXC) + string("/lxc-command/ListOfLXCS.txt");

	myfile.open (outFileStr.c_str());

	for (unsigned int i = 0; i < listOfProxies.size(); i++)
	{
		LXC_Proxy* proxy = listOfProxies[i];
		myfile << proxy->lxcName << "\n";
	}
	myfile.close();
}

void LxcManager::stopExperiment()
{
	for (unsigned int i = 0; i < siminf->get_numTimelines(); i++)
	{
		vector<LXC_Proxy*>* proxies = listOfProxiesByTimeline[i];

		double totalMicroseconds = 0;

		for (int j = 0; j < (int)proxies->size(); j++)
		{
			LXC_Proxy* prx = (*proxies)[j];
			long startTime = prx->simulationStartSec * 1e6 + prx->simulationStartMicroSec;
			long endTime   = startTime + prx->getElapsedTime();
			totalMicroseconds += (endTime - startTime);
		}

		debugPrint("Timeline %ld advanced its LXCs by %f seconds\n", i, totalMicroseconds/1000000.0);

	}

	debugPrint("|==================================================\n");
	debugPrint("| Calling STOP EXPERIMENT\n");
	debugPrint("|==================================================\n");
	stopExp();
}

void LxcManager::setupLog()
{
	struct timeval runTimestamp;
	gettimeofday(&runTimestamp, NULL);

	// Make folder which will contain relevant debug files
	std::stringstream cmd_MakeLogDirSS;
	string cmd_MakeLogDir = string("mkdir -p ") + logFolder ;
	system(cmd_MakeLogDir.c_str());

	string logFile          = logFolder + "/log.txt";
	string advanceErrorFile = logFolder + "/advanceErrorLog.txt";

	fpLogFile          = fopen(logFile.c_str(), "w" );
	fpAdvanceErrorFile = fopen(advanceErrorFile.c_str(), "w" );
}

void LxcManager::debugPrint(char* format, ...)
{
	// Print to Out File
	va_list argptr;
	va_start(argptr, format);
	vfprintf(fpLogFile, format, argptr);
	va_end(argptr);

	// Also print to STDOUT
	va_list temp;
	va_start(temp, format);
	vfprintf(stdout, format, temp);
	va_end(temp);

	fflush(stdout);

}

void LxcManager::debugPrintFileOnly(char* format, ...)
{
	// Print to Out File
	va_list argptr;
	va_start(argptr, format);
	vfprintf(fpLogFile, format, argptr);
	va_end(argptr);
}

unsigned long LxcManager::getWallClockTime()
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return (unsigned long)(tv.tv_sec * 1e6 + tv.tv_usec);
}

// for debugging purpose only
int LxcManager::packet_hash(const u_char * s,int size)
{

    //http://stackoverflow.com/questions/114085/fast-string-hashing-algorithm-with-low-collision-rates-with-32-bit-integer
    int hash = 0;
    const u_char * ptr = s;
    int i = 0;
    
    for(i = 0; i < size; i++)
    {
    	hash += *ptr;
    	hash += (hash << 10);
    	hash ^= (hash >> 6);

        ++ptr;
    }

    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);


    return hash;
}


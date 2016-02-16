/**
 * \file tk_lxc_manager.h
 *
 * \brief LXC Manager
 *
 * authors : Vladimir Adam
 */

#ifndef __TIMEKEEPER_LXCMNGR_H__
#define __TIMEKEEPER_LXCMNGR_H__

#ifndef __S3F_H__
#error "tk_lxc_manager.h can only be included by s3f.h"
#endif

#include <sys/time.h>
#include "lxc_proxy.h"
#include "socket_hooks/hook_defs.h"
#include <poll.h>



#define PACKET_PARSE_SUCCESS        0
#define PACKET_PARSE_UNKNOWN_PACKET 1
#define PACKET_PARSE_IGNORE_PACKET  2
#define PARSE_PACKET_SUCCESS_IP	    3
#define PARSE_PACKET_SUCCESS_ARP    4

class LXC_Proxy;

#define START_LXCS 100
#define STOP_LXCS  200


class LxcManager
{
	public:

	//-------------------------------------------------------------------------------------------------------
	// 										Constructors / Destructor
	//-------------------------------------------------------------------------------------------------------

		/*
		 * Initializes all member variables. Takes as Interface as paramater in case it needs more information
		 * about the model
		 */
		LxcManager(Interface* inf);

		/*
		 * TODO: deallocate all necessary allocated memory
		 */
		~LxcManager();

		/*
		 * Return one instant of the LXC Manager (called in api/interface.cc)
		 */
		static LxcManager* get_lxc_manager(Interface* inf);

		/*
		 * Initialize the directory that will contain logs after an experiment runs. Each run will be uniquely
		 * identified by a folder named with the number of seconds elapsed since epoch time.
		 * Creates the log files by getting the log director from the DML file (see s3fnet/s3fnet.cc).
		 *
		 * First an instance of LXC Manager is created. As soon as a SimInterface is called (which creates
		 * an instance of LXC Manager, this function is called to finish initializing the LXC Manager before
		 * S3FNet further parses the DML files and builds the model
		 */
		void init(char* outputDir);

		/*
		 * The thread which handles all incoming packets coming from LXCs
		 */
		pthread_t incomingThread;

	//-------------------------------------------------------------------------------------------------------
	// 										Information about LXC Manager
	//-------------------------------------------------------------------------------------------------------

		Interface* siminf;                                           // pointer to interface
		std::vector<LXC_Proxy*> listOfProxies;                       // vector of all proxies maintained by the LXC Manager
		vector<LXC_Proxy*>** listOfProxiesByTimeline;                // contains the list of Proxies by a timeline

		vector<long> vectorOfTotalTimesSpentAdvancing;               // vector where each element corresponds to a timeline and
		                                                             // how much total time a timeline called progress

		vector<long> vectorOfHowManyTimesTimelineAdvanced;           // vector where each element corresponds to a timeline and
		                                                             // the total times the LXC on it advanced

		vector<long> vectorOfHowManyTimesTimelineCalledProgress;     // vector where each element corresponds to a timeline and
		                                                             // the total times this timeline called progress(...)

		bool isSimulatorRunning;                                     // flag to see if the thread responsible for capturing LXCS
		                                                             // is running

		string  logFolder;                                           // path to the folder where information about each run will be stored
		FILE*   fpLogFile;                                           // file pointer to the log file
		FILE*   fpAdvanceErrorFile;                                  // file pointer to the file containing all advance errors

	//-------------------------------------------------------------------------------------------------------
	// 										Statistics Measurement
	//-------------------------------------------------------------------------------------------------------

		long   totalAdvanceError;
		long   timesAdvanced;
		long   totalPacketInaccuracy;
		vector<long> dataPoints;

		long   timesAdvancementWentOver;
		long   timesAdvancementWentUnder;
		long   timesAdvancementExact;

		long   minimumAdvanceError;
		long   maximumAdvanceError;

		pthread_mutex_t  statistic_mutex;
		void printLXCstats();

	//-------------------------------------------------------------------------------------------------------
	// 										MAIN THREAD FUNCTION
	//-------------------------------------------------------------------------------------------------------

		/*
		 * Contains the logic for the thread capturing LXC packets.
		 */
		void* manageIncomingPackets();

		/*
		 * Attempt at parallelizing the LXC creation. Currently unused due to the fact that sometimes, the LXC
		 * may not be fully created before calling gettimePID which would result in a PID of -1.
		 */
		void* setUpTearDownLXCs(unsigned int timelineID, int type);

	//-------------------------------------------------------------------------------------------------------
	// 										LXC Proxy Management
	//-------------------------------------------------------------------------------------------------------

		/*
		 * Inserts the a LXC Proxy into to list for LXC Manager use
		 */
		void insertProxy(LXC_Proxy* p);

		/*
		 * Returns the proxy that represents the LXC with a given IP address.
		 * Used after parsing a packet so that when it injected from a host with IP ad
		 */
		LXC_Proxy* findDestProxy(unsigned int dstIP);

		/*
		 * Advances LXCs on a given timeline to the absolute time timeToAdvance
		 */
		bool advanceLXCsOnTimeline(unsigned int id, ltime_t timeToAdvance);

		/*
		 *
		 */
		void syncUpLXCs();

		/*
		 *
		 */
		LXC_Proxy* getLXCProxyWithNHI(string nhi);

	//-------------------------------------------------------------------------------------------------------
	// 										Simulation Management
	//-------------------------------------------------------------------------------------------------------

		/*
		 * Calls stopExp() which cleans up the experiment. It also unfreezes all the LXCs. At this point, it is safe
		 * to rmmod the TimeKeeper module
		 */
		void stopExperiment();

		/*
		 * Subroutine inside manageIncomingPackets(...) which is called each tiem a poll(...) function returns. Depending on the
		 * file descriptor that has data ready to be read, this function, parses the packet from a particular LXC and injects
		 * it into the simulation.
		 */
		void handleIncomingPacket(char* buffer, vector<LXC_Proxy*>* proxiesToCheck, ltime_t selectTime, struct pollfd* fd );

	//-------------------------------------------------------------------------------------------------------
	// 										Helper Methods
	//-------------------------------------------------------------------------------------------------------

		/*
		 * Helper function which sets up the log files
		 */
		void setupLog();

		/*
		 * For each timeline in the simulation, this function prints out what LXCs are aligned on that timeline
		 */
		void printInfoAboutHashTable();

		/*
		 * Creates a file with all the LXC names in the case of a crash. That way, it is much easier to destroy the LXCs
		 * that do not get automatically cleaned up. This writes a file into s3fnet-lxc/lxc-command. For easy clean up
		 * do "sudo ./command ListOfLXCs", type exit, and hit enter.
		 */
		void createFileWithLXCNames();

		/*
		 * Prints the content of a captured packet. Useful for analysis. I recommend http://sadjad.me/phd/ and use that online tool
		 * to quickly decode packets
		 */
		string print_packet(char* pkt_ptr, int len);

		/*
		 * Analyzes a given packet and decides whether this packet should be ignored. For instance, if an IPv6 packet are ignored
		 */
		std::pair<int, unsigned int> analyzePacket(char* pkt_ptr, int len, u_short* etherType);

		/*
		 * Reads from file descriptor that goes through a TAP device
		 */
		int cread (int fd, char *buf, int n);

		/*
		 * Writes to a file descriptor that goes through a TAP device
		 */
		int cwrite(int fd, char *buf, int n);

		/*
		 * Prints into the log files fpLogFile and standard out
		 */
		void debugPrint(char* format, ...);

		/*
		 * Prints into the log files fpLogFile
		 */
		void debugPrintFileOnly(char* format, ...);

		/*
		 * Returns the total number of microsecond since epoch time
		 */
		unsigned long getWallClockTime();
		
		int packet_hash(const u_char * s,int size);
};

typedef struct threadInfo
{
	LxcManager*  lxcManager;
	unsigned int timelineID;

} threadInfo;

typedef struct launchThreadInfo
{
	LxcManager*  lxcManager;
	unsigned int timelineID;
	int typeFlag;

} launchThreadInfo;

#endif

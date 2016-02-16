/*! \mainpage Main Page
 *
 * Following ten years of experience using the Scalable Simulation Framework (SSF)
 * that supports large-scale parallel discrete-event simulation, we revisited its API,
 * making changes to better reflect use and support maintainability. S3F, the second-generation
 * API we have developed, has the following major improvements:
 *  - S3F uses only standard language and library features found in the GNU C++ compiler, g++.
 *  - Time advancement is broken up into epochs, between which control is released from the simulation threads to allow other activity (e.g., recalculation of received radio strength maps and location of mobile devices).
 *  - S3F gives a modeler extreme control over the ordering of process body executions.
 *
 *  A communication network testbed, S3FNet, has also been developed based on S3F, for large-scale system analysis and evaluation.
 *  Users can use emulation to represent the execution of critical software, and simulation to model an extensive ensemble of background computation and communication.
 *  The main features of S3FNet are summarized as follows:
 *  - Parallel discrete-event simulation
 *  - Virtual-machine-based (OpenVZ) emulation (embedded in virtual time instead of wall-clock time)
 *  - Distributed emulation over TCP/IP networking
 *  - Application lookahead (neural-network-based model)
 *  - Simulation and emulation of OpenFlow-based software-defined networks (SDN)
 *
 * <P> <B> Resources on S3F Project </B>
 * <BR> <a href="../base/index.html"> Development Manual of the Basic S3F Version (Simulation Only)</a>
 * <BR> <a href="../full/index.html"> Development Manual of the Advanced S3F Version (OpenVZ-based Emulation, OpenFlow)</a>
 * <BR> <a href="../usrman/index.html"> User Manual </a>
 * <BR> <a href="../publication.html"> Research Papers </a>
 * </P>
 *
 * <P> <B> Research Group and Contact </B>
 * <BR> Modeling of Security and Systems (MOSES) Research Group
 * <BR> Information Trust Institute
 * <BR> University of Illinois at Urbana-Champaign
 * <BR> Urbana, Illinois 61801, USA
 * <BR> Please contact Dong (Kevin) Jin at dongjin2@illinois.edu for further questions regarding S3F/S3FNet.
 * </P>
 *
 * <P><B> Copyright (c) 2010-2013 University of Illinois at Urbana-Champaign </B>
 * <BR> Permission is hereby granted, free of charge, to any individual or
 * institution obtaining a copy of this software and associated
 * documentation files (the "Software"), to use, copy, modify, and
 * distribute without restriction, provided that this copyright and
 * permission notice is maintained, intact, in all copies and supporting documentation.
 * </P>
 *
 * <P>This reference manual describes the classes and functions of the S3F/S3FNet implementation.
 * It is generated automatically from the source code using doxygen. </P>
 *
 */

/**
 * S3F: Illinois Simpler Scalable Simulation Framework
 *
 * \file s3f.h
 *
 * Copyright (c) 2010-2013 University of Illinois
 *
 * Permission is hereby granted, free of charge, to any individual or 
 * institution obtaining a copy of this software and associated 
 * documentation files (the "Software"), to use, copy, modify, and 
 * distribute without restriction, provided that this copyright and 
 * permission notice is maintained, intact, in all copies and supporting 
 * documentation.
 * 
 */

#ifndef __S3F_H__
#define __S3F_H__

/**
 *  \def COMPOSITE_SYNC
 *  whether the composite synchronization mechanism is used.
 *  the default synchronization mechanism is barrier-based global synchronous synchronization
 */
//#define COMPOSITE_SYNC

/* only one of following barrier should be enabled at one time */
#if defined(macintosh) || defined(Macintosh) || defined(__APPLE__) || defined(__MACH__)
	#define MUTEX_BARRIER ///< use mutex for sync barrier
	//#define SCHED_YIELD_BARRIER ///< use sched_yield for sync barrier
#elif defined(_WIN32) || defined(_WIN64)
	#define MUTEX_BARRIER ///< use mutex for sync barrier
	//#define SCHED_YIELD_BARRIER ///< use sched_yield for sync barrier
	//#define FAST_SCHED_YIELD_BARRIER ///< one fast sched_yield type barrier implemented by Lenhard Winterrowd, if this macro is defined, the original sched_yield_barrier is replaced. SCHED_YIELD_BARRIER must be enabled for FAST_SCHED_YIELD_BARRIER to function
	//#define TREE_BARRIER ///< software combining tree for large numbers of threads and/or distributed systems
#else
	//#define MUTEX_BARRIER ///< use mutex for sync barrier
	#define PTHREAD_BARRIER ///< use pthread_barrier for sync barrier
	//#define SCHED_YIELD_BARRIER ///< use sched_yield for sync barrier
	//#define FAST_SCHED_YIELD_BARRIER ///< one fast sched_yield type barrier implemented by Lenhard Winterrowd, if this macro is defined, the original sched_yield_barrier is replaced. SCHED_YIELD_BARRIER must be enabled for FAST_SCHED_YIELD_BARRIER to function
	//#define TREE_BARRIER ///< software combining tree for large numbers of threads and/or distributed systems
#endif

#if defined(MUTEX_BARRIER) + defined(PTHREAD_BARRIER) + defined(SCHED_YIELD_BARRIER) + defined(FAST_SCHED_YIELD_BARRIER) + defined(TREE_BARRIER) != 1
#error Define exactly one of the barrier macro in s3f.h
#endif

/**
 * \def SYNC_WIN_EVENT_COUNT
 * print out events per sync window per timeline at the end of the experiments
 */
//#define SYNC_WIN_EVENT_COUNT

/**
 * ltime_t is the virtual time data type used in S3F
 * -1 is sometimes used to flag an error when an ltime_t value is returned, or an ltime_t value to ignore.
 * So we do not use unsigned long for ltime_t
 */
typedef long ltime_t ;
typedef void* Object;

#include <iostream>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <queue>
#include <utility>
#include <string>
#include <sstream>
#include <tr1/memory>
#include <pthread.h>
#include <math.h>
#include <rng/rng.h>
#include <assert.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/resource.h>

using namespace std;

/**
 * \namespace s3f S3F Scalable Simulation Framework
 */
namespace s3f {
/**
 * s3f Core Classes
 */

class Entity;
class Process;
class InChannel;
class OutChannel;
class Timeline;
class Message;
class Interface;

#include <api/message.h>
#include <api/event.h>
#include <api/entity.h>
#include <api/process.h>
#include <api/inchannel.h>
#include <api/outchannel.h>
#include <time/time_management.h>
#include <aux/barrier_mutex.h>
#include <aux/fast_barrier.h>
#include <aux/fast_tree_barrier.h>
#include <aux/barrier.h>
#include <api/interface.h>
#include <api/timeline.h>
}

using namespace s3f;

#endif /*__S3F_H__*/

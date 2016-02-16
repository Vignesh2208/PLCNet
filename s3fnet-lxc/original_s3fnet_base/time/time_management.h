/**
 * \file time_management.h
 *
 * \brief S3F support for scheduling and executing discrete events
 */
#ifndef __TIME_MANAGEMENT_H__ 
#define __TIME_MANAGEMENT_H__

#ifndef __S3F_H__
#error "time_management.h can only be included by s3f.h"
#endif

/** include the Event Class */
#include <api/event.h>

/** include the virtual priority queue class */
#include <time/pq.h>

/** instantiate the priority queue class with and STL priority_queue */
#include <time/stl-eventlist.h>

#endif /* __TIME_MANAGEMENT_H__ */

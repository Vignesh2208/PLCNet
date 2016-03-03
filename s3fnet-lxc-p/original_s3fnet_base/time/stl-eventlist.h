/**
 * \file stl-eventlist.h
 * \brief Header file for S3F %event list, implemented using STL priority queue.
 */

#ifndef __STL_EVENTLIST_H__
#define __STL_EVENTLIST_H__

#ifndef __S3F_H__
#error "stl-eventlist.h can only be included by s3f.h"
#endif

/**
 * Compare two S3F events
 */
struct evt_Comparer
{
bool operator()(const EventPtr lhs, const EventPtr rhs) const
{
  if( lhs->__time > rhs->__time ) return true;
  if( lhs->__time < rhs->__time ) return false;
  if( lhs->__key2 > rhs->__key2)  return true;
  return false;
 }
};

/**
 * %Event list implementation using STL priority queue
 */
class STL_EventList : pq {
 public:
  STL_EventList() {}
  priority_queue<EventPtr, vector<EventPtr>, evt_Comparer> evtList;
  inline void push(EventPtr n)  { evtList.push(n);        }
  inline void pop()             { evtList.pop();          }
  inline bool     empty()       { return evtList.empty(); }
  inline int      size()       { return evtList.size();  }
  EventPtr    top()            { return evtList.top();   }
 };

#endif /* __STL_EVENTLIST_H */


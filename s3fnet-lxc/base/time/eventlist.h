/**
 * \file eventlist.h
 * \brief Header file for S3F %event list, implemented using priority queue.
 */

#ifndef __EVENTLIST_H__
#define __EVENTLIST_H__

#ifndef __S3F_H__
#error "eventlist.h can only be included by s3f.h"
#endif

/** S3F %event list implementation using priority queue */
class EventList : public pq {
 public:

  priority_queue<pq_node, vector<pq_node>, pq_Comparer> evtList;
  bool     empty()     { return evtList.empty(); }
  int      size()      { return evtList.size(); }
  pq_node  top()       { return evtList.top(); }
  void push(pq_node n) { evtList.push(n); }
  void pop()           { evtList.pop(); }

  vector<pq_node> pop_all() {
	vector<pq_node> allEvts;
	pq_node first, second;
        first = evtList.top();
	evtList.pop();
        allEvts.push_back(first);
	do {
	  second = evtList.top();
          if( first != second ) break;
	  allEvts.push_back(second);
	  evtList.pop();
	} while (1);
	return allEvts;
  }
 };
#endif /* __EVENTLIST_H__ */

/**
 * \file route_cache2.h
 * \brief Header file for the RouteCache2 class.
 *
 * authors : Lenny Winterrowd
 */

#ifndef __ROUTE_CACHE2_H__
#define __ROUTE_CACHE2_H__

#include "net/forwardingtable.h"
#include <cstring>

namespace s3f {
namespace s3fnet {
namespace routecache2 {

// Size 256 will use 6/8 MB (32/64 bit machine) for 2000 hosts
const unsigned int ROUTE_CACHE_SIZE = 256;
const unsigned int ASSOCIATIVITY = 2;
// mask generally == (ROUTE_CACHE_SIZE/ASSOCIATIVITY)-1 (for powers of 2)
const unsigned int INDEX_MASK = 0x7f;

class RouteCacheEntry {
  public:
	RouteCacheEntry() : valid(false) {};
	~RouteCacheEntry() {};

	bool valid;
	IPADDR ipaddr;
	RouteInfo* route;
	unsigned char ctr;
};

/**
 * \brief Associative Routing Cache
 *
 * Stores ROUTE_CACHE_SIZE entries in a ASSOCIATIVITY-way associative array.
 * ROUTE_CACHE_SIZE must divide ASSOCIATIVITY. For ASSOCIATIVITY == 1, use the RouteCache1 class.
 * The current (way) index is the last 7 bits of the IP addr.
 * Uses a pseudo-LFU replacement algorithm and stores the next entry to evict from each way in
 * mEvictor. This entry is is set to the first invalid entry encountered. Otherwise, it is evaluated
 * from min(way.entries.ctr) on cache misses. 
 * TODO: Enable dynamic size/assocativity (via constructor)? Scale with # of hosts?
 * TODO: Try LFU/LRU hybrid; ie on cache hits, if the hit item is the eviction target, the lowest
 * 		 found item replaces it in mEvictor.
 */
class RouteCache2 : public RouteCache {
  public:
	RouteCache2();

	~RouteCache2() {};

	/* Return a route for the given ipdaddr if it exists.
     * Returns NULL (0) otherwise.
     * Should be designed to be as fast as possible.
     */
	virtual RouteInfo* lookup(IPADDR ipaddr);

	/* Adds the given route to the cache. */
	virtual void update(IPADDR ipaddr, RouteInfo* route);

	/* Invalidates all entries in the cache.
	 * Necessary for proper functionality in most of
	 * ForwardingTables's methods.
	 */
	virtual void invalidate();

  protected:
	RouteCacheEntry mData[ROUTE_CACHE_SIZE];
	// need to promote type for ASSOCIATIVITY > 255 (which should be too slow regardless)
	unsigned char mEvictor[ROUTE_CACHE_SIZE/ASSOCIATIVITY];
};

}; // namespace routecache2
}; // namespace s3fnet
}; // namespace s3f

#endif /*__ROUTE_CACHE2_H__*/

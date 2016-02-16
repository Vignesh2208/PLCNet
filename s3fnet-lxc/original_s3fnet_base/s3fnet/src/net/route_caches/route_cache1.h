/**
 * \file route_cache1.h
 * \brief Header file for the RouteCache1 class.
 *
 * authors : Lenny Winterrowd
 */

#ifndef __ROUTE_CACHE1_H__
#define __ROUTE_CACHE1_H__

#include "net/forwardingtable.h"

namespace s3f {
namespace s3fnet {
namespace routecache1 {

// Size 256 will use 6/8 MB (32/64 bit machine) for 2000 hosts
const unsigned int ROUTE_CACHE_SIZE = 256;
// mask generally == ROUTE_CACHE_SIZE - 1 (for powers of 2)
const unsigned int INDEX_MASK =  0xff;

class RouteCacheEntry {
  public:
	RouteCacheEntry() : valid(false) {};
	~RouteCacheEntry() {};

	bool valid;
	IPADDR ipaddr;
	RouteInfo* route;
};

/**
 * \brief Direct-mapped Routing Cache
 *
 * Stores ROUTE_CACHE_SIZE entries in a direct-mapped array.
 * Current index is the last byte of the IP addr.
 * TODO: Enable dynamic size (via constructor)?  Scale with # of hosts?
 */
class RouteCache1 : public RouteCache {
  public:
	RouteCache1() {};

	~RouteCache1() {};

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
};

}; // namespace routecache1
}; // namespace s3fnet
}; // namespace s3f

#endif /*__ROUTE_CACHE1_H__*/

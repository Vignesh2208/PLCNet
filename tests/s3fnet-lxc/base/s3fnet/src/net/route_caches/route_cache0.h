/**
 * \file route_cache0.h
 * \brief Header file for the RouteCache0 class.
 *
 * authors : Lenny Winterrowd
 */

#ifndef __ROUTE_CACHE0_H__
#define __ROUTE_CACHE0_H__

#include "net/forwardingtable.h"

namespace s3f {
namespace s3fnet {
namespace routecache0 {

/**
 * \brief Single entry RouteCache implementation
 *
 * Stores a single RouteInfo value for fast lookup.
 */
class RouteCache0 : public RouteCache {
  public:
	RouteCache0() : cached_addr(IPADDR_INVALID), cached_route(0) {};

	~RouteCache0() {};

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
	IPADDR cached_addr;
	RouteInfo* cached_route;
};

}; // namespace routecache0
}; // namespace s3fnet
}; // namespace s3f

#endif /*__ROUTE_CACHE0_H__*/

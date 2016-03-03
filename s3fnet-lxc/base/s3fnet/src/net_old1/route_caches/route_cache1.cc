/**
 * \file route_cache1.cc
 * \brief Source file for the RouteCache1 class.
 *
 * authors : Lenny Winterrowd
 */

#include "net/route_caches/route_cache1.h"

namespace s3f {
namespace s3fnet {
namespace routecache1 {

RouteInfo* RouteCache1::lookup(IPADDR ipaddr) {
	int index = ipaddr & INDEX_MASK;
	RouteCacheEntry rce = mData[index];
	if(rce.valid && rce.ipaddr == ipaddr) {
		return rce.route;
	}
	return NULL;
}

void RouteCache1::update(IPADDR ipaddr, RouteInfo* route) {
	if(route && ipaddr != IPADDR_INVALID) {
		RouteCacheEntry rce;
		rce.valid = true;
		rce.ipaddr = ipaddr;
		rce.route = route;
		int index = ipaddr & INDEX_MASK;
		mData[index] = rce;
	}
}

void RouteCache1::invalidate() {
	// TODO: Implement partial invalidation based on parameter(s)?
	int i;
	for(i=0;i<ROUTE_CACHE_SIZE;i++) {
		mData[i].valid = false;
	}
}

}; // namespace routecache1
}; // namespace s3fnet
}; // namespace s3f

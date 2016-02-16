/**
 * \file route_cache2.cc
 * \brief Source file for the RouteCache2 class.
 *
 * authors : Lenny Winterrowd
 */

#include "net/route_caches/route_cache2.h"

namespace s3f {
namespace s3fnet {
namespace routecache2 {

RouteCache2::RouteCache2() {
	memset(&mEvictor,0,ROUTE_CACHE_SIZE/ASSOCIATIVITY);
}

RouteInfo* RouteCache2::lookup(IPADDR ipaddr) {
	int way = ipaddr & INDEX_MASK;
	RouteCacheEntry* rce_ptr = &mData[ASSOCIATIVITY*way];
	bool foundInvalid;
	if(rce_ptr->valid) {
		if(rce_ptr->ipaddr == ipaddr) {
			return rce_ptr->route;
		}
	} else {
		foundInvalid = true;
	}

	unsigned char target = 0;
	unsigned char lowestCtr;
	if(!foundInvalid) {
		lowestCtr = rce_ptr->ctr;
	}

	char i;
	for(i=1;i<ASSOCIATIVITY;i++,rce_ptr++) {
		if(rce_ptr->valid) {
			if(rce_ptr->ipaddr == ipaddr) {
				if(foundInvalid) {
					mEvictor[way] = target;
				}
				return rce_ptr->route;
			}
		} else if(!foundInvalid) {
			target = i;
			foundInvalid = true;
		}

		if(!foundInvalid && rce_ptr->ctr < lowestCtr) {
			target = i;
			lowestCtr = rce_ptr->ctr;
		}
	}

	mEvictor[way] = target;

	return NULL;
}

void RouteCache2::update(IPADDR ipaddr, RouteInfo* route) {
	if(route && ipaddr != IPADDR_INVALID) {
		RouteCacheEntry rce;
		rce.valid = true;
		rce.ipaddr = ipaddr;
		rce.route = route;
		rce.ctr = 0;
		int way = ipaddr & INDEX_MASK;
		int index = ASSOCIATIVITY*way+mEvictor[way];
		mData[index] = rce;
	}
}

void RouteCache2::invalidate() {
	// TODO: Implement partial invalidation based on parameter(s)?
	int i;
	for(i=0;i<ROUTE_CACHE_SIZE;i++) {
		mData[i].valid = false;
	}
}

}; // namespace routecache2
}; // namespace s3fnet
}; // namespace s3f

/**
 * \file route_cache0.cc
 * \brief Source file for the RouteCache0 class.
 *
 * authors : Lenny Winterrowd
 */

#include "net/route_caches/route_cache0.h"

namespace s3f {
namespace s3fnet {
namespace routecache0 {

RouteInfo* RouteCache0::lookup(IPADDR ipaddr) {
	if(cached_route && cached_addr == ipaddr) {
		return cached_route;
	}
	return NULL;
}

void RouteCache0::update(IPADDR ipaddr, RouteInfo* route) {
	if(route && ipaddr != IPADDR_INVALID) {
		cached_addr = ipaddr;
		cached_route = route;
	}
}

void RouteCache0::invalidate() {
	if(cached_addr != IPADDR_INVALID) {
		cached_addr = IPADDR_INVALID;
		cached_route = 0;
	}
}

}; // namespace routecache0
}; // namespace s3fnet
}; // namespace s3f

/**
 * \file shstl.h
 * \brief STL templates that are shared-memory safe.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __SHSTL_H__
#define __SHSTL_H__

#include <algorithm>
#include <cstring>
#include <cstddef>
#include <cstdlib>
#include <stack>
#include <string>
#include <deque>
#include "s3f.h"

using namespace std;

namespace s3f {
namespace s3fnet {

/** std::string in shared memory. */
typedef string S3FNET_STRING;

/** std::multimap<x,y> in shared memory. */
#define S3FNET_MULTIMAP(x,y) multimap<x,y>

/** std::map<x,y> in shared memory. */
#define S3FNET_MAP(x,y) map<x,y>

/** std::set<x> in shared memory. */
#define S3FNET_SET(x) set<x>

/** std::vector<x> in shared memory. */
#define S3FNET_VECTOR(x) vector<x>

/** std::deque<x> in shared memory. */
#define S3FNET_DEQUE(x) deque<x>

/** std::make_pair(x,y) in shared memory. */
#define S3FNET_MAKE_PAIR(x,y) make_pair(x,y)

/** std::pair<x,y> in shared memory. */
#define S3FNET_PAIR(x,y) pair<x,y>

/** std::stack<x, std::deque<x> > in shared memory. */
#define S3FNET_STACK(x) stack<x, deque<x> >

/** std::list<x> in shared memory. */
#define S3FNET_LIST(x) list<x >

/** std::hash_map<x,y> in shared memory. */
#define S3FNET_HASH_MAP(x,y) hash_map<x, y, hash<x >, equal_to<x > >

/** std::multimap<x,y,z> in shared memory (with redefinition of the
    compare function). */
#define S3FNET_MULTIMAP_CMP(x,y,z) multimap<x, y, z >

/** std::map<x,y,z> in shared memory (with redefinition of the compare
    function). */
#define S3FNET_MAP_CMP(x,y,z) map<x, y, z >

/** std::set<x,z> in shared memory (with redefinition of the compare
    function). */
#define S3FNET_SET_CMP(x,z) set<x, z >

// short aliases

/** A pair of integers. */
typedef pair<int,int > S3FNET_INT_PAIR;

/** A vector of strings. */
typedef S3FNET_VECTOR(S3FNET_STRING) S3FNET_STRING_VECTOR;

/** A vector of pointers. */
typedef S3FNET_VECTOR(void*) S3FNET_POINTER_VECTOR;

/** A map from an integer to a pointer. */
typedef S3FNET_MAP(int, void*) S3FNET_INT2PTR_MAP;

/** A set of long integers. */
typedef S3FNET_SET(long) S3FNET_LONG_SET;

/** A vector of long integers. */
typedef S3FNET_VECTOR(long) S3FNET_LONG_VECTOR;

/** Make a duplicate string in shared memory. */
inline char* sstrdup(const char* s) {
  if(!s) return 0;
  char* x = new char[strlen(s)+1]; // overloaded new operator
  strcpy(x, s);
  return x;
}

}; // namespace s3fnet
}; // namespace s3f

#endif /*__SHSTL_H__*/

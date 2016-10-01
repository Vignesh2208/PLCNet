/**
 * \file trie.cc
 * \brief Source file for the Trie class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "util/trie.h"

#ifdef TRIE_DEBUG
#include "net/forwardingtable.h"
#endif

namespace s3f {
namespace s3fnet {

#ifdef TRIE_DEBUG
#define TRIE_DUMP(x) printf("TRIE: "); x
#else
#define TRIE_DUMP(x)
#endif

/*
 * These macros allow us to treat a 32-bit quantity as 
 * a stream of bits. The 0th bit is the MSB and the 31st
 * bit is the LSB.
 *
 * returns the nth bit where n is as above.
 */
#define GETBIT(n, bits) (((bits) >> (TRIE_KEY_SIZE(bits) - (n) - 1)) & 1)

/*
 * Removes the first n bits (using left shift).
 */
#define REMOVE(n, bits) ((bits) << (n))

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/*
 * Constructor of TrieNode.
 */
TrieNode::TrieNode() : data(0)
{
  memset(children, 0, sizeof(TrieNode*)*TRIE_KEY_SPAN);
}

TrieNode::~TrieNode()
{
  if(data) delete data;
}

#ifdef TRIE_DEBUG
void TrieNode::dump()
{
  if(data) {
    RouteInfo* ri = (RouteInfo*)data;
    IPPrefix rdest = ri->destination;
    rdest.display();
  }
  if (children[Trie::iterator::LEFT]) { std::cout << "L"; }
  if (children[Trie::iterator::RIGHT]) { std::cout << "R"; }
}
#endif /*TRIE_DEBUG*/

}; // namespace s3fnet
}; // namespace s3f

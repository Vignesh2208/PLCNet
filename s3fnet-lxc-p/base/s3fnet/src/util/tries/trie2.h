/**
 * \file trie2.h
 * \brief Header file for the ip-optimized Trie2 class.
 *
 * authors : Dong (Kevin) Jin, Lenny Winterrowd
 */

#ifndef __TRIE2_H__
#define __TRIE2_H__

#include "s3fnet.h"
#include "util/shstl.h"
#include "util/trie.h"

namespace s3f {
namespace s3fnet {
namespace trie2 {

// DEBUG OFF
//#define TRIE2_DEBUG

const unsigned int TRIE_KEY_SPAN = 2; // digital Trie - 0, 1
const unsigned int TRIE_BIT_MASK = 0x80000000;

class TrieNode;

/**
 * \brief A data structure for forwarding tables.
 *
 * This Trie implementation stores values at their minimum required depth
 * instead of always at depth 32.
 * It's storage use is better than Trie0 and Trie 1, but is still
 * suboptimal, even for this implementation. It can potentially be
 * extended (at the cost of runtime reorganizations) to produce the
 * fewest necessary nodes and/or CIDR-style routing entries.
 *
 * NOTE: The CIDR address loading order limitations noted for Trie0 and
 * Trie1 apply in reverse to this class as shorter CIDR addresses will
 * replace longer ones already in the Trie. For properly-ordered input,
 * this has less overhead than Trie3, which supports unordered input
 * but may propagate entries up/down the Trie as necessary.
 */
class Trie2 : public Trie {
 public:
  /** The constructor. */
  Trie2();

  /** The destructor. */
  virtual ~Trie2();

  /**
   * Inserts a new (key, data) pair.
   * Adds a new (key, data) pair to the Trie. If the key already exists, and if
   * the replace flag is set, we will replace the old data with the
   * new one and return the old data. If the replace flag is not set,
   * the old data remains in the trie and the new data is returned. If
   * the key does not exist in trie, the data will be inserted and the
   * return value is 0.
   * @param key the key to insert
   * @param nBits bits of the key to consider (useful for CIDR)
   * @param data the TrieData object to add
   * @param replace whether to replace an existing value with the same key
   * @return NULL on success, otherwise the TrieData object we attempted to insert (so reference is not lost)
   */
  virtual TrieData* insert(TRIEBITSTRING key, int nBits, 
		   TrieData* data, bool replace);

  /**
   * Removes a key from the Trie.
   * Remove the key from the trie, if it exists, and returns the data
   * corresponding to the removed key. If the key is not in the trie,
   * return NULL.
   * @param key the key to remove
   * @param nBits bits of the key to consider
   * @return the data correspoding to the key on success, otherwise NULL
   */
  virtual TrieData* remove(TRIEBITSTRING key, int nBits);
  
  /**
   * Finds a key in the Trie.
   * Perform a longest-matching prefix search for the key and if
   * found, places the data in pData. Otherwise, pData is NULL.
   * @param key the key to lookup
   * @return the data matching the key on success, otherwise NULL
   */
  virtual TrieData* lookup(TRIEBITSTRING key);

  /**
   * Get the default Route.
   * @return an instance of the default route as a TrieData object
   */
  virtual TrieData* getDefault();

  /**
  * Return the number of items in the trie.
  * @return the size of the Trie (generally the number of elements contained)
  */
  virtual int size() { return nElements; }

 protected:
  /**
  * Deallocate the trie recursively.
  * @param theroot the root entry of the Trie to free
  */
  static void deallocate(TrieNode* theroot);

  /** The root of the trie. */
  TrieNode* root;

  /** Number of data elements stored in the trie. */
  int nElements;

private:
 public:
  /**
   * \internal
   * Depth-first iterator. 
   */
  class iterator {
  public:
    /** 
     * \internal
     * Undefined iterator state exception.
     */
    class State {};

	/** 
    * Enum for iterator movement
    */
    enum NextMove {
      LEFT  = 0, /**< Iterator should move to the left */
      RIGHT = 1, /**< Iterator should move to the right */
      UP    = 2, /**< Iterator should move up (backwards) */
      UNDEF = 3 /**< Iterator movement is undefined */
    };

    /**
    * Stores iterator's state.
    */
    class NodeNext { // stored in stack
    public:
      /**
      * Current Trie node
      */
      TrieNode* node;

       /**
      * Next move to make
      */
      NextMove  next;

      /**
      * Makes a move
      */
      NodeNext(TrieNode* _node, NextMove _next) : 
	node(_node), next(_next) {}

      /**
      * Equals comparator operation
      */
      int operator==(NodeNext& rhs) {
	return (node == rhs.node && next == rhs.next);
      }
    };

    // the constructors and the destructor
    /**
    * iterator constructor
    */
    iterator();

    /**
    * iterator constructor for Trie2 types
    */
    iterator(Trie2* t);

    /**
    * iterator copy constructor
    */
    iterator(const Trie2::iterator& it); // copy costr

    /**
    * iterator destructor
    */
    ~iterator();

    /**
    * Assignment operator.
    * Does deep copy of state stack.
    */
    iterator& operator=(iterator& rhs);

    /** Equals comparison operator. */
    int operator==(iterator& rhs);

    /** Not equals comparison operator. */
    int operator!=(iterator& rhs);

    /** Prefix incr. operator. */
    iterator& operator++();

    /** Arrow operator. */
    TrieNode* operator->();

    /**
    * Trie2 reference
    * Generally root for iterator.
    */
    Trie2* myTrie;

    /**
    * Current iterator node
    */
    TrieNode* currentNode;

    /**
    * Iterator's state stack
    */
    S3FNET_STACK(NodeNext*) stateStack;

    // debug
    /**
    * Prints the iterator.
    * Used primarily for debugging.
    */
    void print();

  private:
    /**
    * Copy state stack.
    * Copies the state stack. Intended as an assignment helper.
    */
    void copyStack(S3FNET_STACK(NodeNext*)& from, bool deepCopy=true);

    /**
    * Move to next node.
    * Moves to the next node. Intended as an iterator helper.
    */
    void nextNode();
  };

  //  iterator first;

  /**
  * Last item in iterator.
  */
  iterator last;

  /*
  * Returns first item in iterator.
  */
  iterator begin();

  /*
  * Returns last item in iterator.
  * @return last item in iterator
  */
  iterator& end() { return last; }
};

/**
 * \internal
 * The class representing each node in the trie.
 */
class TrieNode {
 public:
  /** The constructor. */
  TrieNode();

  /** The destructor. */
  ~TrieNode();

  /** Data field. */
  TrieData* data;

  /** Pointers to children nodes. */
  TrieNode* children[TRIE_KEY_SPAN];

#ifdef TRIE2_DEBUG
  /**
  * Dumps the current Trie
  * Intended for debugging only.
  */
  void dump();
#endif
};

}; // namespace trie2
}; // namespace s3fnet
}; // namespace s3f

#endif /*__TRIE2_H__*/

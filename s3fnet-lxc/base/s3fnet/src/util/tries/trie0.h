/**
 * \file trie0.h
 * \brief Header file for the Trie0 class.
 *
 * authors : Dong (Kevin) Jin, Lenny Winterrowd
 */

#ifndef __TRIE0_H__
#define __TRIE0_H__

#include "s3fnet.h"
#include "util/shstl.h"
#include "util/trie.h"

namespace s3f {
namespace s3fnet {
namespace trie0 {

const unsigned int TRIE_KEY_SPAN = 2; // digital Trie - 0, 1

class TrieNode;

/**
 * \brief A data structure for forwarding tables.
 *
 * We use this class to implement the routing table so that we can do
 * fast longest-matching prefix lookups. Our Trie is a container for
 * bit-strings. When a new bit-string is added, the number of
 * significant bits must be specified. Just as in IP addressing, the
 * significant bits are counted starting from the most significant
 * bit. The maximum size of the bit-string is 32 bits. With each key,
 * we also store a corresponding pointer to hold user data. In a
 * routing table implementation, the keys will be the IP prefixes and
 * the data will be a (link, next-hop) pair.
 *
 * NOTE: If CIDR addresses are used, they must be ordered from longest
 * mask lengths to shortest to prevent DUPLICATE_ROUTE replacement.
 * Example order: ip1, ip2/31, ..., ip100/16, ..., ip1000/1
 * CIDR addresses are also only guaranteed to work for static routing.
 */
class Trie0 : public Trie {
 public:
  /** The constructor. */
  Trie0();

  /** The destructor. */
  virtual ~Trie0();

  /** 
   * Inserts a new (key, data) pair.
   * Adds a new (key, data) pair to the Trie. If the key already exists, and if
   * the replace flag is set, we will replace the old data with the
   * new one and return the old data. If the replace flag is not set,
   * the old data remains in the Trie and the new data is returned. If
   * the key does not exist in Trie, the data will be inserted and the
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
   * corresponding to the removed key. If the key is not in the Trie0,
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
   * Helper function for the insert and lookup methods.
   * This function will search the trie for the key and if found, return a
   * pointer to the TrieNode. If not found, it will return NULL, and set
   * failedNode to the TrieNode where the search fails and
   * failedbitpos to the bit position that failed.
   * @param key the key for insert/lookup
   * @param len the bits to consider of the key
   * @param failedNode a TrieNode reference address to write the address of the last node traversed before failure
   * @param failedbitpos an integer address to write the bit position of the last traversal before failure
   * @return the TrieNode found, NULL otherwise
   */
  TrieNode* search(TRIEBITSTRING key, int len, 
		   TrieNode** failedNode, int* failedbitpos);
  
  /**
   * Creates a node from the root.
   * Static helper function for the insert method. Creates nBits nodes for
   * the key starting at TrieNode theroot.
   * @param theroot the root entry to start from
   * @param key the key to create a node for
   * @param nBits the bits to consider
   * @param data the data to store
   */
  static void create(TrieNode* theroot, TRIEBITSTRING key, 
		     int nBits, TrieData* data);

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
  /** The ip address of the cached route. */
  TRIEBITSTRING cached_key;

  /** The cached route. */
  TrieData* cached_data;

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
    * iterator constructor for Trie0 types
    */
    iterator(Trie0* t);

    /**
    * iterator copy constructor
    */
    iterator(const Trie0::iterator& it); // copy costr

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
    * Trie0 reference
    * Generally root for iterator.
    */
    Trie0* myTrie;

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

#ifdef TRIE0_DEBUG
  /**
  * Dumps the current Trie
  * Intended for debugging only.
  */
  void dump();
#endif
};

}; // namespace trie0
}; // namespace s3fnet
}; // namespace s3f

#endif /*__TRIE0_H__*/

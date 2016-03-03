/**
 * \file triedata_array0.h
 * \brief Header file for an overhead-measuring TrieDataArray (no coalescing).
 *
 * authors : Lenny Winterrowd
 */

#ifndef __TRIEDATA_ARRAY0_H__
#define __TRIEDATA_ARRAY0_H__

#include "util/trie.h"

namespace s3f {
namespace s3fnet {
namespace tda0 {

// TODO: test to determine a good value for these
const unsigned int TDA_DEFAULT_SIZE = 64;
const unsigned int REALLOC_FACTOR = 2.0;

/**
* List of free array indices.
* Linked-list of free space in the array (much like a heap). Prevents us from needing
* to coalesce or extend very often (if at all).
*/
class InsertList {
  public:
    /**
    * Constructor
    * @param i index of this entry (where insertion may occur)
    * @param n next item in the chain, may be NULL
    */
  	InsertList(unsigned int i, InsertList* n) : index(i), next(n) {};

    /**
    * Insertion point for this entry
    */
	unsigned int index;

    /**
    * Pointer to next node.
    * Reference to the next InsertList node in the linked-list.
    */
	InsertList* next;
};

/**
 * \brief Data structure to store uncoalesced TrieData sets.
 *
 * Only useful to measure overhead associated with abstracting TrieData storage to an array.
 * Otherwise performs no useful function over a Trie that stores data in its leaves.
 * Also requires copying items to and from the array in addition to deleting source data
 * to prevent losing it, thus adding both memory and computational overhead.
 *
 * IMPORTANT: Cannot store TrieData items of varying size -- all must return exactly the same
 * 			  value for triedata->size() since there will be implicit casting of raw memory.
 */
class TrieDataArray0 {
  public:
    /**
    * Constructor
    * @param size array size
    */
	TrieDataArray0(unsigned int size = TDA_DEFAULT_SIZE);

	/**
	* Destructor
	*/
	~TrieDataArray0();

	/**
     * Adds the given TrieData to the array.
	 * @param data the element to add
     * @return the index at which the data is stored
     */
	virtual unsigned int addElement(TrieData* data);

	/**
    * Gets the element at 'index'
    * @param index the element index to get
    * @returns the element at index
    */
	virtual TrieData* getElement(unsigned int index);

	/** Remove element at 'index' from the array.
     * @param index the element to get
     * @param get whether to return the item or just remove it
	 * @return a copy of removed element if 'get' is true, else returns NULL.
	 */
	virtual TrieData* removeElement(unsigned int index, bool get = false);

  protected:
    /**
    * Size of the array.
    */
	unsigned int mSize;

    /**
	* Array entries that are full.
    */
	unsigned int mCount;

    /**
    * Size of each element stored in the array
    */
	unsigned int mDataSize;

	/**
    * Pointer to the actual array
    */
	char* mData;

    /**
    * Linked list of free entries
    */
	InsertList* mInsertPoint;

	/** Helper function to extend the TrieDataArray when full. */
	void extend();
};

}; // namespace tda0
}; // namespace s3fnet
}; // namespace s3f

#endif /*__TRIEDATA_ARRAY0_H__*/

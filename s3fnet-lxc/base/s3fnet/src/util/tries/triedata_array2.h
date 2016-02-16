/**
 * \file triedata_array2.h
 * \brief Header file for a mapping, TrieData-coalescing TrieDataArray.
 *
 * authors : Lenny Winterrowd
 */

#ifndef __TRIEDATA_ARRAY2_H__
#define __TRIEDATA_ARRAY2_H__

#include "util/trie.h"

namespace s3f {
namespace s3fnet {
namespace tda2 {

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
* Class to maintain array reference counts.
*/
class TDStruct {
  public:
    /**
    * Constructor
    * @param td TriedData entry this counts for
    */
	TDStruct(TrieData* td = 0) : refCount(0), data(td) {};

	/**
    * Destructor
    */
	~TDStruct() { if(data) delete data; };

	/** Current reference count for the node */
	unsigned int refCount;

	/** Pointer to the data being counted for */
	TrieData* data;
};

/**
 * \brief Data structure to store coalesced TrieData sets.
 *
 * This structure can be used to save space if there are duplicate RouteInfo entries.
 * However, insertion time will go up due to checking for duplicates.
 * Since this is effectively a map to the proper pointer, it introduces at least one additional
 * memory access into the lookup-chain.
 *
 */
class TrieDataArray2 {
  public:
    /**
    * Constructor
    * @param size array size
    */
	TrieDataArray2(unsigned int size = TDA_DEFAULT_SIZE);

	/**
	* Destructor
	*/
	~TrieDataArray2();

	/**
     * Adds the given TrieData to the array.
     * Returns the index at which the data is stored.
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
	* Actual array of reference-count structures
	*/
	TDStruct* mData;

    /**
    * Linked list of free entries
    */
	InsertList* mInsertPoint;

	/** Helper function to extend the TrieDataArray when full. */
	void extend();
};

}; // namespace tda2
}; // namespace s3fnet
}; // namespace s3f

#endif /*__TRIEDATA_ARRAY2_H__*/

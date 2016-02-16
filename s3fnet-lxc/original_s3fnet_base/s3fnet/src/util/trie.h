/**
 * \file trie.h
 * \brief Header file for the Trie base class.
 *
 * authors : Dong (Kevin) Jin, Lenny Winterrowd
 */

#ifndef __TRIE_H__
#define __TRIE_H__

#include <cstring>
#include "s3fnet.h"

namespace s3f {
namespace s3fnet {

#define TRIE_KEY_SIZE(key) 32

/**
 * \internal
 * \brief The key type for the trie.
 *
 * This typedef can be changed (along with a few macros in trie.cc) to
 * support tries of strings.
 *
 * NOTE: For integers, TRIEBITSTRING must guarantee exactly TRIE_KEY_SIZE bits for proper function
 */
typedef uint32 TRIEBITSTRING;

class TrieNode;

/**
 * \brief A generic data type for data stored in trie.
 *
 * This class is used by the Trie class as a base class to store data.
 */
class TrieData {
 public:
  /** The virtual destructor does nothing here. */
  virtual ~TrieData() {};

  /** returns size in bytes */
  virtual int size() = 0;

 protected:
  /** transform the structure into a byte array of length size() */
  virtual char* toByteArray() {
	return (char*)this;
  }

 public:
  /**
  * Compares this TrieData instance to another.
  * @param td the TrieData reference to compare with.
  * @return true if data is identical, otherwise false
  */
  bool equiv(TrieData* td) {
	if(size() != td->size()) {
		return false;
	}

	int i;
	char* selfarray = toByteArray();
	char* tdarray = td->toByteArray();
	for(i=0;i<size();i++) {
		if(selfarray[i] != tdarray[i]) {
			return false;
		}
	}
	return true;
  }

  /**
  * Duplicates the TrieData instance.
  * Creates another TrieData instance containing identical data. Relies on the
  * toByteArray() virtual method but is not, itself, virtual.
  * @see toByteArray()
  * @return (deep) copy of the current TrieData
  */
  TrieData* clone() {
	int s = size();
	int i;
	char* temp = new char[s];
	memcpy((void*)temp,toByteArray(),s);
	return (TrieData*)temp;
  }

};

/**
* Base class for Trie implementations.
* Data structure to store and efficiently match large datasets such as routing
* tables. This is generally meant to be a pure-virtual (abstract) base class
* that others must derive. However, a base implementation is provided.
*/
class Trie {
 public:
    /**
    * Enum containing Trie implementations.
    */
	enum TrieType {
		ORIGINAL = 0, /**< Very first s3fnet Trie implementation. */
		SIMPLE = 1, /**< Identical to ORIGINAL with minor fixes/enhancements */
		PREFIX = 2, /**< Stores values at minimum depth instead of depth equal to their bit-length (ie depth 32 for an ipv4 address). */
		UNORDERED_PREFIX = 3, /**< Like PREFIX but can take CIDR-format IP address as input in any order */
		ARRAY_BACKED = 4, /**< Trie that stores indices into an array in its nodes (vs data IN the nodes). */
		ARRAY_BACKED_PREFIX = 5, /** PREFIX with array-backing (ARRAY_BACKED). */
		ARRAY_BACKED_UNORDERED_PREFIX = 6 /** UNORDERED_PREFIX with array-backing (ARRAY_BACKED). */
	};

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
		   TrieData* data, bool replace) = 0;

  /**
   * Removes a key from the Trie.
   * Remove the key from the trie, if it exists, and returns the data
   * corresponding to the removed key. If the key is not in the trie,
   * return NULL.
   * @param key the key to remove
   * @param nBits bits of the key to consider
   * @return the data correspoding to the key on success, otherwise NULL
   */
  virtual TrieData* remove(TRIEBITSTRING key, int nBits) = 0;
  
  /**
   * Finds a key in the Trie.
   * Perform a longest-matching prefix search for the key and if
   * found, places the data in pData. Otherwise, pData is NULL.
   * @param key the key to lookup
   * @return the data matching the key on success, otherwise NULL
   */
  virtual TrieData* lookup(TRIEBITSTRING key) = 0;

  /**
   * Return the default TrieData item.
   * Gets the 'default' TrieData object (ie, may be returned if there is no
   * match in the Trie but a non-NULL return is needed. This may (and generally
   * will) vary by implementation.
   * @return an instance of the default TrieData object
   */
  virtual TrieData* getDefault() = 0;

  /**
   * Return the size of the Trie.
   * @return the size of the Trie (generally the number of elements contained)
   */
  virtual int size() = 0;
};

/**
 * \brief Abstract base class for storing coalesced TrieData sets.
 *
 * Existence of this class adds an extra memory reference to each lookup due to
 * base-class pointer-casting in implementing Tries. As such, this should be
 * used primarily for testing with Tries designed for such, then the best
 * implementation(s) should be directly added as their own Trie implementation.
 *
 * All types are implicitly maps (vs actual arrays) since they store pointers
 * unless there is an explicit _MAP type with the same name.
 *
 * Is pure-virtual. Derived classes must add their own member array/list/etc.
 * This allows subclasses to choose stack-based or ptr-based storage as desired.
 */
class TrieDataArray {
  public:
   /**
   * Enum listing the types of TrieDataArrays available.
   */
	enum TrieDataArrayType {
		UNCOALESCED = 0, /**< Every object in the Trie has a unique array entry. */
		UNCOALESCED_MAP = 1, /**< Stores references instead of the actual data. NAIVE, HASHED, and FAST_HASHED are based on this for ease of implementation. */
		NAIVE = 2, /**< Multiple nodes in the Trie may map to the same (identical) TrieData entry in the array to save space. */
		HASHED = 3, /**< Expands on NAIVE using hashing to quickly find the correct entry in the array */
		FAST_HASHED = 4 /**< Sacrifices storage in some cases for faster lookup speed by allowing some duplicate entries in the array */
	};

	/** Adds the given TrieData to the array.
     * Pure virtual method to add elements to the TrieDataArray.
     * @param data reference to insert and/or copy (depending on implementation)
     * @return the index at which the data is stored
     */
	virtual unsigned int addElement(TrieData* data) = 0;

	/**
    * Gets the element at 'index'
    * Pure virtual method to get elements from the array.
    * @param index array index of the element to get
    * @return TrieData reference to the element
    */
	virtual TrieData* getElement(unsigned int index) = 0;

	/**
     * Remove element at 'index' from the array.
     * Pure virtual method to remove elements from the array.
     * @param index array index of the element to remove
     * @param get whether to return the removed element or just remove it
	 * @return a copy of removed element if 'get' is true, else returns NULL.
	 */
	virtual TrieData* removeElement(unsigned int index, bool get = false) = 0;
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__TRIE_H__*/

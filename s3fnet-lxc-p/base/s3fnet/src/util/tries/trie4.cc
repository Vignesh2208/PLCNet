/**
 * \file trie4.cc
 * \brief Source file for the ip-optimized Trie4 class.
 *
 * authors : Dong (Kevin) Jin, Lenny Winterrowd
 */

#include <climits>
#include "util/tries/trie4.h"
#include "net/ip_prefix.h"

// TrieDataArray implementations
#include "util/tries/triedata_array0.h"
#include "util/tries/triedata_array1.h"
#include "util/tries/triedata_array2.h"

#ifdef TRIE4_DEBUG
#include "net/forwardingtable.h"
#endif

namespace s3f {
namespace s3fnet {
namespace trie4 {


//#define DEFAULT_ARRAY_VERSION TrieDataArray::UNCOALESCED
//#define DEFAULT_ARRAY_VERSION TrieDataArray::UNCOALESCED_MAP
#define DEFAULT_ARRAY_VERSION TrieDataArray::NAIVE


#ifdef TRIE4_DEBUG
#define TRIE4_DUMP(x) printf("TRIE4: "); x
#else
#define TRIE4_DUMP(x)
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

/*
 * Constructor. We will create a root node.
 */
Trie4::Trie4(int array_version) : nElements(0), last(this) {
	root = new TrieNode();
	assert(root);
	if(array_version == -1) {
		array_version = DEFAULT_ARRAY_VERSION;
	}

  // TODO: Make TrieDataArray configurable via dml (based on version)
  avstart:
	switch(array_version) {
	case TrieDataArray::UNCOALESCED:
		mData = (TrieDataArray*)new tda0::TrieDataArray0();
		break;
	case TrieDataArray::UNCOALESCED_MAP:
		mData = (TrieDataArray*)new tda1::TrieDataArray1();
		break;
	case TrieDataArray::NAIVE:
		mData = (TrieDataArray*)new tda2::TrieDataArray2();
		break;
	default:
		fprintf(stderr, "Invalid TrieDataArray version (%d). Using default (%d).",array_version,
				DEFAULT_ARRAY_VERSION);
		array_version = DEFAULT_ARRAY_VERSION;
		goto avstart;
	}
	assert(mData);
}

/*
 * Recursively free each node.
 */
Trie4::~Trie4()
{
  deallocate(root);
  delete mData;
  nElements = 0;
}

/*
 * Inserts a new (key, data) pair.
 */
TrieData* Trie4::insert(TRIEBITSTRING key, int nBits, 
		       TrieData* data, bool replace) {
  TrieNode *fail = 0;
  int bitpos = 0;

  TrieNode* match = search(key, nBits, &fail, &bitpos);

  // Does the key already exist in our trie?
  // If so, replace the data if replace flag is true.
  if(match) {
    if(!replace) return data;
    TrieData* temp = mData->removeElement(match->index,true);
    match->index = mData->addElement(data);
    return temp;
  }
  
  // Create all the necessary nodes for the unmatched bits
  // after fail and insert the data.
  create(fail, REMOVE(bitpos, key), nBits-bitpos, data);
  nElements++;
  
  return 0;
}

/*
 * Performs a fast longest-matching prefix search for the key. The
 * reason to do so is that it's used in routing entry searches.
 * Because it's so critical to routing performance, a fast search is
 * necessary.
 *
 * We assume the search length is independent. that means we try to
 * find the best.
 */
TrieData* Trie4::lookup(TRIEBITSTRING key)
{
  // If this Trie exists to call 'lookup', root has already been asserted as non-null
  TrieNode* cur = root->children[(key & TRIE_BIT_MASK)?1:0];
  TrieData* pData;
  if(root->index != UINT_MAX) {
	pData = mData->getElement(root->index); // handles default-route case
  } else {
	pData = 0;
  }

  while(cur) {
    if(cur->index != UINT_MAX) {
		pData = mData->getElement(cur->index);
	}
	key <<= 1;
    cur = cur->children[(key & TRIE_BIT_MASK)?1:0];
  }

  return pData;
}

/*
 * Removes the key from the trie, if it exists, and returns the data
 * corresponding to the removed key. If the key is not in the trie,
 * return 0.
 */
TrieData* Trie4::remove(TRIEBITSTRING key, int nBits) {
  TrieNode* ptr;
  TrieData* olddata;
  TrieNode* parent;
  int index;

  ptr = search(key, nBits, &parent, &index);
  if(!ptr) return 0;
  olddata = mData->removeElement(ptr->index,true);

  ptr->index = UINT_MAX;
  nElements--;
  if(ptr->children[0] == 0 && ptr->children[1] == 0) {
	delete parent->children[index];
	parent->children[index] = 0;
  }

  return olddata;
}

TrieData* Trie4::getDefault() {
	if(root) return mData->getElement(root->index);
	return 0;
}

/*
 * Helper function for Insert and Lookup. This function will search
 * the trie for the key. If found, it will return a pointer to the
 * TrieNode, set failedNode to its parent, and set failedbitpos to the
 * index in the parent's array. If not found, it will return NULL, and
 * set failedNode to the TrieNode where the search fails and
 * failedbitpos to the bit position that failed.
 */
TrieNode* Trie4::search(TRIEBITSTRING key, int len, 
		       TrieNode** failedNode, int* failedbitpos)
{
  // TODO: Optimize this function
  TrieNode* cur, *lastcur, *parent;
  TrieNode* match = 0;
  int bitpos, curbit, parentbit;
  if(len > TRIE_KEY_SIZE(key)) {
	len = TRIE_KEY_SIZE(key);
  }

  // Go through the bits starting from the MSB and traverse
  // the tree. Remember the last match.
  for(bitpos = 0, lastcur = cur = root; 
      cur && bitpos < len;
      cur = cur->children[curbit], bitpos++) {
    if(cur->index != UINT_MAX) {
		parent = lastcur;
		parentbit = GETBIT(bitpos-1,key);
		match = cur;
	}
      
    lastcur = cur;
    curbit = GETBIT(bitpos, key);
  }

  if(cur && cur->index != UINT_MAX) {
	match = cur;
	if(failedNode && failedbitpos) {
		// we have a match on the last node, parent is lastcur
		*failedNode = lastcur;
		*failedbitpos = GETBIT(bitpos-1,key);
	}
  } else if(failedNode && failedbitpos) {
	// if there is a match
	if(match) {
		*failedNode = parent;
		*failedbitpos = parentbit;
	} else { // no match
		if(cur) {
		  *failedbitpos = bitpos;
		  *failedNode = cur;
		} else {
		  *failedbitpos = bitpos - 1;
		  *failedNode = lastcur;
		}
	}
  }

  return match;
}


/*
 * Helper function for Insert. Creates nBits nodes for the key
 * starting at TrieNode theroot.
 */
void Trie4::create(TrieNode* theroot, TRIEBITSTRING key,
		  int nBits, TrieData* data)
{
  TrieNode* ptr;

  for(int i=0; i < nBits; i++, theroot = ptr) {
	assert(theroot->children[GETBIT(i, key)] == 0);
    ptr = theroot->children[GETBIT(i, key)] = new TrieNode();
    assert(ptr);
  }
  theroot->index = mData->addElement(data);
}

/*
 * Deallocates the trie recursively.
 */
void Trie4::deallocate(TrieNode* theroot)
{
  if(theroot == 0) return;

  // deallocate all the children
  for(int i = 0; i < TRIE_KEY_SPAN; i++) 
    deallocate(theroot->children[i]);
  
  delete theroot;
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
// iterator
//  

//#define CURR_NODE   currentNode.top()

Trie4::iterator::iterator() : myTrie(0), currentNode(0), stateStack() 
{}

//Trie4::iterator::iterator(Trie4* t) : myTrie(t), currentNode(t->root), 
Trie4::iterator::iterator(Trie4* t) : myTrie(t), currentNode(0), 
				    stateStack() 
{}

Trie4::iterator::iterator(const Trie4::iterator& it) : 
  myTrie(it.myTrie), currentNode(it.currentNode), stateStack() 
{
  if (it.stateStack.size() > 0) { 
    // copy stack -- Need to modify the it.stateStack but will put it back. 
    // So, casting away const.
    S3FNET_STACK(NodeNext*)& ss = (S3FNET_STACK(NodeNext*)&)it.stateStack;
    copyStack(ss, true); // deep copy
  }
}

Trie4::iterator::~iterator() {
  // destroy state contents  
  while (stateStack.size() > 0) {
    iterator::NodeNext* nn = stateStack.top();
    stateStack.pop();
    delete nn;
  }
}

Trie4::iterator& Trie4::iterator::operator=(iterator& rhs) {
  myTrie = rhs.myTrie;
  currentNode = rhs.currentNode;
  if (rhs.stateStack.size() > 0) { 
    // copy stack
    copyStack(rhs.stateStack);
  }

  return *this;
}

void Trie4::iterator::copyStack(S3FNET_STACK(NodeNext*)& from, bool deepCopy) {
  S3FNET_STACK(NodeNext*)& to = stateStack;
  if (from.size() > 0) {
    // pop elem
    NodeNext* nn = from.top();
    from.pop();
    // copy rest
    copyStack(from);
    // copy elem
    if (deepCopy) {
      to.push(new NodeNext(*nn));
    } else {
      to.push(nn);
    }
    from.push(nn);
  }
  return;
}

int Trie4::iterator::operator==(iterator& rhs) {
  if (currentNode != rhs.currentNode) {
    return 0;
  }
  if (stateStack.size() != rhs.stateStack.size()) {
    return 0;
  }
  if (stateStack.size() > 0 && !(stateStack.top() == rhs.stateStack.top())) {
    return 0;
  }
  return 1;
}

int Trie4::iterator::operator!=(iterator& rhs) {
  return (*this == rhs) ? 0 : 1;
}

Trie4::iterator& Trie4::iterator::operator++() {
  if (!currentNode) {
    stateStack.push(new NodeNext(myTrie->root, LEFT));
    currentNode = myTrie->root;
    assert(currentNode);
  }

  do {
    nextNode();
  } while (currentNode->index == UINT_MAX  // repeat until node with data found
	   && currentNode != myTrie->root); // or reached end
  if (currentNode == myTrie->root) {
    currentNode = 0;
    TRIE4_DUMP(std::cout << currentNode << " stopped (end)" << std::endl);
  } else {
    TRIE4_DUMP(std::cout << currentNode << " stopped (found)" << std::endl);
  }
  return *this;
}

void Trie4::iterator::nextNode() {
  bool keepMoving = true;
  while(keepMoving) {
    NodeNext* nn = stateStack.top(); 
    stateStack.pop();
    currentNode = nn->node;
    assert(currentNode);
    switch (nn->next) {
    case LEFT: {
      stateStack.push(new NodeNext(nn->node, RIGHT)); // afterwards move right
      TrieNode* leftChild = currentNode->children[LEFT];
      if (leftChild) { // has left child?
	stateStack.push(new NodeNext(leftChild, LEFT)); // descend left
	TRIE4_DUMP(std::cout << currentNode << " push RIGHT, LEFT down, push LEFT (" << 
		  stateStack.size() << ") - "; currentNode->dump(); std::cout << std::endl);
      } else {
	TRIE4_DUMP(std::cout << currentNode << " push RIGHT (" << stateStack.size() << 
		  ") - "; currentNode->dump(); std::cout << std::endl);
      }
      }
      break;
    case RIGHT: {
      stateStack.push(new NodeNext(nn->node, UP)); // afterwards move up
      TrieNode* rightChild = currentNode->children[RIGHT];
      if (rightChild) { // has right child?
	TRIE4_DUMP(std::cout << currentNode << " push UP, RIGHT down, push LEFT (" << 
		  stateStack.size() << ") - "; currentNode->dump(); std::cout << std::endl);
	stateStack.push(new NodeNext(rightChild, LEFT)); // afterwards move left
      } else {
	TRIE4_DUMP(std::cout << currentNode << " push UP (" << stateStack.size() <<
		  ") - "; currentNode->dump(); std::cout << std::endl);
      }
      }
      break;
    case UP:
      // visit node
      keepMoving = false;
      TRIE4_DUMP(std::cout << currentNode << " visit (" << stateStack.size() <<
		") - "; currentNode->dump(); std::cout << std::endl);
      assert(currentNode != myTrie->root || stateStack.size() == 0);
      break;
    default:
      // ERROR: undefined state of trie iterator
      throw State();
    } // switch
    delete nn;
  } // while
}


/* Arrow operator. */
TrieNode* Trie4::iterator::operator->() {
  return currentNode;
}

// debug
void Trie4::iterator::print() {
  std::cout << "Trie4::iterator -- trie=" << myTrie << " currentNode=" << currentNode
       << " stateStack size=" << stateStack.size() << std::endl;
}

Trie4::iterator Trie4::begin() { 
  iterator first(this); // create iterator
  ++first; // find first element
  return first; // return by copy
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/*
 * Constructor of TrieNode.
 */
TrieNode::TrieNode() : index(UINT_MAX) {
  // UINT_MAX is used to (effectively) represent NULL ptrs
  memset(children, 0, sizeof(TrieNode*)*TRIE_KEY_SPAN);
}

#ifdef TRIE4_DEBUG
void TrieNode::dump()
{
  if(data) {
    RouteInfo* ri = (RouteInfo*)data;
    IPPrefix rdest = ri->destination;
    rdest.display();
  }
  if (children[Trie4::iterator::LEFT]) { std::cout << "L"; }
  if (children[Trie4::iterator::RIGHT]) { std::cout << "R"; }
}
#endif /*TRIE4_DEBUG*/

}; // namespace trie4
}; // namespace s3fnet
}; // namespace s3f

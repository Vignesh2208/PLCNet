/**
 * \file trie1.cc
 * \brief Source file for the ip-optimized Trie1 class (original Trie with minor fixes).
 *
 * authors : Dong (Kevin) Jin, Lenny Winterrowd
 */

#include "util/tries/trie1.h"
#include "net/ip_prefix.h"

#ifdef TRIE1_DEBUG
#include "net/forwardingtable.h"
#endif

namespace s3f {
namespace s3fnet {
namespace trie1 {

#ifdef TRIE1_DEBUG
#define TRIE1_DUMP(x) printf("TRIE1: "); x
#else
#define TRIE1_DUMP(x)
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
Trie1::Trie1() : nElements(0), last(this) {
	root = new TrieNode();
	assert(root);
}

/*
 * Recursively free each node.
 */
Trie1::~Trie1()
{
  deallocate(root);
  nElements = 0;
}

/*
 * Inserts a new (key, data) pair.
 */
TrieData* Trie1::insert(TRIEBITSTRING key, int nBits, 
		       TrieData* data, bool replace) {
  // TODO: Force insertion to specified depth? At least note WHY duplicate route problem occurs.
  TrieNode *fail = 0;
  int bitpos = 0;

  TrieNode* match = search(key, nBits, &fail, &bitpos);

  // Does the key already exist in our trie?
  // If so, replace the data if replace flag is true.
  if(match) {
    if(!replace) return data;
    TrieData* temp = match->data;
    match->data = data;
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
TrieData* Trie1::lookup(TRIEBITSTRING key)
{
  // If this Trie exists to call 'lookup', root has already been asserted as non-null
  TrieNode* cur = root->children[(key & TRIE_BIT_MASK)?1:0];
  TrieData* pData = root->data; // handles default-route case

  while(cur) {
    if(cur->data) {
		pData = cur->data;
	}
	key <<= 1; // needed from pre-loop lookup and removes last unnecessary shift
    cur = cur->children[(key & TRIE_BIT_MASK)?1:0];
  }

  return pData;
}

/*
 * Removes the key from the trie, if it exists, and returns the data
 * corresponding to the removed key. If the key is not in the trie,
 * return 0.
 */
TrieData* Trie1::remove(TRIEBITSTRING key, int nBits)
{
  TrieNode* ptr;
  TrieData* olddata;
  TrieNode* parent;
  int index;

  ptr = search(key, nBits, &parent, &index);
  if(!ptr) return 0;
  olddata = ptr->data;

  ptr->data = 0;
  nElements--;
  if(ptr->children[0] == 0 && ptr->children[1] == 0) {
	delete parent->children[index];
	parent->children[index] = 0;
  }

  return olddata;
}

TrieData* Trie1::getDefault() {
	if(root) return root->data;
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
TrieNode* Trie1::search(TRIEBITSTRING key, int len, 
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
    if(cur->data) {
		parent = lastcur;
		parentbit = GETBIT(bitpos-1,key);
		match = cur;
	}
      
    lastcur = cur;
    curbit = GETBIT(bitpos, key);
  }

  if(cur && cur->data) {
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
void Trie1::create(TrieNode* theroot, TRIEBITSTRING key,
		  int nBits, TrieData* data)
{
  TrieNode* ptr;

  for(int i=0; i < nBits; i++, theroot = ptr) {
	assert(theroot->children[GETBIT(i, key)] == 0);
    ptr = theroot->children[GETBIT(i, key)] = new TrieNode();
    assert(ptr);
  }
  theroot->data = data;
}

/*
 * Deallocates the trie recursively.
 */
void Trie1::deallocate(TrieNode* theroot)
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

Trie1::iterator::iterator() : myTrie(0), currentNode(0), stateStack() 
{}

//Trie1::iterator::iterator(Trie1* t) : myTrie(t), currentNode(t->root), 
Trie1::iterator::iterator(Trie1* t) : myTrie(t), currentNode(0), 
				    stateStack() 
{}

Trie1::iterator::iterator(const Trie1::iterator& it) : 
  myTrie(it.myTrie), currentNode(it.currentNode), stateStack() 
{
  if (it.stateStack.size() > 0) { 
    // copy stack -- Need to modify the it.stateStack but will put it back. 
    // So, casting away const.
    S3FNET_STACK(NodeNext*)& ss = (S3FNET_STACK(NodeNext*)&)it.stateStack;
    copyStack(ss, true); // deep copy
  }
}

Trie1::iterator::~iterator() {
  // destroy state contents  
  while (stateStack.size() > 0) {
    iterator::NodeNext* nn = stateStack.top();
    stateStack.pop();
    delete nn;
  }
}

Trie1::iterator& Trie1::iterator::operator=(iterator& rhs) {
  myTrie = rhs.myTrie;
  currentNode = rhs.currentNode;
  if (rhs.stateStack.size() > 0) { 
    // copy stack
    copyStack(rhs.stateStack);
  }

  return *this;
}

void Trie1::iterator::copyStack(S3FNET_STACK(NodeNext*)& from, bool deepCopy) {
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

int Trie1::iterator::operator==(iterator& rhs) {
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

int Trie1::iterator::operator!=(iterator& rhs) {
  return (*this == rhs) ? 0 : 1;
}

Trie1::iterator& Trie1::iterator::operator++() {
  if (!currentNode) {
    stateStack.push(new NodeNext(myTrie->root, LEFT));
    currentNode = myTrie->root;
    assert(currentNode);
  }

  do {
    nextNode();
  } while (currentNode->data == 0  // repeat until node with data found
	   && currentNode != myTrie->root); // or reached end
  if (currentNode == myTrie->root) {
    currentNode = 0;
    TRIE1_DUMP(std::cout << currentNode << " stopped (end)" << std::endl);
  } else {
    TRIE1_DUMP(std::cout << currentNode << " stopped (found)" << std::endl);
  }
  return *this;
}

void Trie1::iterator::nextNode() {
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
	TRIE1_DUMP(std::cout << currentNode << " push RIGHT, LEFT down, push LEFT (" << 
		  stateStack.size() << ") - "; currentNode->dump(); std::cout << std::endl);
      } else {
	TRIE1_DUMP(std::cout << currentNode << " push RIGHT (" << stateStack.size() << 
		  ") - "; currentNode->dump(); std::cout << std::endl);
      }
      }
      break;
    case RIGHT: {
      stateStack.push(new NodeNext(nn->node, UP)); // afterwards move up
      TrieNode* rightChild = currentNode->children[RIGHT];
      if (rightChild) { // has right child?
	TRIE1_DUMP(std::cout << currentNode << " push UP, RIGHT down, push LEFT (" << 
		  stateStack.size() << ") - "; currentNode->dump(); std::cout << std::endl);
	stateStack.push(new NodeNext(rightChild, LEFT)); // afterwards move left
      } else {
	TRIE1_DUMP(std::cout << currentNode << " push UP (" << stateStack.size() <<
		  ") - "; currentNode->dump(); std::cout << std::endl);
      }
      }
      break;
    case UP:
      // visit node
      keepMoving = false;
      TRIE1_DUMP(std::cout << currentNode << " visit (" << stateStack.size() <<
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
TrieNode* Trie1::iterator::operator->() {
  return currentNode;
}

// debug
void Trie1::iterator::print() {
  std::cout << "Trie1::iterator -- trie=" << myTrie << " currentNode=" << currentNode
       << " stateStack size=" << stateStack.size() << std::endl;
}

Trie1::iterator Trie1::begin() { 
  iterator first(this); // create iterator
  ++first; // find first element
  return first; // return by copy
}

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

#ifdef TRIE1_DEBUG
void TrieNode::dump()
{
  if(data) {
    RouteInfo* ri = (RouteInfo*)data;
    IPPrefix rdest = ri->destination;
    rdest.display();
  }
  if (children[Trie1::iterator::LEFT]) { std::cout << "L"; }
  if (children[Trie1::iterator::RIGHT]) { std::cout << "R"; }
}
#endif /*TRIE1_DEBUG*/

}; // namespace trie1
}; // namespace s3fnet
}; // namespace s3f

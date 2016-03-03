/**
 * \file trie.cc
 * \brief Source file for the Trie0 class.
 *
 * authors : Dong (Kevin) Jin, Lenny Winterrowd
 */

#include "util/tries/trie0.h"
#include "net/ip_prefix.h"

#ifdef TRIE0_DEBUG
#include "net/forwardingtable.h"
#endif

namespace s3f {
namespace s3fnet {
namespace trie0 {

#ifdef TRIE0_DEBUG
#define TRIE0_DUMP(x) printf("TRIE0: "); x
#else
#define TRIE0_DUMP(x)
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
Trie0::Trie0() : nElements(0), last(this)
{
  root = new TrieNode(); assert(root);
}

/*
 * Recursively free each node.
 */
Trie0::~Trie0()
{
  deallocate(root);
  nElements = 0;
}

/*
 * Inserts a new (key, data) pair.
 */
TrieData* Trie0::insert(TRIEBITSTRING key, int nBits, 
		       TrieData* data, bool replace)
{
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
TrieData* Trie0::lookup(TRIEBITSTRING key) {
  TrieNode* cur = root;
  TrieData* pData = 0;
  while(cur) {
    if(cur->data) pData = cur->data;
    cur = cur->children[(key&0x80000000)?1:0];
    key <<= 1;
  }

  return pData;
}

/*
 * Removes the key from the trie, if it exists, and returns the data
 * corresponding to the removed key. If the key is not in the trie,
 * return 0.
 */
TrieData* Trie0::remove(TRIEBITSTRING key, int nBits)
{
  TrieNode* ptr;
  TrieData* olddata;
  
  ptr = search(key, nBits, 0, 0);
  if(!ptr) return 0;
  olddata = ptr->data;
  
  // BUGBUG: Here we just set the data to NULL and the TrieNode still
  // exists.  To conserve memory, we might want to delete the TrieNode
  // if it's a leaf.  But then we need to adjust the parent's pointer
  // and stuff.
  ptr->data = 0;
  nElements--;
  
  return olddata;
}

TrieData* Trie0::getDefault() {
	if(root) return root->data;
	return 0;
}

/*
 * Helper function for Insert and Lookup. This function will search
 * the trie for the key and if found, return a pointer to the
 * TrieNode. If not found, it will return NULL, and set failedNode to
 * the TrieNode where the search fails and failedbitpos to the bit
 * position that failed.
 */
TrieNode* Trie0::search(TRIEBITSTRING key, int len, 
		       TrieNode** failedNode, int* failedbitpos)
{
  TrieNode* cur, *lastcur;
  TrieNode* match = 0;
  int bitpos, curbit;

  // Go through the bits starting from the MSB and traverse
  // the tree. Remember the last match.
  for(bitpos = 0, lastcur = cur = root; 
      cur && bitpos < len && bitpos < TRIE_KEY_SIZE(key); 
      cur = cur->children[curbit], bitpos++) {
    if(cur->data) match = cur;
      
    lastcur = cur;
    curbit = GETBIT(bitpos, key);
  }

  if(cur && cur->data) match = cur;
  else if(failedNode && failedbitpos) {
    if(cur) {
      *failedbitpos = bitpos;
      *failedNode = cur;
    } else {
      *failedbitpos = bitpos - 1;
      *failedNode = lastcur;
    }
  }

  return match;
}


/*
 * Helper function for Insert. Creates nBits nodes for the key
 * starting at TrieNode theroot.
 */
void Trie0::create(TrieNode* theroot, TRIEBITSTRING key,
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
void Trie0::deallocate(TrieNode* theroot)
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

Trie0::iterator::iterator() : myTrie(0), currentNode(0), stateStack() 
{}

//Trie0::iterator::iterator(Trie0* t) : myTrie(t), currentNode(t->root), 
Trie0::iterator::iterator(Trie0* t) : myTrie(t), currentNode(0), 
				    stateStack() 
{}

Trie0::iterator::iterator(const Trie0::iterator& it) : 
  myTrie(it.myTrie), currentNode(it.currentNode), stateStack() 
{
  if (it.stateStack.size() > 0) { 
    // copy stack -- Need to modify the it.stateStack but will put it back. 
    // So, casting away const.
    S3FNET_STACK(NodeNext*)& ss = (S3FNET_STACK(NodeNext*)&)it.stateStack;
    copyStack(ss, true); // deep copy
  }
}

Trie0::iterator::~iterator() {
  // destroy state contents  
  while (stateStack.size() > 0) {
    iterator::NodeNext* nn = stateStack.top();
    stateStack.pop();
    delete nn;
  }
}

Trie0::iterator& Trie0::iterator::operator=(iterator& rhs) {
  myTrie = rhs.myTrie;
  currentNode = rhs.currentNode;
  if (rhs.stateStack.size() > 0) { 
    // copy stack
    copyStack(rhs.stateStack);
  }

  return *this;
}

void Trie0::iterator::copyStack(S3FNET_STACK(NodeNext*)& from, bool deepCopy) {
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

int Trie0::iterator::operator==(iterator& rhs) {
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

int Trie0::iterator::operator!=(iterator& rhs) {
  return (*this == rhs) ? 0 : 1;
}

Trie0::iterator& Trie0::iterator::operator++() {
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
    TRIE0_DUMP(std::cout << currentNode << " stopped (end)" << std::endl);
  } else {
    TRIE0_DUMP(std::cout << currentNode << " stopped (found)" << std::endl);
  }
  return *this;
}

void Trie0::iterator::nextNode() {
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
	TRIE0_DUMP(std::cout << currentNode << " push RIGHT, LEFT down, push LEFT (" << 
		  stateStack.size() << ") - "; currentNode->dump(); std::cout << std::endl);
      } else {
	TRIE0_DUMP(std::cout << currentNode << " push RIGHT (" << stateStack.size() << 
		  ") - "; currentNode->dump(); std::cout << std::endl);
      }
      }
      break;
    case RIGHT: {
      stateStack.push(new NodeNext(nn->node, UP)); // afterwards move up
      TrieNode* rightChild = currentNode->children[RIGHT];
      if (rightChild) { // has right child?
	TRIE0_DUMP(std::cout << currentNode << " push UP, RIGHT down, push LEFT (" << 
		  stateStack.size() << ") - "; currentNode->dump(); std::cout << std::endl);
	stateStack.push(new NodeNext(rightChild, LEFT)); // afterwards move left
      } else {
	TRIE0_DUMP(std::cout << currentNode << " push UP (" << stateStack.size() <<
		  ") - "; currentNode->dump(); std::cout << std::endl);
      }
      }
      break;
    case UP:
      // visit node
      keepMoving = false;
      TRIE0_DUMP(std::cout << currentNode << " visit (" << stateStack.size() <<
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
TrieNode* Trie0::iterator::operator->() {
  return currentNode;
}

// debug
void Trie0::iterator::print() {
  std::cout << "Trie0::iterator -- trie=" << myTrie << " currentNode=" << currentNode
       << " stateStack size=" << stateStack.size() << std::endl;
}

Trie0::iterator Trie0::begin() { 
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

#ifdef TRIE0_DEBUG
void TrieNode::dump()
{
  if(data) {
    RouteInfo* ri = (RouteInfo*)data;
    IPPrefix rdest = ri->destination;
    rdest.display();
  }
  if (children[Trie0::iterator::LEFT]) { std::cout << "L"; }
  if (children[Trie0::iterator::RIGHT]) { std::cout << "R"; }
}
#endif /*TRIE0_DEBUG*/

}; // namespace trie0
}; // namespace s3fnet
}; // namespace s3f

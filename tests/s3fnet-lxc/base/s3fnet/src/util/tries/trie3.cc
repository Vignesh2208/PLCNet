/**
 * \file trie3.cc
 * \brief Source file for the ip-optimized Trie3 class.
 *
 * authors : Dong (Kevin) Jin, Lenny Winterrowd
 */

#include "util/tries/trie3.h"
#include "net/ip_prefix.h"

#ifdef TRIE3_DEBUG
#include "net/forwardingtable.h"
#endif

namespace s3f {
namespace s3fnet {
namespace trie3 {

#ifdef TRIE3_DEBUG
#define TRIE3_DUMP(x) printf("TRIE3: "); x
#else
#define TRIE3_DUMP(x)
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
Trie3::Trie3() : nElements(0), last(this) {
	root = new TrieNode(0,0);
	assert(root);
}

/*
 * Recursively free each node.
 */
Trie3::~Trie3()
{
  deallocate(root);
  nElements = 0;
}

/*
 * Inserts a new (key, data) pair.
 */
TrieData* Trie3::insert(TRIEBITSTRING key, int nBits, TrieData* data, bool replace) {
	if(nBits > TRIE_KEY_SIZE(key)) { // the given mask is too long
		fprintf(stderr, "Invalid mask size %d\n",nBits);
		nBits = TRIE_KEY_SIZE(key);
	}

	TrieNode* cur = root; // Note: This Trie implementation guarantees that root is never null
	TrieNode* lastcur;
	TRIEBITSTRING origkey = key;
	int i;
	int index;
	// Note: This Trie implementation guarantees that any non-null, non-root node has data.
	for(i=0;cur;i++) {
		if(cur->data) { // TODO: Should be able to optimize this check out (see Note above)
			// if this node already contains the correct forwarding data
			if(data->equiv(cur->data)) {
				// want to keep the node that may be propagated down the farthest
				if(cur->maxDepth < nBits) {
					cur->maxDepth = nBits;
					cur->key = origkey;
				}
				delete data; // remove duplicate data (doesn't matter which since identical)
				return 0;
			// if inserting to this exact point but data is different, we may have a real conflict
			} else if(nBits == i) {
				/*
				// DEBUG BLOCK
				RouteInfo* currt = (RouteInfo*)cur->data;
				RouteInfo* newrt = (RouteInfo*)data;
				TRIE3_DUMP(printf("Potential conflict.\n");)
				TRIE3_DUMP(printf("MATCH(nBits == %d): currt.data %s newrt.data\n",i,currt==newrt ? "==":"!=");)
				TRIE3_DUMP(printf("inserted node: %s/%d, existing node: %s/%d\n",
					IPPrefix::ip2txt(newrt->destination.addr), newrt->destination.len,
					IPPrefix::ip2txt(currt->destination.addr), currt->destination.len);)
				TRIE3_DUMP(printf("cur->maxDepth = %d, currt->dest.len = %d\n",cur->maxDepth,currt->destination.len);)
				// END DEBUG BLOCK
				*/
				// if the data was stored here solely as a space-optimization
				if(cur->maxDepth > nBits) {
					// try to propagate it down the tree
					bool success = propagate(cur,nBits);
					//TRIE3_DUMP(printf("propagate %s\n",success?"succeeded":"failed");) // DEBUG
					if(success) {
						// collision was avoided, insert given data at cur
						cur->data = data;
						cur->maxDepth = nBits;
						cur->key = origkey;
						return 0;
					}
				}

				if(!replace) {
					return data;
				}

				TrieData* temp = cur->data;
				cur->data = data;
				cur->maxDepth = nBits;
				cur->key = origkey;
				return temp;
			}
		}

		// check next node
		key <<= 1;
		lastcur = cur;
		index = (key & TRIE_BIT_MASK)?1:0;
		cur = cur->children[index];
	}

	/*
	// DEBUG BLOCK
	RouteInfo* newrt = (RouteInfo*)data;
	TRIE3_DUMP(printf("INSERTED node (depth == %d): %s/%d\n", i-1, IPPrefix::ip2txt(newrt->destination.addr), newrt->destination.len);)
	// END DEBUG BLOCK
	*/

	// cur is null, so lastcur is where we need to insert
	TrieNode* ptr = lastcur->children[index] = new TrieNode(origkey,nBits);
	ptr->data = data;
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
TrieData* Trie3::lookup(TRIEBITSTRING key)
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
 * In this Trie implementation, remove should be used with reservation as for
 * nBits < TRIE_KEY_SIZE(key), multiple routes may actually be removed.
 */
TrieData* Trie3::remove(TRIEBITSTRING key, int nBits) {
 	if(nBits > TRIE_KEY_SIZE(key)) { // the given mask is too long
		fprintf(stderr, "Invalid mask size %d\n",nBits);
		nBits = TRIE_KEY_SIZE(key);
	}

	TrieNode* cur = root;
	TrieNode* lastcur;
	int i;
	int index;
	// move to target depth
	for(i=0;cur && i < nBits;i++) {
		key <<= 1;
		lastcur = cur;
		index = (key & TRIE_BIT_MASK)?1:0;
		cur = cur->children[index];
	}

	// if there is a match at the proper depth
	if(cur && cur->data) {
		TrieData* olddata = cur->data;
		cur->data = 0;
		// free this node if it is now a leaf
		if(cur->children[0] == NULL && cur->children[1] == NULL) {
			delete lastcur->children[index];
		}

		return olddata;
	}

	// no match was found at the given depth
	return 0;
}

TrieData* Trie3::getDefault() {
	if(root) return root->data;
	return 0;
}

/* Recursively attempts to move a duplicate node with unique data down the Trie.
 * TODO: May be possible to eliminate some unnecessary propagation failures. */
bool Trie3::propagate(TrieNode* node, int start_depth) {
	//printf("called propagate( %s/%d , %d )\n",IPPrefix::ip2txt(node->key),node->maxDepth,start_depth); // TRACE
	if(start_depth == TRIE_KEY_SIZE(key)) {
		return false;
	}

	TrieNode* cur = node;
	TrieNode* temp;
	int depth, curbit;
	for(depth=start_depth+1; depth <= node->maxDepth; depth++) {
		curbit = GETBIT(node->key,start_depth+1);
		temp = cur->children[curbit];
		if(temp == NULL) {
			//TRIE3_DUMP(printf("found null node @depth %d\n",depth);) // DEBUG
			temp = cur->children[curbit] = new TrieNode(node->key,node->maxDepth);
			temp->data = node->data;
			return true;
		}
		cur = temp;
	}

	// propagated as deep as possible - must try to chain-propagate the node at maxDepth
	if(cur->maxDepth > node->maxDepth) {
		bool success = propagate(cur,depth);
		if(success) {
			cur->data = node->data;
			cur->maxDepth = node->maxDepth;
			cur->key = node->key;
		}

		return success;
	}

	return false;
}

/*
 * Deallocates the trie recursively.
 */
void Trie3::deallocate(TrieNode* theroot)
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

Trie3::iterator::iterator() : myTrie(0), currentNode(0), stateStack() 
{}

//Trie3::iterator::iterator(Trie3* t) : myTrie(t), currentNode(t->root), 
Trie3::iterator::iterator(Trie3* t) : myTrie(t), currentNode(0), 
				    stateStack() 
{}

Trie3::iterator::iterator(const Trie3::iterator& it) : 
  myTrie(it.myTrie), currentNode(it.currentNode), stateStack() 
{
  if (it.stateStack.size() > 0) { 
    // copy stack -- Need to modify the it.stateStack but will put it back. 
    // So, casting away const.
    S3FNET_STACK(NodeNext*)& ss = (S3FNET_STACK(NodeNext*)&)it.stateStack;
    copyStack(ss, true); // deep copy
  }
}

Trie3::iterator::~iterator() {
  // destroy state contents  
  while (stateStack.size() > 0) {
    iterator::NodeNext* nn = stateStack.top();
    stateStack.pop();
    delete nn;
  }
}

Trie3::iterator& Trie3::iterator::operator=(iterator& rhs) {
  myTrie = rhs.myTrie;
  currentNode = rhs.currentNode;
  if (rhs.stateStack.size() > 0) { 
    // copy stack
    copyStack(rhs.stateStack);
  }

  return *this;
}

void Trie3::iterator::copyStack(S3FNET_STACK(NodeNext*)& from, bool deepCopy) {
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

int Trie3::iterator::operator==(iterator& rhs) {
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

int Trie3::iterator::operator!=(iterator& rhs) {
  return (*this == rhs) ? 0 : 1;
}

Trie3::iterator& Trie3::iterator::operator++() {
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
    TRIE3_DUMP(std::cout << currentNode << " stopped (end)" << std::endl);
  } else {
    TRIE3_DUMP(std::cout << currentNode << " stopped (found)" << std::endl);
  }
  return *this;
}

void Trie3::iterator::nextNode() {
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
	TRIE3_DUMP(std::cout << currentNode << " push RIGHT, LEFT down, push LEFT (" << 
		  stateStack.size() << ") - "; currentNode->dump(); std::cout << std::endl);
      } else {
	TRIE3_DUMP(std::cout << currentNode << " push RIGHT (" << stateStack.size() << 
		  ") - "; currentNode->dump(); std::cout << std::endl);
      }
      }
      break;
    case RIGHT: {
      stateStack.push(new NodeNext(nn->node, UP)); // afterwards move up
      TrieNode* rightChild = currentNode->children[RIGHT];
      if (rightChild) { // has right child?
	TRIE3_DUMP(std::cout << currentNode << " push UP, RIGHT down, push LEFT (" << 
		  stateStack.size() << ") - "; currentNode->dump(); std::cout << std::endl);
	stateStack.push(new NodeNext(rightChild, LEFT)); // afterwards move left
      } else {
	TRIE3_DUMP(std::cout << currentNode << " push UP (" << stateStack.size() <<
		  ") - "; currentNode->dump(); std::cout << std::endl);
      }
      }
      break;
    case UP:
      // visit node
      keepMoving = false;
      TRIE3_DUMP(std::cout << currentNode << " visit (" << stateStack.size() <<
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
TrieNode* Trie3::iterator::operator->() {
  return currentNode;
}

// debug
void Trie3::iterator::print() {
  std::cout << "Trie3::iterator -- trie=" << myTrie << " currentNode=" << currentNode
       << " stateStack size=" << stateStack.size() << std::endl;
}

Trie3::iterator Trie3::begin() { 
  iterator first(this); // create iterator
  ++first; // find first element
  return first; // return by copy
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

/*
 * Constructor of TrieNode.
 */
TrieNode::TrieNode(TRIEBITSTRING node_key, int max_depth) : data(0)
{
  key = node_key;
  maxDepth = max_depth;
  memset(children, 0, sizeof(TrieNode*)*TRIE_KEY_SPAN);
}

TrieNode::~TrieNode()
{
  if(data) delete data;
}

#ifdef TRIE3_DEBUG
void TrieNode::dump()
{
  if(data) {
    RouteInfo* ri = (RouteInfo*)data;
    IPPrefix rdest = ri->destination;
    rdest.display();
  }
  if (children[Trie3::iterator::LEFT]) { std::cout << "L"; }
  if (children[Trie3::iterator::RIGHT]) { std::cout << "R"; }
}
#endif /*TRIE3_DEBUG*/

}; // namespace trie3
}; // namespace s3fnet
}; // namespace s3f

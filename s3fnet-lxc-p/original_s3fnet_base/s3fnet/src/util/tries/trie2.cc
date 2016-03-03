/**
 * \file trie2.cc
 * \brief Source file for the ip-optimized Trie2 class.
 *
 * authors : Dong (Kevin) Jin, Lenny Winterrowd
 */

#include "util/tries/trie2.h"
#include "net/ip_prefix.h"

#ifdef TRIE2_DEBUG
#include "net/forwardingtable.h"
#endif

namespace s3f {
namespace s3fnet {
namespace trie2 {

#ifdef TRIE2_DEBUG
#define TRIE2_DUMP(x) printf("TRIE2: "); x
#else
#define TRIE2_DUMP(x)
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
Trie2::Trie2() : nElements(0), last(this) {
	root = new TrieNode();
	assert(root);
}

/*
 * Recursively free each node.
 */
Trie2::~Trie2()
{
  deallocate(root);
  nElements = 0;
}

/*
 * Inserts a new (key, data) pair.
 */
TrieData* Trie2::insert(TRIEBITSTRING key, int nBits, TrieData* data, bool replace) {
	if(nBits > TRIE_KEY_SIZE(key)) { // the given mask is too long
		fprintf(stderr, "Invalid mask size %d\n",nBits);
		nBits = TRIE_KEY_SIZE(key);
	}

	#ifdef TRIE2_DEBUG
	RouteInfo* data_ri = (RouteInfo*)data;
	printf("new route: %s/%d\n",IPPrefix::ip2txt(data_ri->destination.addr),data_ri->destination.len);
	#endif

	TrieNode* cur = root;
	TrieNode* lastcur;
	int i;
	int index;
	for(i=0;cur;i++) {
		// if we find a match
		if(cur->data) {
			// TODO: Tests indicate that true duplicates are still not always detected
			// if this node already contains the correct forwarding data
			if(data->equiv(cur->data)) {
				delete data; // remove duplicate data (doesn't matter which since identical)
				return 0;
			// if inserting to this exact point but data is different, we have a real conflict
			} else if(nBits == i) {
				#ifdef TRIE2_DEBUG
				printf("CONFLICT!!!\n");
				RouteInfo* cur_ri = (RouteInfo*)cur->data;
				printf("old route: %s/%d\n",IPPrefix::ip2txt(cur_ri->destination.addr),cur_ri->destination.len);
				printf("old next_hop: %s\n",IPPrefix::ip2txt(cur_ri->next_hop));
				printf("new next_hop: %s\n",IPPrefix::ip2txt(data_ri->next_hop));
				#endif
				if(!replace) {
					return data;
				}

				TrieData* temp = cur->data;
				cur->data = data;
				return temp;
			}
		}

		// check next node
		key <<= 1;
		lastcur = cur;
		index = (key & TRIE_BIT_MASK)?1:0;
		cur = cur->children[index];
	}

	#ifdef TRIE2_DEBUG
	printf("Stored at depth %d\n",i); // DEBUG
	#endif

	// cur is null, so lastcur is where we need to insert
	lastcur->children[index] = new TrieNode();
	lastcur->children[index]->data = data;
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
TrieData* Trie2::lookup(TRIEBITSTRING key)
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
TrieData* Trie2::remove(TRIEBITSTRING key, int nBits) {
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

TrieData* Trie2::getDefault() {
	if(root) return root->data;
	return 0;
}

/*
 * Deallocates the trie recursively.
 */
void Trie2::deallocate(TrieNode* theroot)
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

Trie2::iterator::iterator() : myTrie(0), currentNode(0), stateStack() 
{}

//Trie2::iterator::iterator(Trie2* t) : myTrie(t), currentNode(t->root), 
Trie2::iterator::iterator(Trie2* t) : myTrie(t), currentNode(0), 
				    stateStack() 
{}

Trie2::iterator::iterator(const Trie2::iterator& it) : 
  myTrie(it.myTrie), currentNode(it.currentNode), stateStack() 
{
  if (it.stateStack.size() > 0) { 
    // copy stack -- Need to modify the it.stateStack but will put it back. 
    // So, casting away const.
    S3FNET_STACK(NodeNext*)& ss = (S3FNET_STACK(NodeNext*)&)it.stateStack;
    copyStack(ss, true); // deep copy
  }
}

Trie2::iterator::~iterator() {
  // destroy state contents  
  while (stateStack.size() > 0) {
    iterator::NodeNext* nn = stateStack.top();
    stateStack.pop();
    delete nn;
  }
}

Trie2::iterator& Trie2::iterator::operator=(iterator& rhs) {
  myTrie = rhs.myTrie;
  currentNode = rhs.currentNode;
  if (rhs.stateStack.size() > 0) { 
    // copy stack
    copyStack(rhs.stateStack);
  }

  return *this;
}

void Trie2::iterator::copyStack(S3FNET_STACK(NodeNext*)& from, bool deepCopy) {
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

int Trie2::iterator::operator==(iterator& rhs) {
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

int Trie2::iterator::operator!=(iterator& rhs) {
  return (*this == rhs) ? 0 : 1;
}

Trie2::iterator& Trie2::iterator::operator++() {
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
    TRIE2_DUMP(std::cout << currentNode << " stopped (end)" << std::endl);
  } else {
    TRIE2_DUMP(std::cout << currentNode << " stopped (found)" << std::endl);
  }
  return *this;
}

void Trie2::iterator::nextNode() {
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
	TRIE2_DUMP(std::cout << currentNode << " push RIGHT, LEFT down, push LEFT (" << 
		  stateStack.size() << ") - "; currentNode->dump(); std::cout << std::endl);
      } else {
	TRIE2_DUMP(std::cout << currentNode << " push RIGHT (" << stateStack.size() << 
		  ") - "; currentNode->dump(); std::cout << std::endl);
      }
      }
      break;
    case RIGHT: {
      stateStack.push(new NodeNext(nn->node, UP)); // afterwards move up
      TrieNode* rightChild = currentNode->children[RIGHT];
      if (rightChild) { // has right child?
	TRIE2_DUMP(std::cout << currentNode << " push UP, RIGHT down, push LEFT (" << 
		  stateStack.size() << ") - "; currentNode->dump(); std::cout << std::endl);
	stateStack.push(new NodeNext(rightChild, LEFT)); // afterwards move left
      } else {
	TRIE2_DUMP(std::cout << currentNode << " push UP (" << stateStack.size() <<
		  ") - "; currentNode->dump(); std::cout << std::endl);
      }
      }
      break;
    case UP:
      // visit node
      keepMoving = false;
      TRIE2_DUMP(std::cout << currentNode << " visit (" << stateStack.size() <<
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
TrieNode* Trie2::iterator::operator->() {
  return currentNode;
}

// debug
void Trie2::iterator::print() {
  std::cout << "Trie2::iterator -- trie=" << myTrie << " currentNode=" << currentNode
       << " stateStack size=" << stateStack.size() << std::endl;
}

Trie2::iterator Trie2::begin() { 
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

#ifdef TRIE2_DEBUG
void TrieNode::dump()
{
  if(data) {
    RouteInfo* ri = (RouteInfo*)data;
    IPPrefix rdest = ri->destination;
    rdest.display();
  }
  if (children[Trie2::iterator::LEFT]) { std::cout << "L"; }
  if (children[Trie2::iterator::RIGHT]) { std::cout << "R"; }
}
#endif /*TRIE2_DEBUG*/

}; // namespace trie2
}; // namespace s3fnet
}; // namespace s3f

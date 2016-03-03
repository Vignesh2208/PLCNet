/**
 * \file triedata_array1.cc
 * \brief Source file for a mapping, overhead-measuring TrieDataArray (no coalescing).
 *
 * authors : Lenny Winterrowd
 */

#include <climits>
#include "util/tries/triedata_array1.h"

namespace s3f {
namespace s3fnet {
namespace tda1 {

TrieDataArray1::TrieDataArray1(unsigned int size) : mSize(size), mCount(0), mInsertPoint(0) {
	mData = new TrieData*[size];
	assert(mData);
}

TrieDataArray1::~TrieDataArray1() {
	if(mData) {
		int i;
		for(i=0;i<mCount;i++) {
			if(mData[i]) {
				delete mData[i];
			}
		}
		delete[] mData;
	}
}

unsigned int TrieDataArray1::addElement(TrieData* data) {
	if(mInsertPoint != NULL) { // pop from our stack of insertion points
		unsigned int index = mInsertPoint->index;
		InsertList* temp = mInsertPoint;
		mInsertPoint = mInsertPoint->next;
		delete temp;
		mData[index] = data;
		return index;
	} else if(mSize == mCount) { // arraylist full - must realloc
		extend();
	}

	unsigned int index = mCount++;
	mData[index] = data;
	return index;
}

TrieData* TrieDataArray1::getElement(unsigned int index) {
	return mData[index];
}

TrieData* TrieDataArray1::removeElement(unsigned int index, bool get) {
	TrieData* ret;
	if(get) {
		ret = mData[index];
	} else {
		delete mData[index];
		ret = 0;
	}

	if(index == mCount-1) {
		mCount--;
	} else {
		InsertList* temp = new InsertList(index,mInsertPoint);
		mInsertPoint = temp;
		mData[index] = 0;
	}

	return ret;
}

void TrieDataArray1::extend() {
	mSize = (unsigned int)(REALLOC_FACTOR*mSize) + 1;
	TrieData** newData = new TrieData*[mSize];
	int i;
	for(i=0;i<mCount;i++) {
		newData[i] = mData[i];
	}
	delete[] mData;
	mData = newData;
}

}; // namespace tda1
}; // namespace s3fnet
}; // namespace s3f

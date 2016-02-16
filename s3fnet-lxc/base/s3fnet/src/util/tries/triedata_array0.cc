/**
 * \file triedata_array0.cc
 * \brief Source file for an overhead-measuring TrieDataArray (no coalescing).
 *
 * authors : Lenny Winterrowd
 */

#include <climits>
#include <cstring>
#include "util/tries/triedata_array0.h"

namespace s3f {
namespace s3fnet {
namespace tda0 {

TrieDataArray0::TrieDataArray0(unsigned int size) : mSize(size), mData(0), mInsertPoint(0) {
	// allocates on addition of first item since we must know the item size
}

TrieDataArray0::~TrieDataArray0() {
	if(mData) {
		delete[] mData;
	}
}

unsigned int TrieDataArray0::addElement(TrieData* data) {
	if(data->size() == 0) {
		return UINT_MAX;
	} else if(!mData) {
		mDataSize = data->size();
		mData = new char[mSize*mDataSize];
		assert(mData);
		memcpy((void*)mData,(void*)data,mDataSize);
		delete data;
		mCount = 1;
		return 0;
	} else if(mInsertPoint != NULL) { // pop from our stack of insertion points
		unsigned int index = mInsertPoint->index;
		InsertList* temp = mInsertPoint;
		mInsertPoint = mInsertPoint->next;
		delete temp;
		void* dest = (void*)&mData[index*mDataSize];
		memcpy(dest,(void*)data,mDataSize);
		delete data;
		return index;
	} else if(mSize == mCount) { // arraylist full - must realloc
		extend();
	}

	unsigned int index = mCount++;
	void* dest = (void*)&mData[index*mDataSize];
	memcpy(dest,(void*)data,mDataSize);
	delete data;
	return index;
}

TrieData* TrieDataArray0::getElement(unsigned int index) {
	return (TrieData*)&mData[index*mDataSize];
}

TrieData* TrieDataArray0::removeElement(unsigned int index, bool get) {
	TrieData* ret;
	if(get) {
		void* dest = (void*)new char[mDataSize];
		void* src = (void*)&mData[index*mDataSize];
		memcpy(dest,src,mDataSize);
		ret = (TrieData*)dest;
	} else {
		ret = 0;
	}

	if(index == mCount-1) {
		mCount--;
	} else {
		InsertList* temp = new InsertList(index,mInsertPoint);
		mInsertPoint = temp;
	}

	return ret;
}

void TrieDataArray0::extend() {
	mSize = (unsigned int)(REALLOC_FACTOR*mSize) + 1;
	char* newData = new char[mSize*mDataSize];
	memcpy((void*)newData,mData,mSize*mDataSize);
	delete[] mData;
	mData = newData;
}

}; // namespace tda0
}; // namespace s3fnet
}; // namespace s3f

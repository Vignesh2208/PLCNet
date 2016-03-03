/**
 * \file triedata_array2.cc
 * \brief Source file for a mapping, overhead-measuring TrieDataArray (no coalescing).
 *
 * authors : Lenny Winterrowd
 */

#include <climits>
#include "util/tries/triedata_array2.h"

namespace s3f {
namespace s3fnet {
namespace tda2 {

TrieDataArray2::TrieDataArray2(unsigned int size) : mSize(size), mCount(0), mInsertPoint(0) {
	mData = new TDStruct[size];
	assert(mData);
}

TrieDataArray2::~TrieDataArray2() {
	if(mData) {
		delete[] mData;
	}
}

unsigned int TrieDataArray2::addElement(TrieData* data) {
	int i;
	TDStruct td;
	for(i=0;i<mCount;i++) {
		td = mData[i];
		if(td.refCount > 0 && data->equiv(td.data)) {
			delete data;
			td.refCount++;
			return i;
		}
	}
	td.data = 0; // prevent bad free (and possible SIGSEGV)

	unsigned int index;
	if(mInsertPoint != NULL) { // pop from our stack of insertion points
		index = mInsertPoint->index;
		InsertList* temp = mInsertPoint;
		mInsertPoint = mInsertPoint->next;
		delete temp;
	} else {
		if(mSize == mCount) { // arraylist full - must realloc
			extend();
		}
		index = mCount++;
	}

	mData[index].refCount = 1;
	mData[index].data = data;
	return index;
}

TrieData* TrieDataArray2::getElement(unsigned int index) {
	return mData[index].data;
}

/* // This is equivalent to the other removeElement() but has some redundant code
TrieData* TrieDataArray2::removeElement(unsigned int index, bool get) {
	TrieData* ret;
	unsigned int rc = mData[index].refCount--;
	if(get) {
		if(rc == 0) {
			ret = mData[index].data;
			if(index == mCount-1) {
				mCount--;
			} else {
				InsertList* temp = new InsertList(index,mInsertPoint);
				mInsertPoint = temp;
				mData[index].data = 0;
			}
		} else { // must return a copy
			ret = mData[index].data->clone();
		}
	} else {
		if(rc == 0) {
			delete mData[index].data;
			if(index == mCount-1) {
				mCount--;
			} else {
				InsertList* temp = new InsertList(index,mInsertPoint);
				mInsertPoint = temp;
				mData[index].data = 0;
			}
		}
		ret = 0;
	}

	return ret;
}
*/

TrieData* TrieDataArray2::removeElement(unsigned int index, bool get) {
	TrieData* ret;
	unsigned int rc = mData[index].refCount--;
	if(rc == 0) {
		if(get) {
			ret = mData[index].data;
		} else {
			delete mData[index].data;
			ret = 0;
		}

		if(index == mCount-1) {
			mCount--;
		} else {
			InsertList* temp = new InsertList(index,mInsertPoint);
			mInsertPoint = temp;
			mData[index].data = 0;
		}
	} else {
		if(get) { // must return a copy
			ret = mData[index].data->clone();
		} else {
			ret = 0;
		}
	}

	return ret;
}

void TrieDataArray2::extend() {
	mSize = (unsigned int)(REALLOC_FACTOR*mSize) + 1;
	TDStruct* newData = new TDStruct[mSize];
	int i;
	for(i=0;i<mCount;i++) {
		newData[i] = mData[i];
		mData[i].data = 0;
	}
	delete[] mData;
	mData = newData;
}

}; // namespace tda2
}; // namespace s3fnet
}; // namespace s3f

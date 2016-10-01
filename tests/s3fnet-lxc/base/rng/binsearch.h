/**
 * \file binsearch.h
 *
 * \brief Header file for BinarySearch class.
 *
 */
#ifndef BINARY_SEARCH
#define BINARY_SEARCH

/**
 * \brief Binary Search
 */
class BinarySearch {
 public:
   BinarySearch(int lower, int upper) : _lower(lower),
   	_upper(upper) {
	_mid = (_upper + _lower)/2;	
   }

   bool found()  { return (_lower == _upper); }

   int  search() { return _mid; }

   int refine(bool moveleft) { 

       if( _upper - _lower > 1 ) {
	  if(moveleft) _upper = _mid;
	  else _lower = _mid;
	  _mid  = (_upper + _lower)/2;
	} else {
          // having _upper - _lower == 1 is a terminal case 
          // If lessthan is true
	  // then the answer is _lower, otherwise the answer is _upper
	  if( moveleft ) _mid = _upper = _lower;
	  else           _mid = _lower = _upper;
	}
	return _mid;
   }

   private:
     int _lower, _upper, _mid;
};
#endif
